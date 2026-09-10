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
    void nativeTabsAndSeparateWindows() {
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true);
        auto *first=app.activeWindow(); QVERIFY(first);
        first->findChild<MindCanvas *>("mindCanvas")->commitEditing("First");
        app.tabAction("new"); auto *second=app.activeWindow(); QVERIFY(second && second!=first);
        QTest::qWait(200);
        NSWindow *a=reinterpret_cast<NSView *>(first->winId()).window;
        NSWindow *b=reinterpret_cast<NSView *>(second->winId()).window;
        QCOMPARE(a.tabbedWindows.count,NSUInteger(2)); QVERIFY([a.tabbedWindows containsObject:b]);
        app.tabAction("next"); QTest::qWait(100); QCOMPARE(a.tabGroup.selectedWindow,a);
        app.tabAction("previous"); QTest::qWait(100); QCOMPARE(a.tabGroup.selectedWindow,b);
        auto *third=app.open(); QVERIFY(third); QTest::qWait(100);
        NSWindow *c=reinterpret_cast<NSView *>(third->winId()).window;
        QVERIFY(![a.tabbedWindows containsObject:c]);
        app.tabAction("merge"); QTest::qWait(150); QCOMPARE(c.tabbedWindows.count,NSUInteger(3));
        app.tabAction("detach"); QTest::qWait(150); QVERIFY(c.tabbedWindows.count<=1);
        app.activate(first->property("macDocumentWindowId").toLongLong());
        app.tabAction("new"); QTest::qWait(150); QCOMPARE(a.tabbedWindows.count,NSUInteger(3));
        auto *last=app.activeWindow(); QMetaObject::invokeMethod(last,"approveClose"); QTRY_COMPARE(app.windows().size(),3);
        QCOMPARE(a.tabbedWindows.count,NSUInteger(2));
        QVERIFY([NSApp.windowsMenu itemWithTitle:@"Show Next Tab"]);
        QVERIFY([NSApp.windowsMenu itemWithTitle:@"Move Tab to New Window"]);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void nativeTabsRecover() {
        QTemporaryDir directory;
        {
            MacApplication app(directory.path()); app.start({},true);
            app.activeWindow()->findChild<MindCanvas *>("mindCanvas")->commitEditing("One");
            app.tabAction("new"); app.activeWindow()->findChild<MindCanvas *>("mindCanvas")->commitEditing("Two");
            QVERIFY(app.requestQuit()); QTRY_COMPARE(app.windows().size(),0);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
        {
            MacApplication app(directory.path()); app.start({},false);
            QCOMPARE(app.windows().size(),2); app.updateTabs();
            QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),2);
            NSWindow *native=reinterpret_cast<NSView *>(app.activeWindow()->winId()).window;
            QCOMPARE(native.tabbedWindows.count,NSUInteger(2));
            while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
        }
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
            first->findChild<MindCanvas *>("mindCanvas")->commitEditing("Central idea");
            firstEngine->setText(1,"Saved map"); QVERIFY(firstEngine->save(saved));
            auto *second=app.open(); QVERIFY(second); QVERIFY(second!=first);
            QTest::qWait(150);
            auto *secondEngine=app.activeDocument();
            second->findChild<MindCanvas *>("mindCanvas")->commitEditing("Central idea");
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
