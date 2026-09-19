#include "macapplication.h"
#include "canvas.h"
#include "shelltheme.h"
#include "documentsession.h"
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QProcess>
#include <QtTest>
#import <AppKit/AppKit.h>

class MacApplicationTest : public QObject {
    Q_OBJECT
private slots:
    void windowButtonsStayAlignedAcrossTabs() {
        QTemporaryDir directory;
        const auto file=directory.filePath("Saved map.omm");
        Engine document(nullptr,Engine::InitialContent::Blank);
        QVERIFY(document.save(file));
        MacApplication app(directory.filePath("session")); app.start({file},false);
        auto *host=app.activeWindow(); QVERIFY(host); QTest::qWait(150);
        const auto firstId=host->property("documentTabId").toLongLong();
        auto aligned=[host] {
            NSView *view=reinterpret_cast<NSView *>(host->winId());
            NSWindow *native=view.window;
            int index=0;
            for(NSNumber *kind in @[@(NSWindowCloseButton),@(NSWindowMiniaturizeButton),@(NSWindowZoomButton)]) {
                NSButton *button=[native standardWindowButton:(NSWindowButton)kind.integerValue];
                NSRect rect=[view convertRect:button.bounds fromView:button];
                const double top=view.isFlipped ? NSMidY(rect) : NSHeight(view.bounds)-NSMidY(rect);
                if(qAbs(NSMinX(rect)-(18+20*index))>1 || qAbs(top-30)>1) {
                    qWarning("Button %d moved to x=%.1f, center-from-top=%.1f",index,NSMinX(rect),top);
                    return false;
                }
                ++index;
            }
            return true;
        };
        QVERIFY(aligned());
        app.tabAction("new"); QCOMPARE(app.activeWindow(),host);
        QVERIFY(aligned());
        QTest::qWait(150); QVERIFY(aligned());
        app.activate(firstId); QVERIFY(aligned());
        QTest::qWait(100); QVERIFY(aligned());
        QVERIFY(app.activeDocument()->save(directory.filePath("Renamed map.omm")));
        QVERIFY(aligned()); QTest::qWait(100); QVERIFY(aligned());
        app.tabAction("close"); QTRY_COMPARE(app.windows().size(),1); QVERIFY(aligned());
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void cyclingWindowsKeepsNativeZoom() {
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true); QTest::qWait(150);
        auto *first=app.activeWindow(); QVERIFY(first);
        const auto firstId=first->property("documentTabId").toLongLong();
        auto *second=app.open(); QVERIFY(second); QVERIFY(second!=first); QTest::qWait(150);
        const auto secondId=second->property("documentTabId").toLongLong();
        auto zoom=[](QWindow *window) {
            NSWindow *native=reinterpret_cast<NSView *>(window->winId()).window;
            if(!native.zoomed) [native zoom:nil];
            return native.zoomed;
        };
        auto zoomed=[](QWindow *window) {
            return reinterpret_cast<NSView *>(window->winId()).window.isZoomed;
        };
        QVERIFY(zoom(first)); QVERIFY(zoom(second));
        const auto firstFrame=first->geometry();
        const auto secondFrame=second->geometry();
        app.activate(firstId); QTest::qWait(150);
        QVERIFY(zoomed(first));
        QCOMPARE(first->geometry(),firstFrame);
        app.activate(secondId); QTest::qWait(150);
        QVERIFY(zoomed(second));
        QCOMPARE(second->geometry(),secondFrame);
        app.activate(firstId); QTest::qWait(150);
        QVERIFY(zoomed(first));
        QCOMPARE(first->geometry(),firstFrame);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void sharedTabsAndSeparateWindows() {
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true);
        auto *first=app.activeWindow(); QVERIFY(first);
        QTest::qWait(150);
        const auto firstId=first->property("documentTabId").toLongLong();
        auto *firstEngine=app.activeDocument();
        auto *firstSurface=app.activeWorkspace();
        auto *firstCanvas=firstSurface->findChild<MindCanvas *>("mindCanvas");
        QVERIFY(firstCanvas->commitEditing("First"));
        first->setProperty("outlineVisible",true);
        first->setProperty("inspectorVisible",true);
        app.tabAction("new"); auto *second=app.activeWindow(); QCOMPARE(second,first);
        auto *secondEngine=app.activeDocument(); QVERIFY(secondEngine!=firstEngine);
        auto *secondSurface=app.activeWorkspace(); QVERIFY(secondSurface!=firstSurface);
        QTest::qWait(150);
        secondSurface->findChild<MindCanvas *>("mindCanvas")->commitEditing("Second");
        QCOMPARE(first->property("documentTabs").toList().size(),2);
        if(qEnvironmentVariableIsSet("MINDARCHY_TABS_EVIDENCE")) {
            QTest::qWait(150);
            QVERIFY(first->grabWindow().save(qEnvironmentVariable("MINDARCHY_TABS_EVIDENCE")));
        }
        NSWindow *native=reinterpret_cast<NSView *>(first->winId()).window;
        QCOMPARE(native.tabbingMode,NSWindowTabbingModeDisallowed);
        QVERIFY(!native.tabGroup.tabBarVisible);
        app.tabAction("next"); QCOMPARE(app.activeDocument(),firstEngine);
        QCOMPARE(app.activeWorkspace(),firstSurface);
        QCOMPARE(first->property("outlineVisible").toBool(),true);
        app.tabAction("previous"); QCOMPARE(app.activeDocument(),secondEngine);
        auto *third=app.open(); QVERIFY(third && third!=first);
        auto *thirdEngine=app.activeDocument(); QTest::qWait(100);
        app.tabAction("merge"); QCOMPARE(app.activeWindow(),third);
        QCOMPARE(third->property("documentTabs").toList().size(),3);
        app.tabAction("detach"); QVERIFY(app.activeWindow()!=third);
        QCOMPARE(app.activeDocument(),thirdEngine);
        QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),1);
        app.activate(firstId); QCOMPARE(app.activeDocument(),firstEngine);
        QCOMPARE(app.activeWorkspace(),firstSurface);
        app.tabAction("new"); QTest::qWait(100);
        QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),3);
        QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTRY_COMPARE(app.windows().size(),3);
        QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),2);
        QVERIFY([NSApp.windowsMenu itemWithTitle:@"Show Next Tab"]);
        QVERIFY([NSApp.windowsMenu itemWithTitle:@"Move Tab to New Window"]);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void sharedTabsRecover() {
        QTemporaryDir directory;
        {
            MacApplication app(directory.path()); app.start({},true);
            QTest::qWait(150);
            app.activeWorkspace()->findChild<MindCanvas *>("mindCanvas")->commitEditing("One");
            app.activeWindow()->setProperty("outlineVisible",true);
            app.tabAction("new"); QTest::qWait(150);
            app.activeWorkspace()->findChild<MindCanvas *>("mindCanvas")->commitEditing("Two");
            QVERIFY(app.requestQuit()); QTRY_COMPARE(app.windows().size(),0);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
        {
            MacApplication app(directory.path()); app.start({},false);
            QCOMPARE(app.windows().size(),2); app.updateTabs();
            QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),2);
            QCOMPARE(app.activeDocument()->selectedText(),QString("Two"));
            QVERIFY(app.activeWindow()->property("outlineVisible").toBool());
            NSWindow *native=reinterpret_cast<NSView *>(app.activeWindow()->winId()).window;
            QVERIFY(!native.tabGroup.tabBarVisible);
            while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
        }
    }
    void sharedShortcutDispatch() {
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true); QTest::qWait(150);
        auto *host=app.activeWindow(); auto *first=app.activeDocument();
        QTest::keyClick(host,Qt::Key_T,Qt::ControlModifier);
        QTRY_COMPARE(app.windows().size(),2); QCOMPARE(app.activeWindow(),host);
        auto *second=app.activeDocument(); QVERIFY(second!=first); QTest::qWait(100);
        QTest::keyClick(host,Qt::Key_Tab,Qt::MetaModifier);
        QCOMPARE(app.activeDocument(),first);
        QTest::keyClick(host,Qt::Key_Backtab,Qt::MetaModifier|Qt::ShiftModifier);
        QCOMPARE(app.activeDocument(),second);
        // An unmodified Tab remains a node-editing command, never tab navigation.
        QTest::keyClick(host,Qt::Key_Tab); QCOMPARE(app.activeDocument(),second);
        // New/close are not auto-repeatable.
        QKeyEvent repeated(QEvent::KeyPress,Qt::Key_T,Qt::ControlModifier,"t",true);
        QCoreApplication::sendEvent(host,&repeated); QCOMPARE(app.windows().size(),2);
        // Native menu commands reach the same document model.
        NSMenuItem *next=[NSApp.windowsMenu itemWithTitle:@"Show Next Tab"];
        [NSApp sendAction:next.action to:next.target from:next]; QCOMPARE(app.activeDocument(),first);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void documentsShareApplicationAndRecover() {
        QTemporaryDir directory;
        const auto history=directory.filePath("session");
        QString saved=directory.filePath("Saved map.omm");
        {
            MacApplication app(history);
            QCOMPARE(app.startOrForward({},true),MacApplication::Launch::Primary);
            app.start({},true);
            QCOMPARE(app.windows().size(),1);
            auto *first=app.activeWindow(); QVERIFY(first);
            QTRY_VERIFY(first->isVisible()); QTest::qWait(150);
            auto *firstEngine=app.activeDocument(); QVERIFY(firstEngine);
            app.activeWorkspace()->findChild<MindCanvas *>("mindCanvas")->commitEditing("Central idea");
            firstEngine->setText(1,"Saved map"); QVERIFY(firstEngine->save(saved));
            auto *second=app.open(); QVERIFY(second); QVERIFY(second!=first);
            QTest::qWait(150);
            auto *secondEngine=app.activeDocument();
            app.activeWorkspace()->findChild<MindCanvas *>("mindCanvas")->commitEditing("Central idea");
            secondEngine->setText(1,"Recovered draft");
            QCOMPARE(firstEngine->selectedText(),QString("Saved map"));
            QCOMPARE(app.windows().size(),2);
            QCOMPARE(app.open(saved),first); QCOMPARE(app.windows().size(),2);
            // Separate CLI invocation is forwarded into this application.
            QProcess secondary;
            secondary.start(QCoreApplication::applicationFilePath(),{"--forward-test",history});
            QVERIFY2(secondary.waitForStarted(),qPrintable(secondary.errorString()));
            QTRY_VERIFY(secondary.state()==QProcess::NotRunning);
            QCOMPARE(secondary.exitStatus(),QProcess::NormalExit);
            QVERIFY2(secondary.exitCode()==0,secondary.readAllStandardError().constData()); QTRY_COMPARE(app.windows().size(),3);
            auto *third=app.activeWindow(); QVERIFY(third!=first && third!=second);
            QTest::qWait(150);
            // AppKit's standard Dock list is populated from the native window
            // registration; do not append duplicate custom Dock menu entries.
            auto nativeWindowCount=[] {
                int count=0;
                for(NSMenuItem *item in NSApp.windowsMenu.itemArray)
                    if(item.action==@selector(makeKeyAndOrderFront:)) ++count;
                return count;
            };
            QTRY_COMPARE(nativeWindowCount(),3);
            const auto firstId=first->property("macDocumentWindowId").toLongLong();
            NSMenuItem *firstItem=[NSApp.windowsMenu itemWithTitle:@"Saved map"];
            QVERIFY(firstItem);
            first->showMinimized();
            QTest::qWait(400); // Let AppKit finish its minimize animation before selecting.
            [NSApp sendAction:firstItem.action to:firstItem.target from:firstItem];
            QTRY_COMPARE(app.activeWindow(),first); QTRY_VERIFY(first->windowState()!=Qt::WindowMinimized);
            // A normal close forgets only that document and leaves the app running.
            QMetaObject::invokeMethod(third,"requestClose",Q_ARG(QVariant,true),Q_ARG(QVariant,false));
            QTRY_COMPARE(app.windows().size(),2);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            QTRY_COMPARE(nativeWindowCount(),2);
            // Cycling in one process must use the active window, not process ID.
            app.activate(firstId); firstEngine->cycleApplicationWindow(1);
            QCOMPARE(app.activeWindow(),second);
            // Keep both documents unfinished: clean saved maps now reopen from Home.
            firstEngine->setNotes("Unfinished notes");
            // A failed checkpoint cancels the entire quit without closing peers.
            QTest::qWait(1100);
            const auto snapshots=QDir(history).entryList({"*.recovery"},QDir::Files);
            QVERIFY(!snapshots.isEmpty());
            const auto blocked=QDir(history).filePath(snapshots.first());
            QVERIFY(QFile::remove(blocked)); QVERIFY(QDir().mkdir(blocked));
            QVERIFY(app.requestQuit()); QTRY_VERIFY(!app.quitting());
            QCOMPARE(app.windows().size(),2);
            QVERIFY(QDir().rmdir(blocked));
            QVERIFY(app.requestQuit());
            QTRY_COMPARE(app.windows().size(),0);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
        const auto restored=DocumentSession::restorePaths(history,true);
        QCOMPARE(restored.size(),2);
        bool foundDraft=false;
        for(const auto &path:restored) {
            Engine engine(nullptr,Engine::InitialContent::Blank);
            QVERIFY(engine.openRecovery(path));
            if(engine.selectedText()=="Recovered draft") foundDraft=true;
        }
        QVERIFY(foundDraft);
        // A subsequent launch restores all documents within one process too.
        {
            MacApplication app(history);
            QCOMPARE(app.startOrForward({},false),MacApplication::Launch::Primary);
            app.start({},false);
            QCOMPARE(app.windows().size(),2);
            QTest::qWait(150);
            // Simulate confirmed discard on each window: the application stays
            // alive with no document windows, and a Dock click creates a new one.
            while(!app.windows().isEmpty()) {
                QMetaObject::invokeMethod(app.activeWindow(),"approveClose");
                QTest::qWait(50);
            }
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
            QVERIFY(DocumentSession::restorePaths(history,true).isEmpty());
            QVERIFY(!app.requestQuit());
            [NSApp.delegate applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];
            QCOMPARE(app.windows().size(),1);
            QTest::qWait(150);
            QMetaObject::invokeMethod(app.activeWindow(),"requestClose",Q_ARG(QVariant,true),Q_ARG(QVariant,false));
            QTRY_COMPARE(app.windows().size(),0);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    if(argc==3 && QString(argv[1])=="--forward-test") {
        MacApplication desktop(QString::fromLocal8Bit(argv[2]));
        return desktop.startOrForward({},true)==MacApplication::Launch::Forwarded?0:1;
    }
    QQuickStyle::setStyle("Basic");
    ShellTheme shell;
    qmlRegisterSingletonType<ShellTheme>("Mindarchy",1,0,"ShellTheme",[](QQmlEngine *,QJSEngine *) -> QObject * { return new ShellTheme; });
    qmlRegisterUncreatableType<Engine>("Mindarchy",1,0,"Engine","Provided by application");
    qmlRegisterType<MindCanvas>("Mindarchy",1,0,"MindCanvas");
    MacApplicationTest test;
    return QTest::qExec(&test,argc,argv);
}
#include "macapplication_test.moc"
