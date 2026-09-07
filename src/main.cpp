#include "canvas.h"
#include "engine.h"
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
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
#include <QSGRendererInterface>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <memory>
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("Mindmap Lab");
    app.setOrganizationName("MindmapBlue");
    app.setDesktopFileName("blue.mindmap.lab");
    QIcon applicationIcon;
    for (int size : {16, 24, 32, 48, 64, 128, 256, 512})
        applicationIcon.addFile(QString(":/assets/icons/mindmap-blue-%1.png").arg(size));
    app.setWindowIcon(applicationIcon);
    QQuickStyle::setStyle("Basic");
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Qt Quick/C++ mind-map interaction and performance laboratory");
    parser.addHelpOption();
    parser.addOption(
        {"render-benchmark", "Measure 120 pan frames with the complete QML interface"});
    parser.addOption({"benchmark", "Print layout benchmark JSON and exit"});
    parser.addOption({"nodes", "Initial fixture size", "count", "15"});
    parser.addOption({"screenshot", "Capture this application window after startup", "path"});
    parser.addOption({"quit-after", "Exit after milliseconds (test runs)", "ms"});
    parser.addOption({"theme", "Start with a theme ID and show Themes", "id"});
    parser.addOption({"document", "Open a prototype JSON document", "path"});
    parser.process(app);
    Engine document;
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
    if (parser.isSet("document"))
        document.open(parser.value("document"));
    if (parser.isSet("theme")) {
        if (!Themes::contains(parser.value("theme"))) {
            fprintf(stderr, "Unknown theme: %s\n", qPrintable(parser.value("theme")));
            return 1;
        }
        document.setThemeId(parser.value("theme"));
    }
    qmlRegisterUncreatableType<Engine>("MindmapLab", 1, 0, "Engine", "Provided by application");
    qmlRegisterType<MindCanvas>("MindmapLab", 1, 0, "MindCanvas");
    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty("engine", &document);
    QObject::connect(
        &qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    QObject::connect(&qml, &QQmlEngine::warnings, [](const QList<QQmlError> &errors) {
        for (const auto &e : errors)
            fprintf(stderr, "QML: %s\n", qPrintable(e.toString()));
    });
    qml.load(QUrl("qrc:/qml/Main.qml"));
    if (qml.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(qml.rootObjects().first());
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
        QTimer::singleShot(parser.value("quit-after").toInt(), &app, &QCoreApplication::quit);
    return app.exec();
}
