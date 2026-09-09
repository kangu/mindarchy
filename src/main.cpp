#include "shelltheme.h"
#ifdef Q_OS_WIN
#include "windowsdialogs.h"
#include "windowmenubar.h"
#endif
#include "viewportstate.h"
#include "canvas.h"
#include "preview.h"
#include <QFileOpenEvent>
#include <QProcess>
#include <functional>
#include "engine.h"
#include "documentsession.h"
#include "documentrecovery.h"
#include "windowplacement.h"
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTextDocument>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <memory>
class DocumentApplication : public QGuiApplication {
public:
    using QGuiApplication::QGuiApplication;
    QStringList pendingFiles;
    std::function<void(QString)> openFile;
    std::function<bool()> requestQuit;
    bool event(QEvent *event) override {
        if (event->type() == QEvent::Quit && requestQuit && requestQuit()) {
            event->ignore();
            return true;
        }
        if(event->type()==QEvent::FileOpen) {
            auto *file=static_cast<QFileOpenEvent *>(event);
            if(!file->url().isLocalFile()) return false;
            const auto path=file->url().toLocalFile();
            if(openFile) openFile(path); else pendingFiles.append(path);
            return true;
        }
        return QGuiApplication::event(event);
    }
};
int main(int argc, char **argv) {
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);
    for(int i=1;i<argc;++i) if(QByteArray(argv[i])=="--render-preview") {
        qputenv("QT_QPA_PLATFORM","offscreen");
#ifdef Q_OS_WIN
        // Unlike the native Windows QPA plugin, offscreen needs a font path.
        if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR"))
            qputenv("QT_QPA_FONTDIR", (qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts").toUtf8());
#endif
    }
    DocumentApplication app(argc, argv);
    app.setApplicationName("Mindarchy");
#ifdef MINDMAP_VERSION
    app.setApplicationVersion(MINDMAP_VERSION);
#else
    app.setApplicationVersion("0.1.0");
#endif
    app.setOrganizationName("Mindarchy");
    app.setDesktopFileName("blue.mindmap.lab");
    QIcon applicationIcon;
#ifdef Q_OS_MACOS
    applicationIcon.addFile(":/assets/icons/mindarchy-macos-512.png");
#else
    for (int size : {16, 24, 32, 48, 64, 128, 256, 512})
        applicationIcon.addFile(QString(":/assets/icons/mindarchy-%1.png").arg(size));
#endif
    app.setWindowIcon(applicationIcon);
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Mindarchy — mind maps, tasks, calendars and meeting notes");
    parser.addOption({"no-window-state", "Use default window geometry without saving placement"});
    parser.addOption({"recover", "Restore an internal recovery snapshot", "path"});
    parser.addOption({"new", "Start a new blank mindmap"});
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(
        {"render-benchmark", "Measure 120 pan frames with the complete QML interface"});
    parser.addOption({"benchmark", "Print layout benchmark JSON and exit"});
    parser.addOption({"nodes", "Initial fixture size", "count", "15"});
    parser.addOption({"screenshot", "Capture this application window after startup", "path"});
    parser.addOption({"quit-after", "Exit after milliseconds (test runs)", "ms"});
    parser.addOption({"theme", "Start with a theme ID and show Themes", "id"});
    parser.addOption({"document", "Open an .omm or legacy JSON document", "path"});
    parser.addPositionalArgument("file", "Document to open (.omm or legacy .json)", "[file...]");
    parser.addOption({"render-preview", "Render a document without opening a window", "path"});
    parser.addOption({"preview-output", "PNG output for --render-preview", "path"});
    parser.addOption({"preview-size", "Maximum preview dimension (32–4096 pixels)", "pixels", "1600"});
    parser.process(app);
    Engine document(nullptr, Engine::InitialContent::Blank);
    if(parser.isSet("render-preview")) {
        bool valid=false; int size=parser.value("preview-size").toInt(&valid);
        if(!valid || size<32 || size>4096 || !parser.isSet("preview-output")) {
            fprintf(stderr,"Preview requires --preview-output and size between 32 and 4096\n"); return 2;
        }
        if(!document.open(parser.value("render-preview"))) { fprintf(stderr,"%s\n",qPrintable(document.error())); return 1; }
        return renderMapPreview(document,QSize(size,size)).save(parser.value("preview-output"),"PNG") ? 0 : 1;
    }
    if (parser.isSet("benchmark")) {
        QJsonArray results;
        for (int count : {15, 1000, 10000}) {
            QElapsedTimer t;
            t.start();
            document.loadFixture(count);
            double creation = t.nsecsElapsed() / 1000000.;
            for (QString layout : {"Horizontal", "Vertical", "Compact"}) {
                document.setLayout(layout);
                results.append(QJsonObject{{"nodes", count},
                                           {"layout", layout},
                                           {"layout_ms", document.layoutMs()},
                                           {"fixture_build_ms", creation},
                                           {"visible", document.visibleCount()},
                                           {"width", document.bounds().width()},
                                           {"height", document.bounds().height()}});
            }
        }
        QJsonObject report{{"qt", qVersion()},
                           {"platform", QGuiApplication::platformName()},
                           {"results", results},
                           {"measurement", "Synchronous full layout CPU time, includes text "
                                           "measurement; not frame time or end-to-end latency"}};
        auto bytes = QJsonDocument(report).toJson();
        fwrite(bytes.constData(), 1, bytes.size(), stdout);
        return 0;
    }
    if (parser.isSet("nodes"))
        document.loadFixture(parser.value("nodes").toInt());
    QStringList files=parser.positionalArguments();
    if(parser.isSet("document")) files.prepend(parser.value("document"));
    files.append(app.pendingFiles); app.pendingFiles.clear(); files.removeDuplicates();
    const bool sessionEnabled = !parser.isSet("nodes") && !parser.isSet("screenshot") &&
        !parser.isSet("quit-after") && !parser.isSet("render-benchmark") && !parser.isSet("no-window-state");
    const bool recoveryEnabled = sessionEnabled && (QGuiApplication::platformName()=="cocoa" || QGuiApplication::platformName()=="windows");
    QString recoveryFile=parser.value("recover");
    if(!recoveryFile.isEmpty() && (!recoveryEnabled ||
       QFileInfo(recoveryFile).absolutePath()!=QFileInfo(DocumentSession::defaultDirectory()).absoluteFilePath() ||
       QUuid(QFileInfo(recoveryFile).completeBaseName()).isNull())) {
        fprintf(stderr,"Invalid recovery location\n"); return 1;
    }
    const bool restoring = sessionEnabled && files.isEmpty() && !parser.isSet("new") && recoveryFile.isEmpty();
    if (restoring) {
        // Validate before spawning any windows; a missing or corrupt map must
        // not prevent the remaining session (or a blank map) from opening.
        for (const auto &path : DocumentSession::restorePaths(DocumentSession::defaultDirectory(),recoveryEnabled)) {
            Engine candidate(nullptr, Engine::InitialContent::Blank);
            const bool snapshot=recoveryEnabled && path.endsWith(".recovery");
            if (snapshot ? candidate.openRecovery(path) : candidate.open(path)) files.append(path);
            else fprintf(stderr,"Could not restore %s: %s\n",qPrintable(path),qPrintable(candidate.error()));
        }
    }
    auto openInNewInstance=[](QString path) {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {path.endsWith(".recovery") ? "--recover" : "--document",path});
    };
    if(recoveryFile.isEmpty() && !files.isEmpty() && recoveryEnabled && files.first().endsWith(".recovery"))
        recoveryFile=files.takeFirst();
    QVariantMap recoveredUi;
    const bool startedWithFile=!files.isEmpty() || !recoveryFile.isEmpty();
    if(!recoveryFile.isEmpty() && !document.openRecovery(recoveryFile,&recoveredUi)) {
        fprintf(stderr,"%s\n",qPrintable(document.error())); return 1;
    }
    if(recoveryFile.isEmpty() && !files.isEmpty() && !document.open(files.takeFirst())) {
        fprintf(stderr,"%s\n",qPrintable(document.error())); return 1;
    }
    std::unique_ptr<DocumentSession> session;
    if (sessionEnabled) {
        session = std::make_unique<DocumentSession>(DocumentSession::defaultDirectory(),recoveryFile);
        if(!session->locked()) { fprintf(stderr,"Document recovery is already open\n"); return 1; }
        session->setDocument(document.documentPath());
        QObject::connect(&document, &Engine::changed, &app, [&] {
            session->setDocument(document.documentPath());
        });
    }
    for(const auto &path:files) openInNewInstance(path);
    if (parser.isSet("theme")) {
        if (!Themes::contains(parser.value("theme"))) {
            fprintf(stderr, "Unknown theme: %s\n", qPrintable(parser.value("theme")));
            return 1;
        }
        document.setThemeId(parser.value("theme"));
    }
    ShellTheme shellTheme;
    qmlRegisterSingletonInstance("Mindarchy", 1, 0, "ShellTheme", &shellTheme);
    qmlRegisterUncreatableType<Engine>("Mindarchy", 1, 0, "Engine", "Provided by application");
    qmlRegisterType<MindCanvas>("Mindarchy", 1, 0, "MindCanvas");
    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty("engine", &document);
    qml.rootContext()->setContextProperty("deferWindowShow", true);
    qml.rootContext()->setContextProperty("nativeCloseAvailable", QGuiApplication::platformName() == "cocoa" || QGuiApplication::platformName() == "windows");
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    QObject::connect(&qml, &QQmlEngine::warnings, [](const QList<QQmlError> &errors) {
        for (const auto &e : errors)
            fprintf(stderr, "QML: %s\n", qPrintable(e.toString()));
    });
    QObject::connect(&document,&Engine::newDocumentRequested,&app,[] {
        if(!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--new"}))
            fprintf(stderr,"Could not start a new document window\n");
    });
    qml.load(QUrl("qrc:/qml/Main.qml"));
    if (qml.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(qml.rootObjects().first());
    QSettings viewportSettings("Mindarchy", "Mindarchy");
    std::unique_ptr<ViewportState> viewportState;
    if (sessionEnabled && window) {
        if (auto *canvas = window->findChild<MindCanvas *>("mindCanvas"))
            viewportState = std::make_unique<ViewportState>(canvas, &document, &viewportSettings);
    }
    std::unique_ptr<DocumentRecovery> recovery;
    if(recoveryEnabled && session && window) {
        if(auto *canvas=window->findChild<MindCanvas *>("mindCanvas"))
            recovery=std::make_unique<DocumentRecovery>(&document,window,canvas,session->recoveryPath(),recoveredUi);
    }
    QTimer sessionPoll;
    if (session && window) {
        QObject::connect(&document, &Engine::windowCloseApproved, &app, [&](bool forget) {
            if (recovery) recovery->remove();
            if (forget) session->forgetDocument();
        });
        QObject::connect(&document, &Engine::quitRequested, &app, [&] { session->beginQuit(); });
        QObject::connect(&document, &Engine::quitDecision, &app, [&](bool accepted) { session->voteToQuit(accepted); });
        app.requestQuit = [&] {
            if (!window->isVisible() || window->property("allowClose").toBool()) return false;
            session->beginQuit(); return true;
        };
        QObject::connect(&sessionPoll, &QTimer::timeout, window, [&] {
            if (session->takeActivation()) {
                if (window->windowState() == Qt::WindowMinimized) window->showNormal();
                window->raise(); window->requestActivate();
            }
            switch (session->pollQuit()) {
            case DocumentSession::QuitAction::Confirm:
                if(recovery) {
                    session->voteToQuit(recovery->prepareQuit());
                    break;
                }
                window->raise(); window->requestActivate();
                QMetaObject::invokeMethod(window, "requestClose", Q_ARG(QVariant, false), Q_ARG(QVariant, true)); break;
            case DocumentSession::QuitAction::Cancel:
                QMetaObject::invokeMethod(window, "abortSessionQuit"); break;
            case DocumentSession::QuitAction::Close:
                QMetaObject::invokeMethod(window, "completeSessionQuit"); break;
            default: break;
            }
        });
        sessionPoll.start(150);
    }
    if(window && !parser.isSet("no-window-state") && !parser.isSet("screenshot") &&
       !parser.isSet("quit-after") && !parser.isSet("render-benchmark")) {
        const QString placementFile=recovery ? session->recoveryPath()+".window.ini" : QString();
        if(recovery && !QFileInfo::exists(placementFile)) {
            std::unique_ptr<QSettings> defaults(AppIdentity::windowSettings());
            QSettings individual(placementFile,QSettings::IniFormat);
            for(const auto &key:defaults->allKeys()) individual.setValue(key,defaults->value(key));
            individual.sync();
        }
        auto *placement=new WindowPlacement(window,placementFile);
        if(recovery) QObject::connect(&app,&QCoreApplication::aboutToQuit,placement,[placement,placementFile] {
            placement->save();
            QSettings individual(placementFile,QSettings::IniFormat);
            std::unique_ptr<QSettings> defaults(AppIdentity::windowSettings());
            for(const auto &key:individual.allKeys()) defaults->setValue(key,individual.value(key));
            defaults->sync();
        });
    }
#ifdef Q_OS_WIN
    if (window && QGuiApplication::platformName() == "windows") {
        installWindowsDialogs(&document, window);
        new WindowMenuBar(window);
    }
#endif
#ifdef Q_OS_MACOS
    void installMacHelpMenu(QWindow *);
    if (window && QGuiApplication::platformName() == "cocoa")
        QTimer::singleShot(0, window, [window] { installMacHelpMenu(window); });
    void installMacToolbar(QWindow *window);
    if (window && QGuiApplication::platformName() == "cocoa") installMacToolbar(window);
    void installMacWindowMenu(QWindow *, std::function<QVariantList()>, std::function<void(qint64)>);
    if (window && session && QGuiApplication::platformName() == "cocoa") {
        QTimer::singleShot(0, window, [&] {
            installMacWindowMenu(window, [&] { return session->liveWindows(); },
                [&](qint64 pid) { session->activateWindow(pid); });
        });
    }
    void showMacCloseConfirmation(QWindow *, const QString &, std::function<void(int)>);
    void showMacSavePanel(QWindow *, const QString &, std::function<void(QString)>);
    void showMacParentFolderMenu(QWindow *, const QString &, double, double);
    QObject::connect(&document, &Engine::nativeFolderMenuRequested, window, [&, window](double x, double y) {
        if (!document.documentPath().isEmpty()) showMacParentFolderMenu(window, document.documentPath(), x, y);
    });
    bool saveSheetOpen = false;
    QObject::connect(&document, &Engine::nativeSaveRequested, window, [&, window] {
        if (saveSheetOpen) return;
        saveSheetOpen = true;
        QTextDocument title;
        title.setHtml(document.nodes().value(1).text);
        QString name = title.toPlainText().simplified().left(100);
        name.replace('/', '-'); name.replace(':', '-');
        if (name.isEmpty()) name = "Untitled";
        showMacSavePanel(window, name, [&, window](QString path) {
            QTimer::singleShot(0, window, [&, window, path] {
                saveSheetOpen = false;
                QMetaObject::invokeMethod(window, "finishSaveDialog", Q_ARG(QVariant, QVariant(path)));
            });
        });
    });
    bool closeSheetOpen = false;
    QObject::connect(&document, &Engine::nativeCloseRequested, window, [&, window] {
        if (closeSheetOpen) return;
        closeSheetOpen = true;
        QTextDocument rootTitle;
        rootTitle.setHtml(document.nodes().value(1).text);
        QString name = document.documentPath().isEmpty() ? rootTitle.toPlainText()
            : QFileInfo(document.documentPath()).fileName();
        if (name.trimmed().isEmpty()) name = "Untitled";
        showMacCloseConfirmation(window, name, [&, window](int choice) {
            // Let AppKit finish dismissing the sheet before saving or closing.
            QTimer::singleShot(0, window, [&, window, choice] {
                closeSheetOpen = false;
                if (choice == 1) QMetaObject::invokeMethod(window, "saveBeforeClosing");
                else QMetaObject::invokeMethod(window, choice == 2 ? "approveClose" : "cancelClose");
            });
        });
    });
#endif
    bool freshDocument=!startedWithFile && !parser.isSet("nodes") && !parser.isSet("new");
    QObject::connect(&document,&Engine::changed,&app,[&freshDocument] { freshDocument=false; });
    app.openFile=[&document,window,&freshDocument,openInNewInstance](QString path) {
        // A cold Finder launch uses the initial window. Later opens preserve
        // any work in it, including an unsaved new map.
        if(freshDocument) {
            freshDocument=false;
            if(document.open(path) && window)
                if(auto *canvas=window->findChild<MindCanvas *>("mindCanvas")) canvas->initializeView();
        } else openInNewInstance(path);
    };
    for(const auto &path:app.pendingFiles) app.openFile(path);
    app.pendingFiles.clear();
    // Preserve the state prepared by WindowPlacement; show() calls showNormal().
    if(window) window->setVisible(true);
    if(window && !startedWithFile && !parser.isSet("nodes") && !parser.isSet("render-benchmark")) {
        QTimer::singleShot(0,window,[window] {
            if(auto *canvas=window->findChild<MindCanvas *>("mindCanvas"); canvas && canvas->engine()->documentPath().isEmpty()) {
                canvas->fit(); canvas->beginEdit(1);
            }
        });
    }
    if (window && parser.isSet("theme")) {
        window->setProperty("inspectorVisible",true);
        if (auto *tabs=window->findChild<QQuickItem *>("inspectorTabs")) tabs->setProperty("currentIndex",2);
    }
    if (window)
        QObject::connect(
            window, &QQuickWindow::sceneGraphInitialized, window,
            [window] {
                fprintf(stderr,
                        "Scene graph initialized: API=%d (3=OpenGL,5=Vulkan,6=Metal,1=Software), "
                        "platform=%s\n",
                        int(window->rendererInterface()->graphicsApi()),
                        qPrintable(QGuiApplication::platformName()));
            },
            Qt::DirectConnection);
    if (window && parser.isSet("render-benchmark")) {
        QTimer::singleShot(1000, window, [window, &app, &document] {
            auto *canvas = window->findChild<MindCanvas *>("mindCanvas");
            if (!canvas) {
                fprintf(stderr, "Canvas not found for benchmark\n");
                app.exit(2);
                return;
            }
            canvas->fit();
            struct Sample {
                int frame = 0;
                QElapsedTimer clock;
                QVector<double> intervals, prepare, scene;
            };
            auto sample = std::make_shared<Sample>();
            sample->clock.start();
            QObject::connect(
                window, &QQuickWindow::frameSwapped, window,
                [sample, canvas, window, &app, &document] {
                    double interval = sample->clock.nsecsElapsed() / 1000000.;
                    sample->clock.restart();
                    if (sample->frame >= 10) {
                        sample->intervals << interval;
                        sample->scene << canvas->frameMs();
                    }
                    if (sample->frame++ >= 130) {
                        auto percentile = [](QVector<double> values, double p) {
                            std::sort(values.begin(), values.end());
                            return values.isEmpty()
                                       ? 0.
                                       : values[std::min(values.size() - 1,
                                                         qsizetype(values.size() * p))];
                        };
                        QJsonObject report{
                            {"nodes", document.nodeCount()},
                            {"visible_rendered", canvas->visibleRendered()},
                            {"zoom", canvas->zoom()},
                            {"platform", QGuiApplication::platformName()},
                            {"graphics_api", int(window->rendererInterface()->graphicsApi())},
                            {"samples", sample->intervals.size()},
                            {"pan_prepare_p95_ms", percentile(sample->prepare, .95)},
                            {"scene_build_p95_ms", percentile(sample->scene, .95)},
                            {"frame_interval_median_ms", percentile(sample->intervals, .5)},
                            {"frame_interval_p95_ms", percentile(sample->intervals, .95)},
                            {"measurement",
                             "Full QML app, fit view, scripted pan after10 warmup frames. Frame "
                             "intervals include compositor scheduling; labels omitted at overview "
                             "zoom. Not input latency."}};
                        auto bytes = QJsonDocument(report).toJson();
                        fwrite(bytes.constData(), 1, bytes.size(), stdout);
                        app.quit();
                        return;
                    }
                    QElapsedTimer preparation;
                    preparation.start();
                    canvas->panBy(sample->frame % 40 < 20 ? 2 : -2, 0);
                    if (sample->frame >= 10)
                        sample->prepare << preparation.nsecsElapsed() / 1000000.;
                },
                Qt::QueuedConnection);
            canvas->panBy(1, 0);
        });
    }
    if (window && parser.isSet("screenshot")) {
        QString path = parser.value("screenshot");
        QTimer::singleShot(1800, window, [window, path] {
            bool ok = window->grabWindow().save(path);
            fprintf(stderr, "Application screenshot: %s (%s)\n", qPrintable(path),
                    ok ? "saved" : "FAILED");
        });
    }
    if (parser.isSet("quit-after"))
        QTimer::singleShot(parser.value("quit-after").toInt(), &app, [&app] { app.exit(0); });
    return app.exec();
}
