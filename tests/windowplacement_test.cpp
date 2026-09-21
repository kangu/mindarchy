#include "../src/windowplacement.h"
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QScreen>
#include <QSettings>
#include <QWindow>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_MACOS
void nativeZoomForTest(QWindow *window);
bool nativeZoomedForTest(QWindow *window);
#endif
class PlacementTest : public QObject {
    Q_OBJECT
private slots:
    void visibleWindowKeepsGeometryWhenPlacementAttaches() {
        if(QGuiApplication::platformName().startsWith("wayland")) QSKIP("Qt state test");
        QTemporaryDir dir; const auto file=dir.filePath("visible.ini");
        QSettings settings(file,QSettings::IniFormat);
        auto *screen=QGuiApplication::primaryScreen();
        const QRect saved(screen->availableGeometry().topLeft()+QPoint(48,48),QSize(980,720));
        settings.setValue("windowPlacement/v1",QVariantMap{{"backend","qt"},{"state","normal"},
            {"screen",screen->name()+"|"+screen->serialNumber()},{"rect",saved}}); settings.sync();
        QWindow window; window.resize(1112,743); window.setPosition(80,90); window.setVisible(true);
        QTest::qWait(50);
        const auto expected=window.geometry();
        WindowPlacement placement(&window,file);
        QCOMPARE(window.geometry(),expected);
        window.close();
    }
    void maximizedWindowRestores_data() {
        QTest::addColumn<bool>("nativeZoom");
        QTest::newRow("qt-titlebar-double-click") << false;
#ifdef Q_OS_MACOS
        QTest::newRow("appkit-zoom") << true;
#endif
    }
    void maximizedWindowRestores() {
        if(QGuiApplication::platformName()!="cocoa") QSKIP("Requires native macOS");
        QFETCH(bool,nativeZoom);
        QTemporaryDir dir; const auto file=dir.filePath("zoom.ini");
        const auto area=QGuiApplication::primaryScreen()->availableGeometry();
        const QRect normal(area.topLeft()+QPoint(40,40),QSize(900,700));
        {
            QWindow window; window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
            WindowPlacement placement(&window,file); window.setVisible(true);
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            window.setGeometry(normal);
            QTRY_COMPARE(window.geometry(),normal);
            QTest::qWait(100);
#ifdef Q_OS_MACOS
            if(nativeZoom) nativeZoomForTest(&window);
            else
#endif
                window.showMaximized();
            QTest::qWait(200);
            window.close();
        }
        QSettings settings(file,QSettings::IniFormat); auto saved=settings.value("windowPlacement/v1").toMap();
        QCOMPARE(saved["state"].toString(),QString("maximized"));
        QCOMPARE(saved["rect"].toRect(),normal);
        {
            QWindow window; window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
            WindowPlacement placement(&window,file); window.setVisible(true);
            QTRY_COMPARE(window.windowState(),Qt::WindowMaximized);
            QTest::qWait(100);
            window.close();
        }
        settings.sync(); saved=settings.value("windowPlacement/v1").toMap();
        QCOMPARE(saved["state"].toString(),QString("maximized"));
        QCOMPARE(saved["rect"].toRect(),normal);
        {
            QWindow window; window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
            WindowPlacement placement(&window,file); window.setVisible(true);
            QTRY_COMPARE(window.windowState(),Qt::WindowMaximized);
            window.showNormal(); QTest::qWait(100);
            QCOMPARE(window.geometry(),normal);
            window.close();
        }
    }
    void maximizedBeforeFirstShow() {
        if(QGuiApplication::platformName()!="cocoa") QSKIP("Requires native macOS");
        QTemporaryDir dir; const auto file=dir.filePath("first-show.ini");
        auto *screen=QGuiApplication::primaryScreen();
        const QRect normal(screen->availableGeometry().topLeft()+QPoint(40,40),QSize(900,700));
        QSettings settings(file,QSettings::IniFormat);
        settings.setValue("windowPlacement/v1",QVariantMap{{"backend","qt"},{"state","maximized"},
            {"screen",screen->name()+"|"+screen->serialNumber()},{"rect",normal}}); settings.sync();
        QWindow window; window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
        WindowPlacement placement(&window,file);
        QVERIFY(!window.isVisible());
        QCOMPARE(window.windowState(),Qt::WindowMaximized);
#ifdef Q_OS_MACOS
        QVERIFY(nativeZoomedForTest(&window));
#endif
        const auto initial=window.geometry();
        QVERIFY(initial.width()>normal.width());
        window.setVisible(true);
        QCOMPARE(window.geometry(),initial);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.geometry(),initial);
        window.showNormal(); QTest::qWait(100);
        QCOMPARE(window.geometry(),normal);
        window.close();
    }
    void maximizedMissingDisplayFallsBack() {
        if(QGuiApplication::platformName().startsWith("wayland")) QSKIP("Qt state test");
        QTemporaryDir dir; const auto file=dir.filePath("missing.ini");
        QSettings settings(file,QSettings::IniFormat);
        settings.setValue("windowPlacement/v1",QVariantMap{{"backend","qt"},{"state","maximized"},
            {"screen","disconnected"},{"rect",QRect(9000,9000,900,700)}}); settings.sync();
        QWindow window; WindowPlacement placement(&window,file); window.setVisible(true); QTest::qWait(100);
        QCOMPARE(window.windowState(),Qt::WindowNoState);
        QVERIFY(QGuiApplication::primaryScreen()->availableGeometry().contains(window.geometry()));
        window.close();
    }
    void savesAndRestoresNativeWindow() {
        if(QGuiApplication::platformName().startsWith("wayland")) QSKIP("Covered by the Hyprland end-to-end replay");
        QTemporaryDir dir; const auto file=dir.filePath("placement.ini");
        auto *screen=QGuiApplication::primaryScreen(); const auto available=screen->availableGeometry();
        const QRect expected(available.topLeft()+QPoint(20,30),QSize(std::min(900,available.width()-40),std::min(700,available.height()-60)));
        {
            QWindow window; WindowPlacement placement(&window,file); window.setVisible(true);
            window.setGeometry(expected); QTest::qWait(100); placement.save();
            QSettings saved(file,QSettings::IniFormat); QCOMPARE(saved.value("windowPlacement/v1").toMap()["rect"].toRect(),window.geometry());
        }
        {
            QWindow window; WindowPlacement placement(&window,file); window.setVisible(true); QTest::qWait(100);
            QCOMPARE(window.geometry(),expected);
            window.close();
        }
        QSettings saved(file,QSettings::IniFormat); auto record=saved.value("windowPlacement/v1").toMap();
        record["screen"]="disconnected-display"; record["rect"]=QRect(9000,9000,1000,700); saved.setValue("windowPlacement/v1",record); saved.sync();
        QWindow window; WindowPlacement placement(&window,file); window.setVisible(true); QTest::qWait(100);
        QVERIFY(available.contains(window.geometry())); QVERIFY(window.geometry()!=record["rect"].toRect());
    }
};
QTEST_MAIN(PlacementTest)
#include "windowplacement_test.moc"
