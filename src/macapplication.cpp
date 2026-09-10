#include "macapplication.h"
#include "engine.h"
#include "documentsession.h"
#include "documentrecovery.h"
#include "viewportstate.h"
#include "windowplacement.h"
#include "recentdocuments.h"
#include <QQmlComponent>
#include <QQmlContext>
#include <QLocalSocket>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QTextDocument>
#include <QTimer>
#include <QGuiApplication>

void installMacFileMenu(QWindow *, std::function<void(QString,QString)>, std::function<QVariantList()>);
void installMacHelpMenu(QWindow *);
void installMacToolbar(QWindow *);
void installMacWindowMenu(QWindow *, std::function<QVariantList()>, std::function<void(qint64)>);
void installMacReopenHandler(QWindow *, std::function<void()>);
void showMacCloseConfirmation(QWindow *, const QString &, std::function<void(int)>);
void showMacSavePanel(QWindow *, const QString &, std::function<void(QString)>);
void showMacParentFolderMenu(QWindow *, const QString &, double, double);

class MacDocumentWindow : public QObject {
public:
    MacApplication *application;
    qint64 id;
    Engine engine{nullptr,Engine::InitialContent::Blank};
    std::unique_ptr<DocumentSession> session;
    QQmlContext context;
    QQuickWindow *window=nullptr;
    QSettings viewportSettings{"Mindarchy","Mindarchy"};
    std::unique_ptr<ViewportState> viewport;
    std::unique_ptr<DocumentRecovery> recovery;
    WindowPlacement *placement=nullptr;
    QTimer poll;
    bool saveSheet=false, closeSheet=false;
    QString placementFile;
    MacDocumentWindow(MacApplication *app,qint64 identity) : application(app),id(identity),context(app->m_qml.rootContext()) {}
    ~MacDocumentWindow() override {
        poll.stop(); recovery.reset(); viewport.reset();
        delete window;
    }
    void savePlacement() {
        if(!placement) return;
        placement->save();
        QSettings individual(placementFile,QSettings::IniFormat);
        std::unique_ptr<QSettings> defaults(AppIdentity::windowSettings());
        for(const auto &key:individual.allKeys()) defaults->setValue(key,individual.value(key));
        defaults->sync();
    }
    bool load(const QString &path,const QString &theme) {
        const bool snapshot=path.endsWith(".recovery");
        if(snapshot && (QFileInfo(path).absolutePath()!=QFileInfo(application->m_directory).absoluteFilePath() || QUuid(QFileInfo(path).completeBaseName()).isNull())) return false;
        QVariantMap restored;
        engine.setRecentDirectory(application->m_directory);
        if(!path.isEmpty() && !(snapshot?engine.openRecovery(path,&restored):engine.open(path))) return false;
        if(!theme.isEmpty()) engine.setThemeId(theme);
        session=std::make_unique<DocumentSession>(application->m_directory,snapshot?path:QString());
        if(!session->locked()) return false;
        session->setDocument(engine.documentPath());
        context.setContextProperty("engine",&engine);
        context.setContextProperty("deferWindowShow",true);
        context.setContextProperty("nativeCloseAvailable",true);
        QQmlComponent component(&application->m_qml,QUrl("qrc:/qml/Main.qml"));
        window=qobject_cast<QQuickWindow *>(component.create(&context));
        if(!window) { application->m_error=component.errorString(); return false; }
        window->setProperty("macDocumentWindowId",id);
        auto *canvas=window->findChild<MindCanvas *>("mindCanvas");
        viewport=std::make_unique<ViewportState>(canvas,&engine,&viewportSettings);
        recovery=std::make_unique<DocumentRecovery>(&engine,window,canvas,session->recoveryPath(),restored);
        placementFile=session->recoveryPath()+".window.ini";
        if(!QFileInfo::exists(placementFile)) {
            std::unique_ptr<QSettings> defaults(AppIdentity::windowSettings());
            QSettings individual(placementFile,QSettings::IniFormat);
            for(const auto &key:defaults->allKeys()) individual.setValue(key,defaults->value(key));
            individual.sync();
        }
        placement=new WindowPlacement(window,placementFile);
        installMacToolbar(window);
        connect(window,&QWindow::activeChanged,this,[this] { if(window->isActive()) application->m_active=window; });
        connect(window,&QWindow::visibleChanged,this,[this](bool visible) {
            if(!visible && window->property("allowClose").toBool()) {
                savePlacement(); viewport->flush(); poll.stop();
                application->remove(this);
            }
        });
        connect(&engine,&Engine::changed,this,[this] { session->setDocument(engine.documentPath()); });
        connect(&engine,&Engine::newDocumentRequested,this,[this] { application->open(); });
        connect(&engine,&Engine::openDocumentRequested,this,[this](QString file) { application->open(file); });
        connect(&engine,&Engine::windowCloseApproved,this,[this](bool) { recovery->remove(); session->forgetDocument(); });
        connect(&engine,&Engine::quitRequested,this,[this] { application->requestQuit(); });
        engine.setWindowNavigation([this] { return application->windows(); },[this](qint64 identity) { application->activate(identity); });
        connect(&engine,&Engine::nativeFolderMenuRequested,this,[this](double x,double y) {
            if(!engine.documentPath().isEmpty()) showMacParentFolderMenu(window,engine.documentPath(),x,y);
        });
        connect(&engine,&Engine::nativeSaveRequested,this,[this] {
            if(saveSheet) return; saveSheet=true;
            QTextDocument title; title.setHtml(engine.nodes().value(1).text);
            QString name=title.toPlainText().simplified().left(100); name.replace('/','-'); name.replace(':','-');
            if(name.isEmpty()) name="Untitled";
            QPointer<MacDocumentWindow> guard(this);
            showMacSavePanel(window,name,[guard](QString path) { if(guard) QTimer::singleShot(0,guard,[guard,path] {
                guard->saveSheet=false;
                QMetaObject::invokeMethod(guard->window,"finishSaveDialog",Q_ARG(QVariant,path));
            }); });
        });
        connect(&engine,&Engine::nativeCloseRequested,this,[this] {
            if(closeSheet) return; closeSheet=true;
            QTextDocument title; title.setHtml(engine.nodes().value(1).text);
            QString name=engine.documentPath().isEmpty()?title.toPlainText():QFileInfo(engine.documentPath()).fileName();
            QPointer<MacDocumentWindow> guard(this);
            showMacCloseConfirmation(window,name,[guard](int choice) { if(guard) QTimer::singleShot(0,guard,[guard,choice] {
                guard->closeSheet=false;
                QMetaObject::invokeMethod(guard->window,choice==1?"saveBeforeClosing":choice==2?"approveClose":"cancelClose");
            }); });
        });
        connect(&poll,&QTimer::timeout,this,[this] {
            switch(session->pollQuit()) {
            case DocumentSession::QuitAction::Confirm: session->voteToQuit(recovery->prepareQuit()); break;
            case DocumentSession::QuitAction::Cancel:
                application->m_quitting=false; QMetaObject::invokeMethod(window,"abortSessionQuit"); break;
            case DocumentSession::QuitAction::Close: QMetaObject::invokeMethod(window,"completeSessionQuit"); break;
            default: break;
            }
        });
        poll.start(150);
        window->setVisible(true);
        if(!theme.isEmpty()) window->setProperty("inspectorVisible",true);
        if(path.isEmpty()) QTimer::singleShot(0,this,[canvas] { canvas->fit(); canvas->beginEdit(1); });
        return true;
    }
};

MacApplication::MacApplication(QString directory,QObject *parent) : QObject(parent),m_directory(std::move(directory)) {
    QGuiApplication::setQuitOnLastWindowClosed(false);
    connect(&m_qml,&QQmlEngine::warnings,this,[](const QList<QQmlError> &errors) {
        for(const auto &e:errors) fprintf(stderr,"QML: %s\n",qPrintable(e.toString()));
    });
}
MacApplication::~MacApplication() {
    for(auto *document:m_documents) delete document;
}
MacApplication::Launch MacApplication::startOrForward(const QStringList &files,bool fresh) {
    QDir().mkpath(m_directory);
    const QString name="mindarchy-"+QString::fromLatin1(QCryptographicHash::hash(m_directory.toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
    m_lock=std::make_unique<QLockFile>(m_directory+"/application.lock");
    m_lock->setStaleLockTime(0);
    if(!m_lock->tryLock()) {
        QLocalSocket socket; socket.connectToServer(name);
        if(!socket.waitForConnected(3000)) { m_error="Could not contact the running Mindarchy application"; return Launch::Failed; }
        socket.write(QJsonDocument(QJsonObject{{"files",QJsonArray::fromStringList(files)},{"new",fresh}}).toJson(QJsonDocument::Compact)+'\n');
        socket.waitForBytesWritten(3000);
        if(!socket.waitForReadyRead(3000) || socket.readAll()!="ok\n") { m_error="The running application did not accept the request"; return Launch::Failed; }
        return Launch::Forwarded;
    }
    QLocalServer::removeServer(name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if(!m_server.listen(name)) { m_error=m_server.errorString(); return Launch::Failed; }
    connect(&m_server,&QLocalServer::newConnection,this,[this] {
        while(auto *socket=m_server.nextPendingConnection()) {
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QLocalSocket::readyRead,this,[this,socket] {
                if(socket->bytesAvailable()>1024*1024) { socket->disconnectFromServer(); return; }
                if(!socket->canReadLine()) return;
                const auto request=QJsonDocument::fromJson(socket->readLine()).object();
                if(m_quitting) { socket->write("busy\n"); socket->disconnectFromServer(); return; }
                const auto files=request["files"].toArray();
                for(const auto &file:files) open(file.toString());
                if(request["new"].toBool()) open();
                else if(files.isEmpty()) reopen();
                socket->write("ok\n"); socket->disconnectFromServer();
            });
        }
    });
    return Launch::Primary;
}
void MacApplication::start(const QStringList &files,bool fresh,const QString &theme) {
    installMacFileMenu(&m_menuOwner,[this](QString action,QString path) { command(action,path); },[this] { return RecentDocuments(m_directory).list(); });
    installMacHelpMenu(&m_menuOwner);
    installMacWindowMenu(&m_menuOwner,[this] { return windows(); },[this](qint64 id) { activate(id); });
    installMacReopenHandler(&m_menuOwner,[this] { reopen(); });
    const auto paths=files.isEmpty()&&!fresh?DocumentSession::restorePaths(m_directory,true):files;
    for(const auto &path:paths) open(path,theme);
    if(m_documents.empty()) open({},theme);
}
QQuickWindow *MacApplication::open(const QString &path,const QString &theme) {
    if(m_quitting) return nullptr;
    if(!path.isEmpty()) for(auto *document:m_documents) {
        if(QFileInfo(document->engine.documentPath()).canonicalFilePath()==QFileInfo(path).canonicalFilePath() && !document->engine.documentPath().isEmpty() && QFileInfo(path).exists()) {
            activate(document->id); return document->window;
        }
    }
    auto *document=new MacDocumentWindow(this,m_nextId++);
    if(!document->load(path,theme)) {
        if(m_error.isEmpty()) m_error=document->engine.error();
        fprintf(stderr,"Could not open document: %s\n",qPrintable(m_error)); delete document; return nullptr;
    }
    m_documents.push_back(document); activate(document->id); return document->window;
}
void MacApplication::remove(MacDocumentWindow *document) {
    std::erase(m_documents,document);
    if(m_active==document->window) m_active=nullptr;
    document->deleteLater();
    if(m_quitting && m_documents.empty()) QTimer::singleShot(0,qApp,[] { QCoreApplication::exit(0); });
}
bool MacApplication::requestQuit() {
    if(m_documents.empty()) return false;
    if(m_quitting) return true;
    // Do not interrupt an in-flight native save/discard sheet.
    for(auto *document:m_documents) if(document->saveSheet || document->closeSheet) return true;
    m_quitting=true; m_documents.front()->session->beginQuit(); return true;
}
QQuickWindow *MacApplication::activeWindow() const {
    if(m_active && m_active->isVisible()) return m_active;
    return m_documents.empty()?nullptr:m_documents.back()->window;
}
Engine *MacApplication::activeDocument() const {
    for(auto *document:m_documents) if(document->window==activeWindow()) return &document->engine;
    return nullptr;
}
QVariantList MacApplication::windows() const {
    QVariantList result;
    for(auto *document:m_documents) result.append(QVariantMap{{"windowId",document->id},{"pid",document->id},{"title",document->window->title()},{"current",document->window==activeWindow()}});
    return result;
}
void MacApplication::activate(qint64 id) {
    for(auto *document:m_documents) if(document->id==id) {
        auto *window=document->window;
        if(window->windowState()==Qt::WindowMinimized) window->showNormal();
        window->raise(); window->requestActivate(); m_active=window; return;
    }
}
void MacApplication::reopen() { if(m_documents.empty()) open(); else for(auto *d:m_documents) if(d->window==activeWindow()) { activate(d->id); break; } }
void MacApplication::command(const QString &action,const QString &path) {
    if(action=="new") { open(); return; }
    if(action=="recent") { open(path); return; }
    if(action=="clear") { RecentDocuments(m_directory).clear(); return; }
    auto *window=activeWindow();
    if(!window && action=="open") window=open();
    if(!window) return;
    if(action=="open") QMetaObject::invokeMethod(window,"openDocumentMenu");
    else if(action=="save") QMetaObject::invokeMethod(window,"saveDocument",Q_ARG(QVariant,false));
    else if(action=="close") QMetaObject::invokeMethod(window,"requestClose",Q_ARG(QVariant,true),Q_ARG(QVariant,false));
}
