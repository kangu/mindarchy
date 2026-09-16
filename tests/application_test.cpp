#include "macapplication.h"
#include "canvas.h"
#include "shelltheme.h"
#include "tabshortcuts.h"
#include <QQuickStyle>
#include <QQmlContext>
#include <QQuickWindow>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QtTest>
#include <QTextDocument>
static QString testThemeDirectory;
class ApplicationTest : public QObject {
    Q_OBJECT
private slots:
    void shortcutMapping_data() {
        QTest::addColumn<bool>("mac");
        QTest::newRow("Omarchy-Linux-Windows") << false;
        QTest::newRow("macOS") << true;
    }
    void shortcutMapping() {
        QFETCH(bool,mac);
        const auto cycle=mac?Qt::MetaModifier:Qt::ControlModifier;
        QCOMPARE(TabShortcuts::action(Qt::Key_T,Qt::ControlModifier,mac),QString("new"));
        QCOMPARE(TabShortcuts::action(Qt::Key_N,Qt::ControlModifier,mac),QString("window"));
        QCOMPARE(TabShortcuts::action(Qt::Key_W,Qt::ControlModifier,mac),QString("close"));
        QCOMPARE(TabShortcuts::action(Qt::Key_Tab,cycle,mac),QString("next"));
        QCOMPARE(TabShortcuts::action(Qt::Key_Tab,cycle|Qt::ShiftModifier,mac),QString("previous"));
        QCOMPARE(TabShortcuts::action(Qt::Key_Backtab,cycle|Qt::ShiftModifier,mac),QString("previous"));
        QVERIFY(TabShortcuts::action(Qt::Key_Tab,Qt::NoModifier,mac).isEmpty());
        QVERIFY(TabShortcuts::action(Qt::Key_T,Qt::ControlModifier|Qt::AltModifier,mac).isEmpty());
        if(mac) QVERIFY(TabShortcuts::action(Qt::Key_Tab,Qt::ControlModifier,mac).isEmpty());
        QCOMPARE(TabShortcuts::help(mac).size(),5);
    }
    void shortcutsFromFocusedEditors_data() {
        QTest::addColumn<QString>("target");
        for(const char *name:{"mindCanvas","titleEditor","notesEditor","mindmapSearchField","outlineSidebar","inspectorSidebar"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void shortcutsFromFocusedEditors() {
        QFETCH(QString,target);
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true); QTest::qWait(100);
        auto *host=app.activeWindow(); auto *first=app.activeDocument();
        auto *surface=app.activeWorkspace();
        const auto firstId=host->property("documentTabId").toLongLong();
        app.tabAction("new"); QTest::qWait(100); auto *second=app.activeDocument();
        app.activate(firstId); host->setProperty("outlineVisible",true); host->setProperty("inspectorVisible",true);
        auto *canvas=surface->findChild<MindCanvas *>("mindCanvas");
        if(target=="titleEditor") { canvas->beginEdit(1); surface->findChild<QQuickItem *>(target)->setProperty("text","Edited first"); }
        if(target=="notesEditor") surface->findChild<QQuickItem *>(target)->setProperty("text","Uncommitted notes");
        if(target=="mindmapSearchField") surface->setProperty("searchOpen",true);
        auto *focus=surface->findChild<QQuickItem *>(target); QVERIFY(focus);
        focus->forceActiveFocus(); QTRY_VERIFY(focus->hasActiveFocus());
        const auto cycle=TabShortcuts::nativeMac?Qt::MetaModifier:Qt::ControlModifier;
        QTest::keyClick(host,Qt::Key_Tab,cycle); QCOMPARE(app.activeDocument(),second);
        QTest::keyClick(host,Qt::Key_Backtab,cycle|Qt::ShiftModifier); QCOMPARE(app.activeDocument(),first);
        if(target=="titleEditor") { QTextDocument text; text.setHtml(first->selectedText()); QCOMPARE(text.toPlainText(),QString("Edited first")); }
        if(target=="notesEditor") QCOMPARE(focus->property("text").toString(),QString("Uncommitted notes"));
        focus->forceActiveFocus();
        QTest::keyClick(host,Qt::Key_T,Qt::ControlModifier); QCOMPARE(app.windows().size(),3);
        QTest::qWait(100); QTest::keyClick(host,Qt::Key_W,Qt::ControlModifier); QTRY_COMPARE(app.windows().size(),2);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void rejectedEditAndModalBlockTabCommands() {
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true); QTest::qWait(100);
        app.tabAction("new"); QTest::qWait(100);
        auto *host=app.activeWindow(); auto *document=app.activeDocument(); auto *surface=app.activeWorkspace();
        auto *canvas=surface->findChild<MindCanvas *>("mindCanvas");
        auto *editor=surface->findChild<QQuickItem *>("titleEditor");
        canvas->beginEdit(1); editor->setProperty("text",QString(17000,'x')); editor->forceActiveFocus();
        const auto cycle=TabShortcuts::nativeMac?Qt::MetaModifier:Qt::ControlModifier;
        QTest::keyClick(host,Qt::Key_Tab,cycle); QCOMPARE(app.activeDocument(),document);
        QTest::keyClick(host,Qt::Key_T,Qt::ControlModifier); QCOMPARE(app.windows().size(),2);
        QVERIFY(canvas->editing());
        editor->setProperty("text","Valid title"); QVERIFY(QMetaObject::invokeMethod(surface,"commitForTabSwitch"));
        auto *dialog=surface->findChild<QObject *>("closeConfirmation"); QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
        QTest::keyClick(host,Qt::Key_Tab,cycle); QCOMPARE(app.activeDocument(),document);
        QTest::keyClick(host,Qt::Key_T,Qt::ControlModifier); QCOMPARE(app.windows().size(),2);
        QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
        surface->setProperty("nativeDialogPending",true);
        QVERIFY(app.requestQuit()); QVERIFY(!app.quitting());
        surface->setProperty("nativeDialogPending",false);
        QTest::keyClick(host,Qt::Key_Tab,cycle); QVERIFY(app.activeDocument()!=document);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void closingTabSelectsNearestLeft_data() {
        QTest::addColumn<int>("closedIndex");
        QTest::addColumn<bool>("reorder");
        QTest::newRow("first-falls-right") << 0 << false;
        QTest::newRow("middle-selects-left") << 2 << false;
        QTest::newRow("last-selects-left") << 3 << false;
        QTest::newRow("respects-reordered-tabs") << 2 << true;
    }
    void closingTabSelectsNearestLeft() {
        QFETCH(int,closedIndex); QFETCH(bool,reorder);
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true);
        auto *host=app.activeWindow();
        QList<qint64> ids;
        for(int i=0;i<4;++i) {
            if(i) app.tabAction("new");
            QTest::qWait(50);
            ids.append(host->property("documentTabId").toLongLong());
        }
        if(reorder) {
            QMetaObject::invokeMethod(host,"tabMoveRequested",Q_ARG(double,double(ids.last())),Q_ARG(int,1));
            ids.move(3,1);
        }
        // Another window must never become a neighbor in this group.
        QVERIFY(app.open()!=host);
        app.activate(ids[closedIndex]);
        const auto expected=ids[closedIndex ? closedIndex-1 : 1];
        app.tabAction("close");
        QTRY_COMPARE(app.windows().size(),4);
        QCOMPARE(app.activeWindow(),host);
        QCOMPARE(host->property("documentTabId").toLongLong(),expected);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void switchingTabsDoesNotResizeHost() {
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true); QTest::qWait(100);
        auto *host=app.activeWindow(); QVERIFY(host);
        const auto firstId=host->property("documentTabId").toLongLong();
        host->resize(1112,743); QTest::qWait(50);
        const auto expected=host->geometry();
        const auto state=host->windowState();
        app.tabAction("new"); QTest::qWait(100);
        QCOMPARE(app.activeWindow(),host);
        QCOMPARE(host->geometry(),expected);
        QCOMPARE(host->windowState(),state);
        const auto secondId=host->property("documentTabId").toLongLong();
        QVERIFY(secondId!=firstId);
        app.activate(firstId); QTest::qWait(50);
        QCOMPARE(app.activeWindow(),host);
        QCOMPARE(host->property("documentTabId").toLongLong(),firstId);
        QCOMPARE(host->geometry(),expected);
        QCOMPARE(host->windowState(),state);
        app.activate(secondId); QTest::qWait(50);
        QCOMPARE(host->property("documentTabId").toLongLong(),secondId);
        QCOMPARE(host->geometry(),expected);
        QCOMPARE(host->windowState(),state);
#ifdef Q_OS_MACOS
        if(QGuiApplication::platformName()=="cocoa") {
            host->showMaximized(); QTRY_VERIFY(host->windowState()==Qt::WindowMaximized || host->visibility()==QWindow::Maximized);
            const auto maximized=host->geometry();
            app.activate(firstId); QTest::qWait(50);
            QVERIFY(host->windowState()==Qt::WindowMaximized || host->visibility()==QWindow::Maximized);
            QCOMPARE(host->geometry(),maximized);
            host->showNormal(); QTest::qWait(50);
        }
#endif
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void inactiveDetachAndCancelledClose() {
        QTemporaryDir directory;
        MacApplication app(directory.path()); app.start({},true);
        QTest::qWait(150);
        auto *first=app.activeDocument();
        auto firstId=app.activeWindow()->property("documentTabId").toLongLong();
        app.tabAction("new"); QTest::qWait(100);
        auto *second=app.activeDocument();
        auto secondId=app.activeWindow()->property("documentTabId").toLongLong();
        app.activate(firstId);
        app.tabAction("detach",secondId);
        QCOMPARE(app.activeDocument(),second);
        QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),1);
        app.tabAction("merge");
        app.activate(firstId);
        QQmlEngine::contextForObject(app.activeWorkspace())->parentContext()->setContextProperty("nativeCloseAvailable",false);
        app.activate(secondId);
        QQmlEngine::contextForObject(app.activeWorkspace())->parentContext()->setContextProperty("nativeCloseAvailable",false);
        app.activate(firstId);
        second->setText(1,"Unsaved second");
        app.tabAction("close",secondId);
        QCOMPARE(app.activeDocument(),second);
        // Cancel the dialog through the document, retaining both docs and the
        // previously active map. Use QML confirmation for deterministic testing.
        QMetaObject::invokeMethod(app.activeWorkspace(),"cancelClose");
        QTRY_COMPARE(app.activeDocument(),first);
        QCOMPARE(app.windows().size(),2);
        while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
    }
    void reorderedTabsRecoverInOrder() {
        QTemporaryDir directory;
        QStringList expected;
        {
            MacApplication app(directory.path()); app.start({},true);
            for(int i=0;i<3;++i) {
                if(i) app.tabAction("new");
                QTest::qWait(100);
                app.activeWorkspace()->findChild<MindCanvas *>("mindCanvas")->commitEditing(QString("Map %1").arg(i));
                QVERIFY(app.activeDocument()->save(directory.filePath(QString("Map %1.omm").arg(i))));
            }
            auto *host=app.activeWindow();
            QMetaObject::invokeMethod(host,"tabMoveRequested",Q_ARG(double,host->property("documentTabId").toDouble()),Q_ARG(int,0));
            for(const auto &entry:host->property("documentTabs").toList()) expected.append(entry.toMap().value("title").toString());
            QCOMPARE(expected.first(),QString("Map 2"));
            QVERIFY(app.requestQuit()); QTRY_COMPARE(app.windows().size(),0);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
        {
            MacApplication app(directory.path()); app.start({},false);
            QStringList actual;
            for(const auto &entry:app.activeWindow()->property("documentTabs").toList()) actual.append(entry.toMap().value("title").toString());
            QCOMPARE(actual,expected);
            QCOMPARE(app.activeDocument()->documentName(),QString("Map 2"));
            while(!app.windows().isEmpty()) { QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTest::qWait(30); }
        }
    }
    void groupsCycleDetachCloseAndRecover() {
        QTemporaryDir directory;
        testThemeDirectory=directory.filePath("colors"); QDir().mkpath(testThemeDirectory+"/theme");
        {
            MacApplication app(directory.path()); app.start({},true);
            auto *host=app.activeWindow(); QVERIFY(host);
            auto *firstSurface=app.activeWorkspace();
            auto *firstCanvas=firstSurface->findChild<MindCanvas *>("mindCanvas");
            QTRY_VERIFY(firstCanvas->editing()); QVERIFY(firstCanvas->commitEditing("First"));
            const auto firstId=host->property("documentTabId").toLongLong();
            auto *firstEngine=app.activeDocument();
            QTest::keyClick(host,Qt::Key_T,Qt::ControlModifier); QTRY_COMPARE(app.windows().size(),2);
            QCOMPARE(app.activeWindow(),host);
            auto *secondSurface=app.activeWorkspace(); QVERIFY(secondSurface!=firstSurface);
            auto *secondCanvas=secondSurface->findChild<MindCanvas *>("mindCanvas");
            QTRY_VERIFY(secondCanvas->editing()); QVERIFY(secondCanvas->commitEditing("Second"));
            auto *secondEngine=app.activeDocument();
            QCOMPARE(host->property("documentTabs").toList().size(),2);
            const auto cycle=TabShortcuts::nativeMac?Qt::MetaModifier:Qt::ControlModifier;
            QTest::keyClick(host,Qt::Key_Tab,cycle); QCOMPARE(app.activeDocument(),firstEngine);
            QTest::keyClick(host,Qt::Key_Backtab,cycle|Qt::ShiftModifier); QCOMPARE(app.activeDocument(),secondEngine);
            QCOMPARE(app.activeWindow(),host); QVERIFY(host->isVisible());
            // Reordering changes navigation order without replacing documents.
            const auto secondId=host->property("documentTabId").toDouble();
            QMetaObject::invokeMethod(host,"tabMoveRequested",Q_ARG(double,secondId),Q_ARG(int,0));
            QCOMPARE(host->property("documentTabs").toList().first().toMap().value("id").toDouble(),secondId);
            QCOMPARE(app.activeDocument(),secondEngine);
            QTest::keyClick(host,Qt::Key_N,Qt::ControlModifier); QTRY_COMPARE(app.windows().size(),3);
            auto *third=app.activeWindow(); QVERIFY(third!=host);
            auto *thirdSurface=app.activeWorkspace();
            auto *thirdCanvas=thirdSurface->findChild<MindCanvas *>("mindCanvas");
            QTRY_VERIFY(thirdCanvas->editing()); QVERIFY(thirdCanvas->commitEditing("Third"));
            app.tabAction("merge"); QCOMPARE(third->property("documentTabs").toList().size(),3);
            app.tabAction("detach"); QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),1);
            QCOMPARE(app.activeWorkspace(),thirdSurface);
            app.activate(firstId); QCOMPARE(app.activeWorkspace(),firstSurface);
            app.tabAction("new");
            QMetaObject::invokeMethod(app.activeWindow(),"approveClose"); QTRY_COMPARE(app.windows().size(),3);
            app.activate(firstId); QCOMPARE(app.activeWindow()->property("documentTabs").toList().size(),2);
            QVERIFY(app.requestQuit()); QTRY_COMPARE(app.windows().size(),0);
            QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        }
        {
            MacApplication restored(directory.path()); restored.start({},false);
            QCOMPARE(restored.windows().size(),3);
            int grouped=0,visible=0;
            for(auto *window:QGuiApplication::allWindows()) if(!window->property("macDocumentWindowId").isNull()) {
                if(window->property("documentTabs").toList().size()==2) ++grouped;
                if(window->isVisible()) ++visible;
            }
            QCOMPARE(grouped,1); QCOMPARE(visible,2);
            while(!restored.windows().isEmpty()) { QMetaObject::invokeMethod(restored.activeWindow(),"approveClose"); QTest::qWait(30); }
        }
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv); QQuickStyle::setStyle("Basic");
    qmlRegisterSingletonType<ShellTheme>("Mindarchy",1,0,"ShellTheme",[](QQmlEngine *,QJSEngine *) -> QObject * { return new ShellTheme(testThemeDirectory); });
    qmlRegisterUncreatableType<Engine>("Mindarchy",1,0,"Engine","Provided by application");
    qmlRegisterType<MindCanvas>("Mindarchy",1,0,"MindCanvas");
    ApplicationTest test; return QTest::qExec(&test,argc,argv);
}
#include "application_test.moc"
