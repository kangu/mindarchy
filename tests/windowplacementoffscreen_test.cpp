#include "../src/windowplacement.h"
#include <QCoreApplication>
#include <QProcess>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_MACOS
#include <QWindow>
void prepareMacMaximizedWindow(QWindow *window) { Q_UNUSED(window); }
bool macWindowIsZoomed(QWindow *window) { Q_UNUSED(window); return false; }
#endif
class PlacementOffscreenTest : public QObject {
    Q_OBJECT
private slots:
    void panelVisibilitySurvivesRestartAndResize() {
        QTemporaryDir dir; const auto file=dir.filePath("panels.ini");
        QQmlEngine engine; QQmlComponent component(&engine);
        component.setData(R"(import QtQuick
            Window {
                width: 1200; height: 700
                property bool outlineVisible: width>=1100
                property bool inspectorVisible: width>=1000
            })", QUrl());
        QVERIFY2(component.isReady(),qPrintable(component.errorString()));
        for (bool outline : {false,true}) for (bool inspector : {false,true}) {
            {
                std::unique_ptr<QObject> object(component.create());
                auto *window=qobject_cast<QQuickWindow *>(object.get()); QVERIFY(window);
                WindowPlacement placement(window,file);
                window->setProperty("outlineVisible",outline);
                window->setProperty("inspectorVisible",inspector);
                placement.save();
            }
            {
                std::unique_ptr<QObject> object(component.create());
                auto *window=qobject_cast<QQuickWindow *>(object.get()); QVERIFY(window);
                WindowPlacement placement(window,file);
                QCOMPARE(window->property("outlineVisible").toBool(),outline);
                QCOMPARE(window->property("inspectorVisible").toBool(),inspector);
                window->resize(700,700); window->resize(1400,700);
                QCOMPARE(window->property("outlineVisible").toBool(),outline);
                QCOMPARE(window->property("inspectorVisible").toBool(),inspector);
            }
        }
    }
    void nativeHyprlandRestore() {
        if(!QGuiApplication::platformName().startsWith("wayland")) QSKIP("Requires native Hyprland");
        auto call=[](QStringList args) {
            QProcess p; p.start("hyprctl",args); if(!p.waitForFinished(500)) {p.kill(); p.waitForFinished(100); return QByteArray();}
            return p.readAllStandardOutput();
        };
        const auto monitors=QJsonDocument::fromJson(call({"-j","monitors"})).array(); QVERIFY(!monitors.isEmpty());
        const auto mon=monitors[0].toObject(); const double scale=mon["scale"].toDouble(1);
        const QRect expected(mon["x"].toInt()+80,mon["y"].toInt()+60,800,650);
        auto client=[&]() {
            for(const auto &v:QJsonDocument::fromJson(call({"-j","clients"})).array()) {
                const auto c=v.toObject();
                if(c["pid"].toInteger()==QCoreApplication::applicationPid() && c["initialTitle"].toString()=="Placement persistence test") return c;
            }
            return QJsonObject();
        };
        auto rect=[&]() {const auto c=client(); const auto a=c["at"].toArray(),z=c["size"].toArray();
            return a.size()==2 && z.size()==2 ? QRect(a[0].toInt(),a[1].toInt(),z[0].toInt(),z[1].toInt()) : QRect();};
        QTemporaryDir dir; const auto file=dir.filePath("hypr.ini");
        {
            QQuickWindow window; window.setTitle("Placement persistence test"); WindowPlacement placement(&window,file); window.setVisible(true);
            QTRY_VERIFY(!client().isEmpty()); const auto selector="address:"+client()["address"].toString();
            const auto response=call({"eval",QString("hl.dispatch(hl.dsp.window.float({window='%1',action='enable'})); "
                "hl.dispatch(hl.dsp.window.resize({window='%1',x=800,y=650,relative=false})); "
                "hl.dispatch(hl.dsp.window.move({window='%1',x=%2,y=%3,relative=false}))")
                .arg(selector).arg(expected.x()).arg(expected.y())});
            QVERIFY2(response.trimmed()=="ok",response.constData());
            QTRY_COMPARE(rect(),expected); window.close(); QTest::qWait(100);
        }
        QSettings settings(file,QSettings::IniFormat); auto record=settings.value("windowPlacement/v1").toMap();
        QVERIFY(record.value("floating").toBool()); QCOMPARE(record["rect"].toRect(),expected);
        {
            QQuickWindow window; window.setTitle("Placement persistence test"); WindowPlacement placement(&window,file); window.setVisible(true);
            QTRY_COMPARE(rect(),expected); QVERIFY(client()["floating"].toBool()); QTest::qWait(400); window.close(); QTest::qWait(100);
        }
        record["screen"]="missing-monitor"; record["rect"]=QRect(9000,9000,1000,700);
        settings.setValue("windowPlacement/v1",record); settings.sync();
        {
            QQuickWindow window; window.setTitle("Placement persistence test"); WindowPlacement placement(&window,file); window.setVisible(true);
            QTRY_VERIFY(!client().isEmpty()); QTest::qWait(600);
            const QRect screen(mon["x"].toInt(),mon["y"].toInt(),qRound(mon["width"].toInt()/scale),qRound(mon["height"].toInt()/scale));
            QVERIFY(screen.contains(rect())); QVERIFY(rect()!=record["rect"].toRect());
            const auto selector="address:"+client()["address"].toString(); call({"dispatch",QString("hl.dsp.window.float({window='%1',action='disable'})").arg(selector)});
            QTRY_VERIFY(!client()["floating"].toBool()); window.close(); QTest::qWait(100);
        }
        {
            QQuickWindow window; window.setTitle("Placement persistence test"); WindowPlacement placement(&window,file); window.setVisible(true);
            QTRY_VERIFY(!client().isEmpty()); QTest::qWait(500); QVERIFY(!client()["floating"].toBool()); window.close();
        }
    }
    void geometryValidation() {
        QVector<PlacementScreen> screens{{"main",{0,26,1440,874}},{"external",{-1920,0,1920,1080}}};
        QVariantMap saved{{"screen","external"},{"rect",QRect(-1800,100,1000,750)}};
        auto r=resolveWindowPlacement(saved,screens); QVERIFY(r.restored); QCOMPARE(r.rect,saved["rect"].toRect()); QCOMPARE(r.screen,1);
        screens.removeLast(); r=resolveWindowPlacement(saved,screens); QVERIFY(!r.restored); QVERIFY(screens[0].available.contains(r.rect));
        saved["screen"]="main"; saved["rect"]=QRect(1400,10,1000,750);
        QVERIFY(!resolveWindowPlacement(saved,screens).restored);
        saved["rect"]=QRect(0,0,0,0); QVERIFY(!resolveWindowPlacement(saved,screens).restored);
        saved["rect"]=QRect(10,30,1000000,1000000); QVERIFY(!resolveWindowPlacement(saved,screens).restored);
        screens={{"tiny",{2000,200,480,360}}}; r=resolveWindowPlacement({},screens); QVERIFY(screens[0].available.contains(r.rect));
        saved={{"screen","tiny"},{"rect",r.rect}};
        // Smaller-than-normal displays still receive a usable default.
        QVERIFY(resolveWindowPlacement(saved,screens).rect.isValid());
    }
};
QTEST_MAIN(PlacementOffscreenTest)
#include "windowplacementoffscreen_test.moc"
