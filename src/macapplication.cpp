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
#include <QQuickItem>
#include <QKeyEvent>
#include "tabshortcuts.h"

#ifdef Q_OS_MACOS
void installMacFileMenu(QWindow *, std::function<void(QString,QString)>, std::function<QVariantList()>);
void installMacHelpMenu(QWindow *);
void installMacToolbar(QWindow *);
void installMacWindowMenu(QWindow *, std::function<QVariantList()>, std::function<void(qint64)>);
void installMacReopenHandler(QWindow *, std::function<void()>);
void showMacCloseConfirmation(QWindow *, const QString &, std::function<void(int)>);
void showMacSavePanel(QWindow *, const QString &, std::function<void(QString)>);
void showMacParentFolderMenu(QWindow *, const QString &, double, double);

#else
#include "windowsdialogs.h"
#include "windowmenubar.h"
#endif

namespace {
void installGroupPlacement(QQuickWindow *host,const QString &source,const QString &directory) {
    const auto file=directory+"/window-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".ini";
    QFile::copy(source,file);
    auto *placement=new WindowPlacement(host,file);
    host->setProperty("placementOwner",QVariant::fromValue<QObject *>(placement));
    host->setProperty("placementFile",file);
    QObject::connect(host,&QObject::destroyed,[file] { QFile::remove(file); });
}
}

class MacDocumentWindow : public QObject {
public:
    MacApplication *application;
    qint64 id;
    QQuickItem *workspace=nullptr;
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
    qint64 closeReturnId=0;
    QString placementFile;
    MacDocumentWindow(MacApplication *app,qint64 identity) : application(app),id(identity),context(app->m_qml.rootContext()) {}
    ~MacDocumentWindow() override {
        poll.stop(); recovery.reset(); viewport.reset();
        delete workspace;
    }
    void savePlacement() {
        auto *owner=static_cast<WindowPlacement *>(window->property("placementOwner").value<QObject *>());
        if(!owner) return;
        owner->save();
        QSettings groupSettings(window->property("placementFile").toString(),QSettings::IniFormat);
        QSettings individual(placementFile,QSettings::IniFormat);
        std::unique_ptr<QSettings> defaults(AppIdentity::windowSettings());
        for(const auto &key:groupSettings.allKeys()) {
            individual.setValue(key,groupSettings.value(key));
            defaults->setValue(key,groupSettings.value(key));
        }
        individual.sync(); defaults->sync();
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
        context.setContextProperty("sharedWindowManaged",true);
        context.setContextProperty("nativeCloseAvailable",QGuiApplication::platformName()=="cocoa" || QGuiApplication::platformName()=="windows");
        window=application->m_openTarget ? application->m_openTarget.data() : application->createHost(&engine);
        if(!window) return false;
        QQmlComponent component(&application->m_qml,QUrl("qrc:/qml/DocumentWorkspace.qml"));
        workspace=qobject_cast<QQuickItem *>(component.createWithInitialProperties({{"hostWindow",QVariant::fromValue(window)},{"parent",QVariant::fromValue(window->contentItem())}},&context));
        if(!workspace) { application->m_error=component.errorString(); return false; }
        workspace->setParent(this);
        workspace->setParentItem(window->contentItem());
        if(auto *content=window->contentItem()) workspace->setSize(content->size());
        workspace->setProperty("documentTabId",id);
        workspace->setVisible(false);
        workspace->setEnabled(false);
        QObject::connect(workspace,SIGNAL(closeApproved()),application,SLOT(workspaceClosed()));
        QObject::connect(workspace,SIGNAL(closeCancelled()),application,SLOT(workspaceCloseCancelled()));
        auto *canvas=workspace->findChild<MindCanvas *>("mindCanvas");
        viewport=std::make_unique<ViewportState>(canvas,&engine,&viewportSettings);
        // Group placement owns sidebar state; old per-document drafts must not
        // change it later when a hidden canvas initializes.
        restored.remove("outline"); restored.remove("inspector");
        recovery=std::make_unique<DocumentRecovery>(&engine,workspace,canvas,session->recoveryPath(),restored);
        placementFile=session->recoveryPath()+".window.ini";
        if(!window->property("placementOwner").value<QObject *>()) {
            if(!QFileInfo::exists(placementFile)) {
                std::unique_ptr<QSettings> defaults(AppIdentity::windowSettings());
                QSettings individual(placementFile,QSettings::IniFormat);
                for(const auto &key:defaults->allKeys()) individual.setValue(key,defaults->value(key));
                individual.sync();
            }
            installGroupPlacement(window,placementFile,application->m_directory);
        }
#ifdef Q_OS_WIN
        installWindowsDialogs(&engine,window,workspace);
#endif
        connect(&engine,&Engine::changed,this,[this] { session->setDocument(engine.documentPath()); application->updateTabs(); });
        connect(&engine,&Engine::tabActionRequested,this,[this](QString action,qint64 target) { application->m_active=window; application->tabAction(action,target); });
        connect(&engine,&Engine::newDocumentRequested,this,[this] { if(application->prepareTabCommand()) application->open(); });
        connect(&engine,&Engine::openDocumentRequested,this,[this](QString file) { application->open(file); });
        connect(&engine,&Engine::windowCloseApproved,this,[this](bool) { recovery->remove(); session->forgetDocument(); });
        connect(&engine,&Engine::quitRequested,this,[this] { application->requestQuit(); });
        engine.setWindowNavigation([this] { return application->windows(); },[this](qint64 identity) { application->activate(identity); });
#ifdef Q_OS_MACOS
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
                QMetaObject::invokeMethod(guard->workspace,"finishSaveDialog",Q_ARG(QVariant,path));
            }); });
        });
        connect(&engine,&Engine::nativeCloseRequested,this,[this] {
            if(closeSheet) return; closeSheet=true;
            QTextDocument title; title.setHtml(engine.nodes().value(1).text);
            QString name=engine.documentPath().isEmpty()?title.toPlainText():QFileInfo(engine.documentPath()).fileName();
            QPointer<MacDocumentWindow> guard(this);
            showMacCloseConfirmation(window,name,[guard](int choice) { if(guard) QTimer::singleShot(0,guard,[guard,choice] {
                guard->closeSheet=false;
                QMetaObject::invokeMethod(guard->workspace,choice==1?"saveBeforeClosing":choice==2?"approveClose":"cancelClose");
            }); });
        });
#endif
        connect(&poll,&QTimer::timeout,this,[this] {
            switch(session->pollQuit()) {
            case DocumentSession::QuitAction::Confirm: session->voteToQuit(recovery->prepareQuit()); break;
            case DocumentSession::QuitAction::Cancel:
                application->m_quitting=false; QMetaObject::invokeMethod(workspace,"abortSessionQuit"); break;
            case DocumentSession::QuitAction::Close: QMetaObject::invokeMethod(workspace,"completeSessionQuit"); break;
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
    qApp->installEventFilter(this);
    connect(&m_qml,&QQmlEngine::warnings,this,[](const QList<QQmlError> &errors) {
        for(const auto &e:errors) fprintf(stderr,"QML: %s\n",qPrintable(e.toString()));
    });
}
MacApplication::~MacApplication() {
    QSet<QQuickWindow *> hosts;
    for(auto *document:m_documents) hosts.insert(document->window);
    for(auto *document:m_documents) delete document;
    for(auto *host:hosts) delete host;
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
#ifdef Q_OS_MACOS
    installMacFileMenu(&m_menuOwner,[this](QString action,QString path) { command(action,path); },[this] { return RecentDocuments(m_directory).list(); });
    installMacHelpMenu(&m_menuOwner);
    installMacWindowMenu(&m_menuOwner,[this] { return windows(); },[this](qint64 id) { activate(id); });
    installMacReopenHandler(&m_menuOwner,[this] { reopen(); });
#endif
    m_restoringTabs=true;
    const auto paths=files.isEmpty()&&!fresh?DocumentSession::restorePaths(m_directory,true):files;
    for(const auto &path:paths) open(path,theme);
    if(m_documents.empty()) open({},theme);
    if(files.isEmpty() && !fresh) {
        QSettings saved(m_directory+"/tabs.ini",QSettings::IniFormat);
        std::vector<MacDocumentWindow *> restoredOrder;
        for(const auto &value:saved.value("groups").toList()) {
            MacDocumentWindow *first=nullptr;
            for(const auto &snapshot:value.toMap().value("paths").toStringList()) for(auto *d:m_documents) if(d->session->recoveryPath()==snapshot) {
                if(std::find(restoredOrder.begin(),restoredOrder.end(),d)==restoredOrder.end()) restoredOrder.push_back(d);
                if(!first) first=d;
                else moveDocument(d,first->window);
            }
            if(first) {
                activate(first->id);
                const auto group=value.toMap();
                if(group.contains("outline")) first->window->setProperty("outlineVisible",group.value("outline"));
                if(group.contains("inspector")) first->window->setProperty("inspectorVisible",group.value("inspector"));
            }
            for(auto *d:m_documents) if(d->session->recoveryPath()==value.toMap().value("active").toString()) activate(d->id);
        }
        for(auto *d:m_documents) if(std::find(restoredOrder.begin(),restoredOrder.end(),d)==restoredOrder.end()) restoredOrder.push_back(d);
        m_documents=std::move(restoredOrder);
        const auto active=saved.value("activeDocument").toString();
        for(auto *d:m_documents) if(d->session->recoveryPath()==active) { activate(d->id); break; }
    }
    m_restoringTabs=false; updateTabs();
    m_tabTimer=new QTimer(this); m_tabTimer->setInterval(500);
    connect(m_tabTimer,&QTimer::timeout,this,[this] { updateTabs(); saveTabs(); }); m_tabTimer->start();
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
        fprintf(stderr,"Could not open document: %s\n",qPrintable(m_error));
        auto *host=document->window;
        const bool ownsHost=host && std::none_of(m_documents.begin(),m_documents.end(),[host](auto *d) { return d->window==host; });
        delete document; if(ownsHost) delete host; return nullptr;
    }
    m_documents.push_back(document); activate(document->id); return document->window;
}
void MacApplication::workspaceClosed() {
    for(auto *document:m_documents) if(document->workspace==sender()) {
        QTimer::singleShot(0,document,[this,document] { remove(document); });
        return;
    }
}
void MacApplication::workspaceCloseCancelled() {
    for(auto *d:m_documents) if(d->workspace==sender() && d->closeReturnId) {
        const auto previous=d->closeReturnId; d->closeReturnId=0;
        QTimer::singleShot(0,this,[this,previous] { activate(previous); });
        return;
    }
}
void MacApplication::remove(MacDocumentWindow *document) {
    if(std::find(m_documents.begin(),m_documents.end(),document)==m_documents.end()) return;
    auto *host=document->window;
    document->savePlacement(); document->viewport->flush(); document->poll.stop();
    MacDocumentWindow *replacement=nullptr;
    MacDocumentWindow *previouslyActive=nullptr;
    bool passedClosedTab=false;
    for(auto *d:m_documents) if(d->window==host) {
        if(d==document) { passedClosedTab=true; continue; }
        // Keep the nearest left neighbor, or the first right neighbor when
        // closing the first tab. Other window groups do not affect tab order.
        if(!passedClosedTab || !replacement) replacement=d;
        if(d->id==document->closeReturnId) previouslyActive=d;
    }
    if(previouslyActive) replacement=previouslyActive;
    std::erase(m_documents,document);
    if(replacement) {
        if(host->property("activeWorkspace").value<QObject *>()==document->workspace) activate(replacement->id);
    } else {
        host->setProperty("activeWorkspace",QVariant::fromValue<QObject *>(nullptr));
        if(m_active==host) m_active=nullptr;
        host->hide();
        QTimer::singleShot(0,host,[host] { host->deleteLater(); });
    }
    document->deleteLater();
    updateTabs(); saveTabs();
    if(m_quitting && m_documents.empty()) QTimer::singleShot(0,qApp,[] { QCoreApplication::exit(0); });
}
bool MacApplication::requestQuit() {
    if(m_documents.empty()) return false;
    if(m_quitting) return true;
    // Do not interrupt an in-flight native save/discard sheet.
    for(auto *document:m_documents) if(document->saveSheet || document->closeSheet || document->workspace->property("nativeDialogPending").toBool()) return true;
    saveTabs(); m_quitting=true; m_documents.front()->session->beginQuit(); return true;
}
QQuickWindow *MacApplication::activeWindow() const {
    if(m_active && m_active->isVisible()) return m_active;
    return m_documents.empty()?nullptr:m_documents.back()->window;
}
QObject *MacApplication::activeWorkspace() const {
    auto *host=activeWindow();
    return host ? host->property("activeWorkspace").value<QObject *>() : nullptr;
}
Engine *MacApplication::activeDocument() const {
    for(auto *document:m_documents) if(document->workspace==activeWorkspace()) return &document->engine;
    return nullptr;
}
QVariantList MacApplication::windows() const {
    QVariantList result;
    for(auto *document:m_documents) result.append(QVariantMap{{"windowId",document->id},{"pid",document->id},{"title",document->engine.documentName()},{"current",document->workspace==activeWorkspace()}});
    return result;
}
void MacApplication::activate(qint64 id) {
    for(auto *document:m_documents) if(document->id==id) {
        auto *window=document->window;
        auto *old=window->property("activeWorkspace").value<QObject *>();
        if(old && old!=document->workspace && !m_quitting) {
            if(old->property("modalTabBlocked").toBool() || old->property("nativeDialogPending").toBool()) return;
            for(auto *d:m_documents) if(d->workspace==old && (d->saveSheet || d->closeSheet)) return;
            QVariant result;
            if(!QMetaObject::invokeMethod(old,"commitForTabSwitch",Q_RETURN_ARG(QVariant,result)) || !result.toBool()) return;
        }
        for(auto *peer:m_documents) if(peer->window==window) {
            peer->workspace->setVisible(peer==document);
            peer->workspace->setEnabled(peer==document);
        }
        window->setProperty("activeWorkspace",QVariant::fromValue<QObject *>(document->workspace));
        window->setProperty("documentTabId",document->id);
        window->setProperty("macDocumentWindowId",document->id);
        // show() calls showNormal() and would resize a visible host. Tab
        // activation only presents a minimized or hidden window.
        if(window->windowState()==Qt::WindowMinimized) window->showNormal();
        else if(!window->isVisible()) window->setVisible(true);
        window->raise(); window->requestActivate(); m_active=window;
        document->workspace->forceActiveFocus();
        updateTabs(); return;
    }
}
void MacApplication::reopen() { if(m_documents.empty()) open(); else if(auto *host=activeWindow()) activate(host->property("documentTabId").toLongLong()); }
void MacApplication::command(const QString &action,const QString &path) {
    if(action=="new") { if(prepareTabCommand()) open(); return; }
    if(action=="newtab") { tabAction("new"); return; }
    if(action=="recent") { open(path); return; }
    if(action=="clear") { RecentDocuments(m_directory).clear(); return; }
    auto *window=activeWindow();
    if(!window && action=="open") window=open();
    if(!window) return;
    if(action=="open") QMetaObject::invokeMethod(window,"openDocumentMenu");
    else if(action=="save") QMetaObject::invokeMethod(window,"saveDocument",Q_ARG(QVariant,false));
    else if(action=="close") tabAction("close");
}

bool MacApplication::prepareTabCommand() {
    if(m_quitting || QGuiApplication::modalWindow()) return false;
    for(auto *d:m_documents) if(d->window==activeWindow() && (d->saveSheet || d->closeSheet)) return false;
    auto *workspace=activeWorkspace();
    if(!workspace) return true;
    if(workspace->property("modalTabBlocked").toBool() || workspace->property("nativeDialogPending").toBool()) return false;
    QVariant result;
    return QMetaObject::invokeMethod(workspace,"commitForTabSwitch",Q_RETURN_ARG(QVariant,result)) && result.toBool();
}
void MacApplication::tabAction(const QString &action,qint64 target) {
    if(!prepareTabCommand()) return;
    auto *current=activeWindow(); if(!current) { if(action=="new") open(); return; }
    MacDocumentWindow *selected=nullptr;
    for(auto *d:m_documents) if(d->workspace==activeWorkspace()) selected=d;
    if(!selected) return;
    if(action=="detach" && target && target!=selected->id) {
        activate(target);
        for(auto *d:m_documents) if(d->workspace==activeWorkspace()) selected=d;
        if(selected->id!=target) return;
        current=selected->window;
    }
    if(action=="new") {
        m_openTarget=current; open(); m_openTarget=nullptr;
    } else if(action=="merge") {
        for(auto *d:m_documents) {
            if(d->saveSheet || d->closeSheet || d->workspace->property("modalTabBlocked").toBool() || d->workspace->property("nativeDialogPending").toBool()) return;
            QVariant committed;
            if(!QMetaObject::invokeMethod(d->workspace,"commitForTabSwitch",Q_RETURN_ARG(QVariant,committed)) || !committed.toBool()) return;
        }
        // Append intact group blocks to the destination's existing tab order.
        std::vector<MacDocumentWindow *> ordered;
        for(auto *d:m_documents) if(d->window==current) ordered.push_back(d);
        QSet<QQuickWindow *> grouped{current};
        for(auto *d:m_documents) if(!grouped.contains(d->window)) {
            auto *source=d->window; grouped.insert(source);
            for(auto *peer:m_documents) if(peer->window==source) ordered.push_back(peer);
        }
        m_documents=std::move(ordered);
        for(auto *d:m_documents) if(d->window!=current) moveDocument(d,current);
        activate(selected->id);
    } else if(action=="detach") {
        int count=0; for(auto *d:m_documents) if(d->window==current) ++count;
        if(count>1) {
            auto *host=createHost(&selected->engine);
            if(host) {
                host->setProperty("outlineVisible",current->property("outlineVisible"));
                host->setProperty("inspectorVisible",current->property("inspectorVisible"));
                moveDocument(selected,host);
                host->setGeometry(current->geometry().translated(30,30)); activate(selected->id);
            }
        }
    } else if(action=="activate") activate(target);
    else if(action=="next" || action=="previous") {
        QList<MacDocumentWindow *> peers;
        for(auto *d:m_documents) if(d->window==current) peers.append(d);
        const auto index=peers.indexOf(selected);
        activate(peers[(index+(action=="next"?1:peers.size()-1))%peers.size()]->id);
    } else if(action=="close") {
        if(!target) target=selected->id;
        for(auto *d:m_documents) if(d->id==target) {
            d->closeReturnId=selected->id==d->id ? 0 : selected->id;
            activate(target);
            if(activeWorkspace()==d->workspace) QMetaObject::invokeMethod(d->workspace,"requestClose",Q_ARG(QVariant,true),Q_ARG(QVariant,false));
            break;
        }
    }
    updateTabs(); saveTabs();
}
void MacApplication::moveTab(double identity,int index) {
    if(!prepareTabCommand()) return;
    auto found=std::find_if(m_documents.begin(),m_documents.end(),[identity](auto *d) { return d->id==qint64(identity); });
    if(found==m_documents.end()) return;
    auto *document=*found;
    QList<MacDocumentWindow *> peers;
    for(auto *d:m_documents) if(d->window==document->window) peers.append(d);
    index=qBound(0,index,int(peers.size()-1));
    auto *before=peers[index];
    if(before==document) return;
    const bool after=peers.indexOf(document)<index;
    m_documents.erase(found);
    auto position=std::find(m_documents.begin(),m_documents.end(),before);
    m_documents.insert(position+(after?1:0),document);
    updateTabs(); saveTabs();
}
void MacApplication::updateTabs() {
    QSet<QQuickWindow *> seen;
    for(auto *d:m_documents) {
        if(seen.contains(d->window)) continue;
        seen.insert(d->window);
        QVariantList entries;
        for(auto *peer:m_documents) if(peer->window==d->window) entries.append(QVariantMap{{"id",peer->id},{"title",peer->engine.documentName()},{"edited",peer->workspace->property("documentEdited").toBool()}});
        if(d->window->property("documentTabs").toList()!=entries) d->window->setProperty("documentTabs",entries);
        d->window->setProperty("canMergeWindows",m_documents.size()>size_t(entries.size()));
    }
}
void MacApplication::saveTabs() {
    if(m_restoringTabs || m_quitting) return;
    for(auto *d:m_documents) d->savePlacement();
    QVariantList groups; QSet<QQuickWindow *> seen;
    for(auto *d:m_documents) {
        if(seen.contains(d->window)) continue;
        seen.insert(d->window);
        QStringList paths; QString active;
        for(auto *peer:m_documents) if(d->window==peer->window) {
            paths.append(peer->session->recoveryPath());
            if(peer->id==d->window->property("documentTabId").toLongLong()) active=peer->session->recoveryPath();
        }
        groups.append(QVariantMap{{"paths",paths},{"active",active},{"outline",d->window->property("outlineVisible")},{"inspector",d->window->property("inspectorVisible")}});
    }
    const auto path=m_directory+"/tabs.ini";
    QSettings saved(path,QSettings::IniFormat);
    if(saved.value("version",1).toInt()<2 && QFile::exists(path)) QFile::copy(path,path+".pre-custom-tabs.bak");
    saved.setValue("version",2); saved.setValue("groups",groups);
    for(auto *d:m_documents) if(d->workspace==activeWorkspace()) saved.setValue("activeDocument",d->session->recoveryPath());
    saved.sync();
}
QQuickWindow *MacApplication::createHost(Engine *engine) {
    auto *context=new QQmlContext(m_qml.rootContext(),this);
    context->setContextProperty("engine",engine);
    context->setContextProperty("sharedWindowManaged",true);
    context->setContextProperty("deferWindowShow",true);
    QQmlComponent component(&m_qml,QUrl("qrc:/qml/Main.qml"));
    auto *host=qobject_cast<QQuickWindow *>(component.create(context));
    if(!host) { m_error=component.errorString(); delete context; return nullptr; }
    context->setParent(host);
    connect(host,&QWindow::activeChanged,this,[this,host] { if(host->isActive() && host->isVisible()) m_active=host; });
    connect(host,SIGNAL(tabMoveRequested(double,int)),this,SLOT(moveTab(double,int)));
#ifdef Q_OS_MACOS
    installMacToolbar(host);
#elif defined(Q_OS_WIN)
    new WindowMenuBar(host);
#endif
    return host;
}
void MacApplication::moveDocument(MacDocumentWindow *document,QQuickWindow *host) {
    auto *old=document->window; if(old==host) return;
    document->placement=nullptr;
    document->window=host;
    if(!host->property("placementOwner").value<QObject *>()) {
        installGroupPlacement(host,document->placementFile,m_directory);
    }
    document->workspace->setProperty("hostWindow",QVariant::fromValue(host));
    document->workspace->setParentItem(host->contentItem());
    if(auto *content=host->contentItem()) document->workspace->setSize(content->size());
    document->workspace->setVisible(false); document->workspace->setEnabled(false);
    MacDocumentWindow *replacement=nullptr;
    for(auto *d:m_documents) if(d->window==old) { replacement=d; break; }
    if(replacement) {
        if(old->property("activeWorkspace").value<QObject *>()==document->workspace) activate(replacement->id);
    } else {
        old->setProperty("activeWorkspace",QVariant::fromValue<QObject *>(nullptr));
        if(m_active==old) m_active=nullptr;
        old->hide(); old->deleteLater();
    }
}
bool MacApplication::eventFilter(QObject *object,QEvent *event) {
    if(event->type()!=QEvent::ShortcutOverride && event->type()!=QEvent::KeyPress) return false;
    auto *key=static_cast<QKeyEvent *>(event);
    const auto action=TabShortcuts::action(key->key(),key->modifiers());
    if(action.isEmpty()) return false;
    auto *host=qobject_cast<QQuickWindow *>(object);
    if(!host) if(auto *item=qobject_cast<QQuickItem *>(object)) host=item->window();
    if(!host || host!=activeWindow()) return false;
    event->accept();
    if(event->type()==QEvent::ShortcutOverride) return true;
    if(key->isAutoRepeat() && action!="next" && action!="previous") return true;
    if(action=="window") { if(prepareTabCommand()) open(); }
    else tabAction(action);
    return true;
}
