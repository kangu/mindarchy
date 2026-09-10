#include "macapplication.h"
#include "canvas.h"
#include "shelltheme.h"
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
static QString testThemeDirectory;
class ApplicationTest : public QObject {
    Q_OBJECT
private slots:
    void groupsCycleDetachCloseAndRecover() {
        QTemporaryDir directory;
        testThemeDirectory=directory.filePath("colors"); QDir().mkpath(testThemeDirectory+"/theme");
        auto theme=[&](QString bg) { QFile file(testThemeDirectory+"/theme/colors.toml"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(("background = \""+bg+"\"\nforeground = \"#eeeeee\"\naccent = \"#aacc88\"\n").toUtf8()); };
        theme("#332222");
        {
            MacApplication app(directory.path()); app.start({},true);
            auto *first=app.activeWindow(); QVERIFY(first);
            auto *firstCanvas=first->findChild<MindCanvas *>("mindCanvas");
            QTRY_VERIFY(firstCanvas->editing()); QVERIFY(firstCanvas->commitEditing("First"));
            const auto firstId=first->property("macDocumentWindowId").toLongLong();
            first->requestActivate(); firstCanvas->forceActiveFocus(); QTRY_VERIFY(first->isActive());
            QTest::keyClick(first,Qt::Key_T,Qt::ControlModifier); QTRY_COMPARE(app.windows().size(),2);
            auto *second=app.activeWindow(); QVERIFY(second!=first);
            auto *secondCanvas=second->findChild<MindCanvas *>("mindCanvas");
            QTRY_VERIFY(secondCanvas->editing()); QVERIFY(secondCanvas->commitEditing("Second"));
            QCOMPARE(second->property("documentTabs").toList().size(),2);
            auto *strip=second->findChild<QQuickItem *>("documentTabStrip"); QVERIFY(strip);
            QCOMPARE(strip->property("color").value<QColor>(),QColor("#332222"));
            theme("#223344"); QTRY_COMPARE(strip->property("color").value<QColor>(),QColor("#223344"));
            auto *help=second->findChild<QQuickWindow *>("windowsKeyboardShortcuts"); QVERIFY(help);
            QTRY_COMPARE(help->color(),QColor("#223344"));
            second->requestActivate(); second->findChild<MindCanvas *>("mindCanvas")->forceActiveFocus();
            QTRY_VERIFY(second->isActive());
            if(qEnvironmentVariableIsSet("MINDARCHY_TABS_EVIDENCE")) QVERIFY(second->grabWindow().save(qEnvironmentVariable("MINDARCHY_TABS_EVIDENCE")));
            QTest::keyClick(second,Qt::Key_Tab,Qt::ControlModifier); QTRY_COMPARE(app.activeWindow(),first);
            first->requestActivate(); first->findChild<MindCanvas *>("mindCanvas")->forceActiveFocus();
            QTRY_VERIFY(first->isActive());
            QTest::keyClick(first,Qt::Key_Tab,Qt::ControlModifier|Qt::ShiftModifier); QTRY_COMPARE(app.activeWindow(),second);
            QVERIFY(second->isVisible()); QVERIFY(!first->isVisible());
            app.tabAction("next"); QCOMPARE(app.activeWindow(),first); QVERIFY(first->isVisible()); QVERIFY(!second->isVisible());
            app.tabAction("previous"); QCOMPARE(app.activeWindow(),second);
            second->requestActivate(); secondCanvas->forceActiveFocus(); QTRY_VERIFY(second->isActive());
            QTest::keyClick(second,Qt::Key_N,Qt::ControlModifier); QTRY_COMPARE(app.windows().size(),3);
            auto *third=app.activeWindow(); QVERIFY(third!=first && third!=second);
            QTRY_VERIFY(third->findChild<MindCanvas *>("mindCanvas")->editing());
            QVERIFY(third->findChild<MindCanvas *>("mindCanvas")->commitEditing("Third"));
            QVERIFY(third->isVisible()); QVERIFY(second->isVisible());
            app.tabAction("merge"); QCOMPARE(third->property("documentTabs").toList().size(),3);
            QVERIFY(!first->isVisible()); QVERIFY(!second->isVisible());
            app.tabAction("detach"); QCOMPARE(third->property("documentTabs").toList().size(),1);
            app.activate(firstId); app.tabAction("new");
            auto *fourth=app.activeWindow(); QMetaObject::invokeMethod(fourth,"approveClose"); QTRY_COMPARE(app.windows().size(),3);
            app.activate(firstId); QCOMPARE(first->property("documentTabs").toList().size(),2);
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
            QCOMPARE(grouped,2); QCOMPARE(visible,2);
            while(!restored.windows().isEmpty()) { QMetaObject::invokeMethod(restored.activeWindow(),"approveClose"); QTest::qWait(30); }
        }
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv); QQuickStyle::setStyle("Basic");
    ShellTheme shell; qmlRegisterSingletonType<ShellTheme>("Mindarchy",1,0,"ShellTheme",[](QQmlEngine *,QJSEngine *) -> QObject * { return new ShellTheme(testThemeDirectory); });
    qmlRegisterUncreatableType<Engine>("Mindarchy",1,0,"Engine","Provided by application");
    qmlRegisterType<MindCanvas>("Mindarchy",1,0,"MindCanvas");
    ApplicationTest test; return QTest::qExec(&test,argc,argv);
}
#include "application_test.moc"
