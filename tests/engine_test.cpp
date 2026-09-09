#include "../src/engine.h"
#include "../src/documentsession.h"
#include <QFile>
#include <QProcess>
#include <cstdlib>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextCursor>
#include <QtTest>
#include <cmath>
class EngineTest : public QObject {
    Q_OBJECT
  private slots:
    void fuzzySearchFindsFoldedTitlesNotesAndDates() {
        Engine e(nullptr,Engine::InitialContent::Blank);
        e.setText(1,"Café planning"); e.addChild(); const int child=e.selectedId(); e.setText(child,"Meeting discussion");
        e.setNotes("Quarterly budget"); e.select(1); e.toggleFold();
        QCOMPARE(e.searchNodes("CAFE").first().toInt(),1);
        QVERIFY(e.searchNodes("meetin").contains(child));
        QVERIFY(e.searchNodes("meting").contains(child));
        QVERIFY(e.searchNodes("mtg").contains(child));
        QVERIFY(e.searchNodes("budget").contains(child));
        QVERIFY(e.searchNodes("xyz987missing").isEmpty()); QVERIFY(e.searchNodes(" ").isEmpty());
        QVERIFY(e.revealSearchNode(child)); QVERIFY(!e.nodes().value(1).folded); QCOMPARE(e.selectedId(),child);
        QVERIFY(!e.revealSearchNode(99999));
        e.setNodeKind(child,"date"); QVERIFY(e.setDateEntry(child,"2026-09-09","Revenue report"));
        QVERIFY(e.searchNodes("revenue").contains(child));
    }
    void weeklyTemplateCalendarAndAtomicInsertion() {
        Engine e(nullptr,Engine::InitialContent::Blank);
        auto weeks=e.templateCalendar("2021-01-01").value("weeks").toList();
        QCOMPARE(weeks.first().toMap().value("number").toInt(),53);
        QCOMPARE(weeks.first().toMap().value("year").toInt(),2020);
        QCOMPARE(weeks.first().toMap().value("monday").toString(),QString("2020-12-28"));
        QVERIFY(e.templateCalendar("invalid").isEmpty());
        QVERIFY(!e.addNodeTemplate("unknown","2026-09-07")); QCOMPARE(e.nodeCount(),1);
        QVERIFY(!e.addNodeTemplate("weekly-tasks","invalid")); QCOMPARE(e.nodeCount(),1);
        QVERIFY(e.addNodeTemplate("weekly-tasks","2026-09-09"));
        QCOMPARE(e.nodeCount(),12);
        const int week=e.nodes().value(1).children.first();
        QVERIFY(e.nodes().value(week).text.contains("Week 37"));
        const auto days=e.nodes().value(week).children; QCOMPARE(days.size(),5);
        for(int i=0;i<5;++i) {
            const auto day=e.nodes().value(days[i]);
            QCOMPARE(day.text,QDate(2026,9,7).addDays(i).toString("dddd · d MMM"));
            QCOMPARE(day.children.size(),1); QVERIFY(day.task); QVERIFY(!day.checked);
            const auto task=e.nodes().value(day.children.first());
            QVERIFY(task.task); QVERIFY(!task.checked); QVERIFY(task.text.isEmpty());
            QCOMPARE(task.parent,day.id);
        }
        QCOMPARE(e.selectedId(),e.nodes().value(days.first()).children.first());
        for(int day:days) { e.select(e.nodes().value(day).children.first()); e.toggleChecked(); }
        QVERIFY(e.nodes().value(week).checked);
        QTemporaryDir dir; const auto path=dir.filePath("week.omm"); QVERIFY(e.save(path));
        Engine reopened; QVERIFY(reopened.open(path)); QCOMPARE(reopened.nodeCount(),12);
        QVERIFY(reopened.nodes().value(week).checked);
        for(int i=0;i<5;++i) e.undo();
        e.undo(); QCOMPARE(e.nodeCount(),1);
        e.redo(); QCOMPARE(e.nodeCount(),12);
        QVERIFY(!e.nodes().value(week).checked);
    }
    void pointerChildInheritsTaskAndPersistsMirroredPosition() {
        Engine engine(nullptr,Engine::InitialContent::Blank);
        engine.setManual(true); engine.toggleTask();
        const QPointF left(-200,100);
        engine.addChildFromPointer(1,left);
        const int parent=engine.selectedId();
        QVERIFY(engine.selectedTask()); QCOMPARE(engine.nodes().value(parent).rect.center(),left);
        engine.addChildFromPointer(parent,QPointF(-400,100));
        engine.select(parent); engine.toggleFold();
        const QPointF target(-450,220);
        engine.addChildFromPointer(parent,target);
        const int child=engine.selectedId();
        QVERIFY(engine.selectedTask()); QVERIFY(!engine.nodes().value(parent).folded);
        QVERIFY(QLineF(engine.nodes().value(child).rect.center(),target).length()<.001);
        engine.undo(); QVERIFY(!engine.nodes().contains(child)); QVERIFY(engine.nodes().value(parent).folded);
        engine.redo(); QVERIFY(engine.nodes().contains(child));
        QTemporaryDir dir; const auto path=dir.filePath("pointer.omm");
        QVERIFY(engine.save(path)); Engine reopened; QVERIFY(reopened.open(path));
        QVERIFY(QLineF(reopened.nodes().value(child).rect.center(),target).length()<.001);
    }
    void meetingTemplatePreservesChildrenAndPersists() {
        Engine e(nullptr,Engine::InitialContent::Blank);
        e.setText(1,"Product review"); e.addChild(); const int existing=e.selectedId(); e.setText(existing,"Existing note"); e.select(1);
        QVERIFY(e.addNodeTemplate("meeting-notes", "")); const int draft=e.selectedId();
        const int meeting=e.nodes().value(1).children.last();
        QCOMPARE(e.nodeCount(),11);
        QCOMPARE(e.nodes().value(1).text,QString("Product review"));
        QCOMPARE(e.nodes().value(1).children,QVector<int>({existing,meeting}));
        QVERIFY(e.nodes().value(1).meeting.isEmpty());
        QCOMPARE(e.nodes().value(meeting).text,QString("Meeting Notes"));
        QVERIFY(e.nodes().value(draft).text.isEmpty()); QCOMPARE(e.selectedEntryPrompt(),QString("Capture a note…"));
        const auto sections=e.nodes().value(meeting).children;
        QCOMPARE(sections.size(),4); QCOMPARE(e.nodes().value(existing).parent,1);
        for(int section:sections) QCOMPARE(e.nodes().value(section).children.size(),1);
        QVERIFY(e.nodes().value(sections[3]).task);
        e.undo(); QCOMPARE(e.nodeCount(),2); QCOMPARE(e.nodes().value(1).children,QVector<int>({existing}));
        e.redo(); QCOMPARE(e.nodes().value(meeting).children,sections);
        e.select(meeting); QVERIFY(e.updateMeeting("2026-09-08","14:30","Alex, Sam"));
        QVERIFY(!e.updateMeeting("not a date","",""));
        QTemporaryDir dir; const auto path=dir.filePath("meeting.omm"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path)); loaded.select(meeting);
        QCOMPARE(loaded.selectedMeeting(),e.selectedMeeting()); QVERIFY(!loaded.selectedTask());
        loaded.select(sections[3]); loaded.addChild(); QVERIFY(loaded.selectedTask()); QVERIFY(loaded.selectedText().isEmpty());
        QVERIFY(!loaded.nodes().value(meeting).task);
        loaded.select(1); const int count=loaded.nodeCount(); QVERIFY(loaded.addNodeTemplate("meeting-notes",""));
        QCOMPARE(loaded.nodeCount(),count+9); QCOMPARE(loaded.nodes().value(1).children.size(),3);
        loaded.undo(); QCOMPARE(loaded.nodeCount(),count);
    }
    void nestedTaskProgressSurvivesSaveAndStructuralChanges() {
        Engine e(nullptr,Engine::InitialContent::Blank);
        e.addChild(); const int parent=e.selectedId();
        e.addChild(); const int first=e.selectedId();
        e.addSibling(); const int second=e.selectedId();
        e.select(1); e.toggleTask();
        e.select(first); e.toggleChecked();
        QCOMPARE(e.nodes().value(parent).completedTaskChildren,1);
        QCOMPARE(e.nodes().value(parent).taskChildren,2);
        QVERIFY(!e.nodes().value(1).checked);
        e.select(second); e.toggleChecked();
        e.select(parent); e.toggleFold();
        QVERIFY(e.nodes().value(parent).checked); QVERIFY(e.nodes().value(1).checked);
        QTemporaryDir dir; const auto path=dir.filePath("tasks.omm");
        QVERIFY(e.save(path)); Engine loaded; QVERIFY(loaded.open(path));
        QVERIFY(loaded.nodes().value(1).checked);
        QCOMPARE(loaded.nodes().value(parent).completedTaskChildren,2);
        loaded.select(parent); loaded.toggleFold(); loaded.addChild();
        QVERIFY(loaded.selectedTask()); QVERIFY(!loaded.nodes().value(1).checked);
        loaded.removeSelected(); QVERIFY(loaded.nodes().value(1).checked);
    }
    void taskBranchesCascadeAndAggregate() {
        Engine e; e.loadFixture(15);
        const int parent=e.nodes().value(1).children.first();
        e.select(parent); e.toggleTask();
        const auto children=e.nodes().value(parent).children;
        for(int child:children) QVERIFY(e.nodes().value(child).task);
        QVERIFY(!e.nodes().value(parent).checked);
        for(int i=0;i<children.size();++i) {
            e.select(children[i]); e.toggleChecked();
            QCOMPARE(e.nodes().value(parent).checked,i==children.size()-1);
        }
        e.select(parent); e.toggleChecked();
        QVERIFY(e.nodes().value(parent).checked);
        e.select(children.first()); e.toggleChecked();
        QVERIFY(!e.nodes().value(parent).checked);
        e.undo(); QVERIFY(e.nodes().value(parent).checked);
        e.select(parent); e.toggleTask();
        for(int child:children) QVERIFY(!e.nodes().value(child).task);
        e.undo(); QVERIFY(e.nodes().value(parent).checked);
    }
    void roundedDateNodesHaveRoundedCornersAcrossThemes() {
        Engine e; e.loadFixture(15);
        const int branch=e.nodes().value(1).children.first();
        const int child=e.nodes().value(branch).children.first();
        e.setThemeId("beach-day");
        QVERIFY(e.setNodeKind(child,"date"));
        QCOMPARE(e.appearance(child).shape,NodeShape::Rounded);
        QVERIFY(e.appearance(child).radius>0);
        e.select(child);
        QVERIFY(e.applyNodeStyle({{"shape",int(NodeShape::Rounded)}}));
        QVERIFY(e.appearance(child).radius>0);
        e.setThemeId("holographic");
        QVERIFY(e.setNodeKind(branch,"date")); e.select(branch);
        QVERIFY(e.applyNodeStyle({{"shape",int(NodeShape::Rounded)}}));
        QVERIFY(e.appearance(branch).radius>0);
        QVERIFY(e.applyNodeStyle({{"shape",int(NodeShape::Rectangle)}}));
        QCOMPARE(e.appearance(branch).shape,NodeShape::Rectangle);
        QCOMPARE(e.appearance(branch).radius,0.);
    }
    void dateNodesSupportEveryExplicitShape() {
        Engine e(nullptr, Engine::InitialContent::Blank);
        QVERIFY(e.setNodeKind(1, "date"));
        const auto calendar=e.selectedCalendar();
        const auto size=e.nodes().value(1).rect.size();
        for (int shape=0; shape<8; ++shape) {
            QVERIFY(e.applyNodeStyle({{"shape",shape}}));
            QCOMPARE(int(e.appearance(1).shape), shape);
            QCOMPARE(e.nodes().value(1).rect.size(), size);
            QCOMPARE(e.selectedCalendar(), calendar);
        }
        QTemporaryDir dir;
        QVERIFY(e.save(dir.filePath("date.omm")));
        Engine loaded; QVERIFY(loaded.open(dir.filePath("date.omm")));
        QCOMPARE(int(loaded.appearance(1).shape), 7);
        QCOMPARE(loaded.selectedCalendar(), calendar);
    }
    void sessionQuitRequiresEveryWindowAndCanCancel() {
        QTemporaryDir dir;
        const QString registry = dir.filePath("session");
        const auto first = dir.filePath("first.omm"), second = dir.filePath("second.omm");
        Engine map(nullptr, Engine::InitialContent::Blank);
        QVERIFY(map.save(first)); QVERIFY(map.save(second));
        using Action = DocumentSession::QuitAction;
        {
            DocumentSession one(registry), two(registry);
            one.setDocument(first); two.setDocument(second);
            one.beginQuit();
            QCOMPARE(one.pollQuit(), Action::Confirm);
            QCOMPARE(two.pollQuit(), Action::None);
            one.voteToQuit(true);
            QCOMPARE(one.pollQuit(), Action::None);
            QCOMPARE(two.pollQuit(), Action::Confirm);
            two.voteToQuit(false);
            QCOMPARE(one.pollQuit(), Action::Cancel);
            QCOMPARE(two.pollQuit(), Action::Cancel);
            one.beginQuit();
            QCOMPARE(one.pollQuit(), Action::Confirm); one.voteToQuit(true);
            DocumentSession lateWindow(registry);
            QCOMPARE(two.pollQuit(), Action::Confirm); two.voteToQuit(true);
            QCOMPARE(one.pollQuit(), Action::None);
            QCOMPARE(lateWindow.pollQuit(), Action::Confirm); lateWindow.voteToQuit(true);
            QCOMPARE(one.pollQuit(), Action::Close);
            QCOMPARE(two.pollQuit(), Action::Close);
            QCOMPARE(lateWindow.pollQuit(), Action::Close);
        }
        QCOMPARE(DocumentSession::restorePaths(registry).size(), 2);
        {
            DocumentSession one(registry);
            one.setDocument(first);
            one.forgetDocument();
        }
        QVERIFY(DocumentSession::restorePaths(registry).isEmpty());
    }
    void documentSessionRestoresMultipleWindows() {
        QTemporaryDir dir;
        const QString registry = dir.filePath("session");
        const QString first = dir.filePath("first.omm"), second = dir.filePath("second.omm");
        Engine map(nullptr, Engine::InitialContent::Blank);
        QVERIFY(map.save(first)); QVERIFY(map.save(second));
        QVERIFY(DocumentSession::restorePaths(registry).isEmpty());
        {
            DocumentSession one(registry);
            one.setDocument(first);
            {
                DocumentSession two(registry);
                two.setDocument(second);
                QCOMPARE(one.liveWindows().size(), 2);
                one.activateWindow(QCoreApplication::applicationPid());
                QVERIFY(two.takeActivation());
                QVERIFY(!one.takeActivation());
                QVERIFY(DocumentSession::restorePaths(registry).isEmpty());
            }
            QVERIFY(DocumentSession::restorePaths(registry).isEmpty());
        }
        auto restored = DocumentSession::restorePaths(registry);
        restored.sort();
        QCOMPARE(restored, QStringList({first, second}));
        // A new blank window must not erase the last saved session.
        { DocumentSession blank(registry); }
        QCOMPARE(DocumentSession::restorePaths(registry).size(), 2);
        QVERIFY(QFile::remove(first));
        QCOMPARE(DocumentSession::restorePaths(registry), QStringList({second}));
        // A stale process entry from a crash is ignored.
        QSettings settings(registry + "/documents.ini", QSettings::IniFormat);
        settings.setValue("windows/terminated-process", second); settings.sync();
        QCOMPARE(DocumentSession::restorePaths(registry), QStringList({second}));
    }
    void unsavedChangesFollowDocumentContent() {
        QTemporaryDir dir;
        Engine e(nullptr, Engine::InitialContent::Blank);
        QCOMPARE(e.documentName(), QString("New mindmap"));
        QVERIFY(!e.edited());
        QVERIFY(!e.hasUnsavedChanges());
        e.setText(1, "Changed new document");
        QVERIFY(e.hasUnsavedChanges());
        e.undo();
        QVERIFY(!e.hasUnsavedChanges());
        const QString path = dir.filePath("map.omm");
        QVERIFY(e.save(path));
        QCOMPARE(e.documentName(), QString("map"));
        QVERIFY(!e.edited());
        QVERIFY(!e.hasUnsavedChanges());
        QCOMPARE(e.documentPath(), path);
        e.select(1);
        QVERIFY(!e.hasUnsavedChanges());
        e.setText(1, "Changed");
        QVERIFY(e.edited());
        QVERIFY(e.hasUnsavedChanges());
        e.undo(); QVERIFY(!e.hasUnsavedChanges()); QVERIFY(!e.edited());
        e.redo(); QVERIFY(e.hasUnsavedChanges());
        QVERIFY(!e.save(dir.filePath("missing/map.omm")));
        QVERIFY(e.hasUnsavedChanges());
        QVERIFY(e.open(path));
        QVERIFY(!e.hasUnsavedChanges());
        e.setNotes("Pending notes");
        QVERIFY(e.hasUnsavedChanges());
    }
    void blankDocumentStartsWithOneEditableRoot() {
        Engine e(nullptr,Engine::InitialContent::Blank);
        QCOMPARE(e.themeId(),QString("beach-day"));
        QVERIFY(!e.hasUnsavedChanges());
        QCOMPARE(e.nodeCount(),1); QCOMPARE(e.selectedId(),1);
        QCOMPARE(e.selectedText(),QString("Central idea"));
        QVERIFY(!e.canUndo()); QVERIFY(!e.canRedo());
        QCOMPARE(e.nodes().value(1).parent,-1);
        e.addChild(); QCOMPARE(e.nodeCount(),2); QCOMPARE(e.selectedId(),2);
        e.undo(); QCOMPARE(e.nodeCount(),1);
    }
    void nodeStylesPersistAndUndoAsOneCommand() {
        Engine e; e.selectMany({2,3});
        const auto original=e.nodes().value(2).text;
        QVERIFY(e.applyNodeStyle({{"shape",7},{"fill",QString("#123456")},{"borderWidth",3.5},
            {"borderStyle",2},{"branchStroke",3},{"branchWidth",4.5},{"width",240},
            {"fontSize",28},{"bold",true},{"alignment",1}}));
        for(int id:{2,3}) {
            QCOMPARE(e.appearance(id).shape,NodeShape::Octagon);
            QCOMPARE(e.appearance(id).fill,QColor("#123456"));
            QCOMPARE(e.nodes().value(id).rect.width(),240.);
        }
        QCOMPARE(e.selectedStyle()["fontSize"].toDouble(),28.);
        QVERIFY(e.selectedStyle()["bold"].toBool());
        QCOMPARE(e.selectedStyle()["alignment"].toInt(),1);
        e.undo(); QCOMPARE(e.nodes().value(2).text,original); QVERIFY(e.nodes().value(3).style.isEmpty());
        e.redo(); QCOMPARE(e.appearance(3).branchStroke,Qt::DotLine);
        QTemporaryDir dir; const auto path=dir.filePath("styles.json"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path));
        QCOMPARE(loaded.nodes().value(2).style,e.nodes().value(2).style);
        QCOMPARE(loaded.nodes().value(2).rect.size(),e.nodes().value(2).rect.size());
        e.setThemeId("retro"); QCOMPARE(e.appearance(2).fill,QColor("#123456"));
        e.resetNodeStyle(); QVERIFY(e.nodes().value(2).style.isEmpty());
        QCOMPARE(e.appearance(2).shape,Themes::appearance("retro",1,0).shape);
        e.undo(); QCOMPARE(e.appearance(2).shape,NodeShape::Octagon);
    }
    void nodeStylesValidateAtomicallyAndReportMixedValues() {
        Engine e; e.select(2); QVERIFY(e.applyNodeStyle({{"fill",QString("#123456")},{"width",180}}));
        e.selectMany({2,3}); QVERIFY(e.selectedStyle()["mixed"].toStringList().contains("fill"));
        const auto before=e.nodes().value(2).style;
        QVERIFY(!e.applyNodeStyle({{"fill",QString("#ffffff")},{"width",-1}}));
        QCOMPARE(e.nodes().value(2).style,before); QVERIFY(e.nodes().value(3).style.isEmpty());
        QVERIFY(!e.applyNodeStyle({{"shape",99}}));
        QVERIFY(!e.applyNodeStyle({{"branchStroke",0}}));
        QVERIFY(!e.applyNodeStyle({{"fontSize",10000}}));
        QTemporaryDir dir; const auto path=dir.filePath("bad.json"); QVERIFY(e.save(path));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); auto json=QJsonDocument::fromJson(file.readAll()).object(); file.close();
        auto nodes=json["nodes"].toArray(); auto node=nodes[0].toObject();
        node["style"]=QJsonObject{{"shape",999}}; nodes[0]=node; json["nodes"]=nodes;
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(json).toJson()); file.close();
        QVERIFY(!e.open(path)); QCOMPARE(e.nodes().value(2).style,before);
    }
    void textColorOverridesRichRunsAndThicknessCanReset() {
        Engine e; e.select(2);
        QVERIFY(e.setText(2,"<span style='color:red;font-size:12pt'>Colored title</span>"));
        QVERIFY(e.applyNodeStyle({{"textColor",QString("#123456")},{"fontSize",30},{"underline",true},{"strike",true},{"branchWidth",7}}));
        QTextDocument doc; doc.setHtml(e.selectedText()); QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
        QCOMPARE(cursor.charFormat().foreground().color(),QColor("#123456"));
        QCOMPARE(e.selectedStyle()["fontSize"].toDouble(),30.);
        QVERIFY(e.selectedStyle()["underline"].toBool()); QVERIFY(e.selectedStyle()["strike"].toBool());
        QVERIFY(!e.selectedStyle()["themeBranchWidth"].toBool());
        e.resetBranchWidth(); QVERIFY(e.selectedStyle()["themeBranchWidth"].toBool());
        QCOMPARE(e.appearance(2).text,QColor("#123456"));
        e.undo(); QCOMPARE(e.appearance(2).branchWidth,7.);
    }
    void recoverySurvivesAbruptProcessExit() {
        const auto crashDirectory=qEnvironmentVariable("MINDARCHY_TEST_CRASH_DIRECTORY");
        if(!crashDirectory.isEmpty()) {
            DocumentSession session(crashDirectory);
            Engine map(nullptr,Engine::InitialContent::Blank);
            if(!map.setText(1,"Survived process crash") || !map.saveRecovery(session.recoveryPath(),{})) std::_Exit(2);
            std::_Exit(0); // No destructors, no close handler, stale process/instance entries.
        }
        QTemporaryDir dir; QProcess child;
        auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert("MINDARCHY_TEST_CRASH_DIRECTORY",dir.path()); child.setProcessEnvironment(environment);
        child.start(QCoreApplication::applicationFilePath(),{QString::fromLatin1(QTest::currentTestFunction())});
        QVERIFY(child.waitForFinished(10000)); QCOMPARE(child.exitCode(),0);
        const auto paths=DocumentSession::restorePaths(dir.path(),true); QCOMPARE(paths.size(),1);
        Engine recovered; QVERIFY(recovered.openRecovery(paths.first()));
        QCOMPARE(recovered.selectedText(),QString("Survived process crash")); QVERIFY(recovered.edited());
        DocumentSession resumed(dir.path(),paths.first()); QVERIFY(resumed.locked());
        QVERIFY(DocumentSession::restorePaths(dir.path(),true).isEmpty());
    }
    void sessionRecoversMultipleWindowsWithoutDuplicateOriginals() {
        QTemporaryDir dir; const auto original=dir.filePath("Original.omm");
        const auto registry=dir.filePath("session"); QString first,second;
        Engine saved(nullptr,Engine::InitialContent::Blank),blank(nullptr,Engine::InitialContent::Blank);
        QVERIFY(saved.save(original));
        {
            DocumentSession one(registry),two(registry);
            one.setDocument(original); first=one.recoveryPath(); second=two.recoveryPath();
            QVERIFY(saved.setText(1,"Unsaved saved-file edit")); QVERIFY(saved.saveRecovery(first,{}));
            QVERIFY(blank.setText(1,"Untitled edit")); QVERIFY(blank.saveRecovery(second,{}));
            QVERIFY(DocumentSession::restorePaths(registry,true).isEmpty());
            one.beginQuit(); QCOMPARE(one.pollQuit(),DocumentSession::QuitAction::Confirm); one.voteToQuit(true);
            QCOMPARE(two.pollQuit(),DocumentSession::QuitAction::Confirm); two.voteToQuit(true);
            QCOMPARE(one.pollQuit(),DocumentSession::QuitAction::Close);
            QCOMPARE(two.pollQuit(),DocumentSession::QuitAction::Close);
        }
        auto paths=DocumentSession::restorePaths(registry,true);
        QCOMPARE(paths.size(),2); QVERIFY(paths.contains(first)); QVERIFY(paths.contains(second)); QVERIFY(!paths.contains(original));
        {
            DocumentSession restored(registry,second); QCOMPARE(restored.recoveryPath(),second);
            restored.removeRecovery(); restored.forgetDocument();
        }
        QCOMPARE(DocumentSession::restorePaths(registry,true),QStringList{first});
    }
    void recoveryPreservesIdentityDirtyStateAndOriginal() {
        QTemporaryDir dir;
        const auto original=dir.filePath("Original.omm"), recovery=dir.filePath("window.recovery");
        Engine e(nullptr,Engine::InitialContent::Blank);
        QVERIFY(e.save(original));
        QFile file(original); QVERIFY(file.open(QIODevice::ReadOnly)); const auto disk=file.readAll(); file.close();
        QVERIFY(e.setText(1,"Unsaved changes"));
        const QVariantMap draft{{"text",QString("Still typing")},{"editingId",1}};
        QVERIFY(e.saveRecovery(recovery,draft));
        QCOMPARE(e.documentPath(),original); QVERIFY(e.edited());
        Engine restored(nullptr,Engine::InitialContent::Blank); QVariantMap state;
        QVERIFY(restored.openRecovery(recovery,&state));
        QCOMPARE(restored.documentPath(),original); QCOMPARE(restored.selectedText(),QString("Unsaved changes"));
        QVERIFY(restored.edited()); QCOMPARE(state,draft);
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),disk);
        QVERIFY(!e.saveRecovery(dir.filePath("missing/window.recovery"),{}));
        Engine stillValid; QVERIFY(stillValid.openRecovery(recovery)); QCOMPARE(stillValid.selectedText(),QString("Unsaved changes"));
    }
    void recoveryRestoresUntitledAndCleanDocuments() {
        QTemporaryDir dir; const auto recovery=dir.filePath("window.recovery");
        Engine blank(nullptr,Engine::InitialContent::Blank);
        QVERIFY(blank.saveRecovery(recovery,{}));
        Engine restored; QVERIFY(restored.openRecovery(recovery));
        QVERIFY(restored.documentPath().isEmpty()); QVERIFY(!restored.edited());
        QVERIFY(blank.setText(1,"Untitled work")); QVERIFY(blank.saveRecovery(recovery,{}));
        QVERIFY(restored.openRecovery(recovery)); QVERIFY(restored.edited());
        QCOMPARE(restored.documentName(),QString("New mindmap"));
        QFile broken(recovery); QVERIFY(broken.open(QIODevice::WriteOnly)); broken.write("{}"); broken.close();
        QVERIFY(!restored.openRecovery(recovery)); QCOMPARE(restored.selectedText(),QString("Untitled work"));
    }
    void fixedWidthWrapsDraftWithFinalMetrics() {
        Engine e; e.select(2); QVERIFY(e.applyNodeStyle({{"width",160},{"fontSize",26},{"italic",true}}));
        const QString title="A longer title which wraps across several lines";
        const auto preview=e.previewTextSize(2,title);
        QVERIFY(e.setText(2,title)); QCOMPARE(e.nodes().value(2).rect.size(),preview);
        QCOMPARE(preview.width(),160.); QVERIFY(preview.height()>42);
        QVERIFY(e.applyNodeStyle({{"width",0}})); QVERIFY(e.nodes().value(2).rect.width()>160);
    }
    void manualBranchesMirrorAndPersist() {
        Engine e; e.loadFixture(45); e.setManual(true);
        const int branch=e.nodes().value(1).children.first();
        const int child=e.nodes().value(branch).children.first();
        e.moveManual(child,20,12);
        const auto before=e.nodes();
        const QPointF delta(e.nodes().value(1).rect.center().x()-e.nodes().value(branch).rect.center().x()-150,25);
        const auto preview=e.manualGeometry(branch,delta);
        QCOMPARE(e.nodes().value(branch).rect,before.value(branch).rect);
        QVERIFY(preview.value(child).center().x()<preview.value(branch).center().x());
        QCOMPARE(preview.value(child).center().x()-preview.value(branch).center().x(),
                 -(before.value(child).rect.center().x()-before.value(branch).rect.center().x()));
        e.moveManual(branch,delta.x(),delta.y());
        for(int id:e.visibleIds()) QCOMPARE(e.nodes().value(id).rect,preview.value(id));
        const auto mirrored=e.nodes();
        e.undo(); for(int id:e.visibleIds()) QCOMPARE(e.nodes().value(id).rect,before.value(id).rect);
        e.redo(); QCOMPARE(e.nodes().value(child).rect,mirrored.value(child).rect);
        const QPointF childBefore=e.nodes().value(child).rect.center();
        e.moveManual(child,30,15); QCOMPARE(e.nodes().value(child).rect.center(),childBefore+QPointF(30,15));
        e.undo();
        QTemporaryDir dir; const auto path=dir.filePath("mirrored.json"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path));
        for(int id:e.visibleIds()) QCOMPARE(loaded.nodes().value(id).rect,e.nodes().value(id).rect);
        e.select(branch); e.toggleFold(); e.toggleFold();
        QCOMPARE(e.nodes().value(child).rect,mirrored.value(child).rect);
        e.moveManual(branch,-delta.x(),-delta.y());
        for(int id:e.visibleIds()) QCOMPARE(e.nodes().value(id).rect,before.value(id).rect);
    }
    void nodeTypeConversionPreservesContent() {
        Engine e; e.select(2); e.toggleTask(); const auto original=e.nodes().value(2);
        QVERIFY(e.setNodeKind(2,"date")); QVERIFY(e.nodeCount()>2);
        QCOMPARE(e.nodes().value(2).children,original.children);
        QCOMPARE(e.nodes().value(2).text,original.text); QVERIFY(!e.nodes().value(2).task);
        e.undo(); QVERIFY(e.nodes().value(2).task); QCOMPARE(e.nodes().value(2).kind,QString("text"));
        e.redo(); QVERIFY(e.setDateEntry(2,"2026-09-08","Retained entry"));
        QVERIFY(e.setNodeKind(2,"text")); QCOMPARE(e.nodes().value(2).text,original.text);
        QTemporaryDir dir; QVERIFY(e.save(dir.filePath("types.json")));
        Engine loaded; QVERIFY(loaded.open(dir.filePath("types.json")));
        QVERIFY(loaded.setNodeKind(2,"date")); QCOMPARE(loaded.dateEntry(2,"2026-09-08"),QString("Retained entry"));
    }
    void calendarNumericTotals() {
        CalendarData c; c.view="month"; c.anchor=QDate(2026,9,8);
        const auto plainSize=Calendar::size(c); QVERIFY(!Calendar::totals(c).enabled);
        c.entries={{"2026-09-01","10"},{"2026-09-06","-2.5"},{"2026-09-07","3,25"},
                   {"2026-09-08","meeting"},{"2026-09-30","0"},{"2026-10-01","999"}};
        const auto sums=Calendar::totals(c); QVERIFY(sums.enabled);
        QCOMPARE(sums.weeks.size(),5); QCOMPARE(sums.weeks[0],7.5); QCOMPARE(sums.weeks[1],3.25);
        QCOMPARE(sums.weeks[4],0.); QCOMPARE(sums.month,10.75);
        QVERIFY(Calendar::size(c).width()>plainSize.width()); QCOMPARE(Calendar::size(c).height(),plainSize.height()+36);
        const auto key=Calendar::key(c); c.entries["2026-09-01"]="12"; QVERIFY(Calendar::key(c)!=key);
        c.view="week"; c.anchor=QDate(2026,9,30);
        QCOMPARE(Calendar::totals(c).weeks[0],999.); QCOMPARE(Calendar::size(c).height(),114.);
        double value; QVERIFY(Calendar::numericValue("  +.5  ",value)); QCOMPARE(value,.5);
        QVERIFY(!Calendar::numericValue("12 hours",value)); QVERIFY(!Calendar::numericValue("1,000.50",value));
        QVERIFY(!Calendar::numericValue("NaN",value)); QVERIFY(!Calendar::numericValue("",value));
        Engine e; e.select(2); QVERIFY(e.setNodeKind(2,"date"));
        QVERIFY(e.configureDateNode(2,"month","2026-09-08"));
        QVERIFY(e.setDateEntry(2,"2026-09-08","4")); QCOMPARE(Calendar::totals(e.nodes().value(2).calendar).month,4.);
        e.undo(); QVERIFY(!Calendar::totals(e.nodes().value(2).calendar).enabled);
        e.redo(); QCOMPARE(Calendar::totals(e.nodes().value(2).calendar).month,4.);
    }
    void calendarPeriodsEntriesAndHistory() {
        Engine e; const int count=e.nodeCount(); e.addDateNode("week"); const int id=e.selectedId();
        QCOMPARE(e.nodeCount(),count+1); QCOMPARE(e.nodes().value(id).kind,QString("date"));
        QCOMPARE(e.nodes().value(id).calendar.anchor,QDate::currentDate());
        QVERIFY(e.configureDateNode(id,"week","2025-12-31"));
        auto days=Calendar::days(e.nodes().value(id).calendar);
        QCOMPARE(days.size(),7); QCOMPARE(days.first(),QDate(2025,12,29)); QCOMPARE(days.last(),QDate(2026,1,4));
        QVERIFY(e.setDateEntry(id,"2026-01-01","Start a new project"));
        const auto size=e.nodes().value(id).rect.size();
        QVERIFY(e.setDateEntry(id,"2026-01-01","Revised plan")); QCOMPARE(e.nodes().value(id).rect.size(),size);
        e.undo(); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Start a new project"));
        e.redo(); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QVERIFY(e.configureDateNode(id,"month","2024-02-15"));
        days=Calendar::days(e.nodes().value(id).calendar); QCOMPARE(days.size(),35);
        QVERIFY(days.contains(QDate(2024,2,29))); QVERIFY(!days.first().isValid());
        e.undo(); QCOMPARE(e.nodes().value(id).calendar.view,QString("week"));
        QVERIFY(e.configureDateNode(id,"month","2026-01-01"));
        QVERIFY(e.shiftDateNode(id,1)); QCOMPARE(e.nodes().value(id).calendar.anchor,QDate(2026,2,1));
        QVERIFY(e.shiftDateNode(id,-1)); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QTemporaryDir dir; const auto path=dir.filePath("calendar.json"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path));
        QCOMPARE(loaded.nodes().value(id).calendar.entries,e.nodes().value(id).calendar.entries);
        QCOMPARE(loaded.nodes().value(id).rect.size(),e.nodes().value(id).rect.size());
        QVERIFY(!e.setDateEntry(id,"2026-02-30","Invalid"));
        QVERIFY(!e.setDateEntry(id,"2026-01-01",QString(4097,'x')));
        QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QVERIFY(e.setDateEntry(id,"2026-01-01","")); QVERIFY(e.dateEntry(id,"2026-01-01").isEmpty());
        e.undo(); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); auto json=QJsonDocument::fromJson(file.readAll()).object(); file.close();
        auto nodes=json["nodes"].toArray(); auto n=nodes.last().toObject(); auto c=n["calendar"].toObject();
        c["anchor"]="bad date"; n["calendar"]=c; nodes[nodes.size()-1]=n; json["nodes"]=nodes;
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(json).toJson()); file.close();
        QVERIFY(!loaded.open(path)); QCOMPARE(loaded.dateEntry(id,"2026-01-01"),QString("Revised plan"));
    }
    void seed() {
        Engine e;
        QVERIFY(e.nodeCount() >= 12);
        QCOMPARE(e.nodes().value(1).parent, -1);
        QVERIFY(e.selectedText().contains("Mindarchy"));
        QCOMPARE(e.nodeCount(), e.visibleCount());
    }
    void researchThemeRecipesAreAtomicAndReadable() {
        auto luminance=[](QColor c) {
            auto linear=[](double v) { return v<=.04045 ? v/12.92 : std::pow((v+.055)/1.055,2.4); };
            return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());
        };
        for(const auto &id:QStringList{"canopy","atlas","studio","nocturne"}) {
            Engine e; e.select(2); e.applyNodeStyle({{"fill",QString("#abcdef")}});
            e.setManual(true); const auto original=e.nodes(); const auto oldTheme=e.themeId();
            QSignalSpy changes(&e,&Engine::changed);
            QVERIFY(e.applyThemeRecipe(id)); QCOMPARE(changes.count(),1);
            QCOMPARE(e.themeId(),id); QVERIFY(!e.manual());
            QCOMPARE(e.layout(),Themes::layoutRecipe(id)["layout"].toString());
            for(auto it=original.begin();it!=original.end();++it) {
                QCOMPARE(e.nodes().value(it.key()).text,it->text);
                QCOMPARE(e.nodes().value(it.key()).children,it->children);
                QCOMPARE(e.nodes().value(it.key()).style,it->style);
            }
            e.undo(); QCOMPARE(e.themeId(),oldTheme); QVERIFY(e.manual());
            e.redo(); QCOMPARE(e.themeId(),id);
            QTemporaryDir dir; const auto file=dir.filePath("recipe.json"); QVERIFY(e.save(file));
            Engine loaded; QVERIFY(loaded.open(file)); QCOMPARE(loaded.themeId(),id); QCOMPARE(loaded.layout(),e.layout());
            const QString example=QFINDTESTDATA(qPrintable("../examples/research-templates/"+id+".json"));
            QVERIFY(!example.isEmpty()); Engine starter; QVERIFY(starter.open(example));
            QCOMPARE(starter.themeId(),id); QCOMPARE(starter.nodeCount(),13);
            for(int depth=0;depth<5;++depth) for(int branch=0;branch<6;++branch) {
                const auto a=Themes::appearance(id,depth,branch);
                const double fg=luminance(a.text),bg=luminance(a.fill.alpha()?a.fill:Themes::get(id).canvas);
                QVERIFY2((std::max(fg,bg)+.05)/(std::min(fg,bg)+.05)>=4.5,qPrintable(id));
            }
        }
        Engine invalid; const auto old=invalid.themeId(); QVERIFY(!invalid.applyThemeRecipe("missing"));
        QCOMPARE(invalid.themeId(),old); QCOMPARE(Themes::get("missing").id,QString("lab"));
    }
    void themeCatalogAndAppearance() {
        Engine e;
        QCOMPARE(e.themeId(), QString("lab"));
        const QVariantList themes = e.themes();
        QVERIFY(themes.size() >= 5);
        const QStringList expectedIds{"beach-day", "holographic", "retro", "arcade", "lab", "canopy", "atlas", "studio", "nocturne"};
        for (int i = 0; i < expectedIds.size(); ++i) {
            const QVariantMap entry = themes[i].toMap();
            QCOMPARE(entry.value("id").toString(), expectedIds[i]);
            QVERIFY(entry.value("name").isValid());
            QVERIFY(entry.value("canvas").value<QColor>().isValid());
            QCOMPARE(entry.value("palette").toList().size(), 6);
        }
        QCOMPARE(e.canvasColor(), QColor("#111920"));
        e.setThemeId("beach-day");
        QCOMPARE(e.canvasColor(), QColor("#faf9f6"));
        const NodeAppearance root = e.appearance(1);
        QCOMPARE(root.fill, QColor("#f3ddb4"));
        QCOMPARE(root.shape, NodeShape::Rounded);
        const int branch = e.nodes().value(1).children.first();
        const NodeAppearance child = e.appearance(branch);
        QCOMPARE(child.fill, QColor("#ffffff"));
        QCOMPARE(child.border, QColor("#98a5cc"));
        const int grandchild = e.nodes().value(branch).children.first();
        const NodeAppearance deeper = e.appearance(grandchild);
        QCOMPARE(deeper.shape, NodeShape::Underline);
        QCOMPARE(deeper.fill.alpha(), 0);
        QCOMPARE(deeper.branch, QColor("#98a5cc"));
    }
    void themeUsesTopLevelBranchOrdinalAfterMove() {
        Engine e;
        e.setThemeId("arcade");
        const auto branches = e.nodes().value(1).children;
        const int descendant = e.nodes().value(branches[0]).children.first();
        QCOMPARE(e.appearance(descendant).branch, QColor("#b86cc7"));
        e.moveNode(descendant, branches[1]);
        QCOMPARE(e.appearance(descendant).branch, QColor("#e36b73"));
    }
    void themeHistoryAndPersistenceAreTransactional() {
        Engine e;
        e.setLayout("Vertical");
        e.setThemeId("retro");
        QCOMPARE(e.themeId(), QString("retro"));
        e.undo();
        QCOMPARE(e.themeId(), QString("lab"));
        QCOMPARE(e.layout(), QString("Vertical"));
        e.redo();
        QCOMPARE(e.themeId(), QString("retro"));
        e.setThemeId("unknown");
        QCOMPARE(e.themeId(), QString("retro"));
        QVERIFY(e.error().contains("theme", Qt::CaseInsensitive));

        QTemporaryDir dir;
        const QString path = dir.filePath("themed.json");
        QVERIFY(e.save(path));
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.themeId(), QString("retro"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        json["themeId"] = "missing";
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(json).toJson());
        f.close();
        QVERIFY(!loaded.open(path));
        QCOMPARE(loaded.themeId(), QString("retro"));
        json.remove("themeId");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(json).toJson());
        f.close();
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.themeId(), QString("lab"));
    }
    void editingAndHistory() {
        Engine e;
        int count = e.nodeCount();
        QSignalSpy edit(&e, &Engine::editRequested);
        e.addChild();
        int id = e.selectedId();
        QCOMPARE(e.nodeCount(), count + 1);
        QCOMPARE(e.nodes()[id].parent, 1);
        QCOMPARE(edit.size(), 1);
        e.setText(id, "<b>New idea</b>");
        e.setNotes("Details");
        e.toggleTask();
        e.toggleChecked();
        QVERIFY(e.selectedTask());
        QVERIFY(e.selectedChecked());
        QCOMPARE(e.selectedNotes(), QString("Details"));
        e.undo();
        QVERIFY(!e.selectedChecked());
        e.redo();
        QVERIFY(e.selectedChecked());
        e.addSibling();
        int sibling = e.selectedId();
        QCOMPARE(e.nodes()[sibling].parent, 1);
        e.select(id);
        e.select(sibling, true);
        e.removeSelected();
        QCOMPARE(e.nodeCount(), count);
        e.undo();
        QCOMPARE(e.nodeCount(), count + 2);
    }
    void folding() {
        Engine e;
        int branch = e.nodes()[1].children.first();
        e.select(branch);
        int before = e.visibleCount();
        e.toggleFold();
        QVERIFY(e.visibleCount() < before);
        QVERIFY(e.selectedFolded());
        e.undo();
        QCOMPARE(e.visibleCount(), before);
        e.select(e.nodes()[branch].children.first());
        e.select(branch, true);
        e.toggleFold();
        QVERIFY(e.visibleIds().contains(e.selectedId()));
    }
    void reparenting() {
        Engine e;
        auto branches = e.nodes()[1].children;
        int a = branches[0], b = branches[1], child = e.nodes()[a].children.first();
        e.moveNode(a, child);
        QCOMPARE(e.nodes()[a].parent, 1);
        QVERIFY(!e.error().isEmpty());
        e.moveNode(child, b);
        QCOMPARE(e.nodes()[child].parent, b);
        QVERIFY(!e.nodes()[a].children.contains(child));
        QVERIFY(e.nodes()[b].children.contains(child));
        e.undo();
        QCOMPARE(e.nodes()[child].parent, a);
        e.redo();
        QCOMPARE(e.nodes()[child].parent, b);
        e.moveNode(b, 1, a);
        QCOMPARE(e.nodes()[1].children.first(), b);
        e.moveNode(1, b);
        QCOMPARE(e.nodes()[1].parent, -1);
    }
    void automaticLayoutUsesParentGapWithoutOverlaps() {
        Engine e;
        e.loadFixture(160);
        for(int id : {2,7,19}) QVERIFY(e.setNodeKind(id,"date"));
        for(int id : {3,8,20}) QVERIFY(e.setText(id,QString(100,'W')));
        for(QString layout : {QString("Horizontal"),QString("Vertical")}) {
            e.setLayout(layout);
            for(QString spacing : {QString("Narrow"),QString("Standard"),QString("Wide")}) {
                e.setSpacing(spacing);
                const qreal gap=spacing=="Narrow" ? 36 : spacing=="Wide" ? 104 : 64;
                const auto ids=e.visibleIds();
                for(int i=0;i<ids.size();++i) {
                    const auto node=e.nodes().value(ids[i]);
                    if(node.parent>=0) {
                        const auto parent=e.nodes().value(node.parent).rect;
                        QCOMPARE(layout=="Horizontal" ? node.rect.left()-parent.right() : node.rect.top()-parent.bottom(),gap);
                    }
                    for(int j=i+1;j<ids.size();++j)
                        QVERIFY2(!node.rect.intersects(e.nodes().value(ids[j]).rect),qPrintable(layout+" / "+spacing));
                }
            }
        }
    }
    void layouts() {
        Engine e;
        e.loadFixture(1000);
        e.setText(9, QString(600, 'W'));
        for (QString layout : {"Horizontal", "Vertical", "Compact"}) {
            e.setLayout(layout);
            QCOMPARE(e.visibleCount(), 1000);
            for (int i = 0; i < e.visibleIds().size(); ++i) {
                QRectF a = e.nodes()[e.visibleIds()[i]].rect;
                QVERIFY(a.width() > 0 && a.height() > 0);
                QVERIFY(e.bounds().contains(a));
                for (int j = i + 1; j < e.visibleIds().size(); ++j)
                    QVERIFY2(!a.intersects(e.nodes()[e.visibleIds()[j]].rect), qPrintable(layout));
            }
        }
        e.setLayout("Horizontal");
        QRectF narrow = e.bounds();
        e.setSpacing("Wide");
        QVERIFY(e.bounds().height() > narrow.height());
        e.select(1);
        e.navigate("right");
        QVERIFY(e.selectedId() != 1);
        e.navigate("left");
        QCOMPARE(e.selectedId(), 1);
        e.setLayout("Vertical");
        e.navigate("down");
        QVERIFY(e.selectedId() != 1);
        e.navigate("up");
        QCOMPARE(e.selectedId(), 1);
        e.setManual(true);
        int id = e.selectedId();
        QPointF pos = e.nodes()[id].rect.topLeft();
        e.moveManual(id, 20, 30);
        QCOMPARE(e.nodes()[id].rect.topLeft(), pos + QPointF(20, 30));
        e.undo();
        QCOMPARE(e.nodes()[id].rect.topLeft(), pos);
    }
    void persistence() {
        Engine e;
        QTemporaryDir dir;
        QString path = dir.filePath("map.json");
        e.setText(1, "Saved map");
        e.setNotes("Notes");
        e.setLayout("Vertical");
        QVERIFY(e.save(path));
        Engine read;
        QVERIFY(read.open(path));
        QCOMPARE(read.selectedText(), QString("Saved map"));
        QCOMPARE(read.selectedNotes(), QString("Notes"));
        QCOMPARE(read.layout(), QString("Vertical"));
        QCOMPARE(read.nodeCount(), e.nodeCount());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        QJsonArray nodes = json["nodes"].toArray();
        nodes.append(nodes.first());
        json["nodes"] = nodes;
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(json).toJson());
        f.close();
        int before = read.nodeCount();
        QVERIFY(!read.open(path));
        QCOMPARE(read.nodeCount(), before);
        QCOMPARE(read.selectedText(), QString("Saved map"));
        QVERIFY(!read.save(dir.path()));
    }
    void malformedDocuments() {
        Engine e;
        QTemporaryDir dir;
        QString path = dir.filePath("bad.json");
        QVERIFY(e.save(path));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject original = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        for (int mutation = 0; mutation < 7; ++mutation) {
            QJsonObject obj = original;
            QJsonArray nodes = obj["nodes"].toArray();
            QJsonObject root = nodes[0].toObject();
            QJsonObject second = nodes[1].toObject();
            if (mutation == 0)
                obj["version"] = 1.5;
            if (mutation == 1) {
                root["parent"] = 2;
                nodes[0] = root;
            }
            if (mutation == 2) {
                second["parent"] = 999;
                nodes[1] = second;
            }
            if (mutation == 3) {
                QJsonArray children = second["children"].toArray();
                children.append(1);
                second["children"] = children;
                nodes[1] = second;
            }
            if (mutation == 4) {
                root["children"] = QJsonArray();
                nodes[0] = root;
            }
            if (mutation == 5) {
                second["x"] = 1e20;
                nodes[1] = second;
            }
            if (mutation == 6) {
                second["id"] = 2.5;
                nodes[1] = second;
            }
            obj["nodes"] = nodes;
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QJsonDocument(obj).toJson());
            f.close();
            QVERIFY2(!e.open(path), qPrintable(QString("Mutation %1").arg(mutation)));
            QCOMPARE(e.nodeCount(), 15);
            QCOMPARE(e.selectedText(), QString("Mindarchy"));
        }
    }
    void relationships() {
        Engine e;
        e.select(2);
        e.select(3, true);
        e.connectSelection();
        QCOMPARE(e.connectionCount(), 1);
        e.connectSelection();
        QCOMPARE(e.connectionCount(), 1);
        e.undo();
        QCOMPARE(e.connectionCount(), 0);
        e.redo();
        QCOMPARE(e.connectionCount(), 1);
        QTemporaryDir dir;
        QString path = dir.filePath("connections.json");
        QVERIFY(e.save(path));
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.connections(), e.connections());
        e.select(2);
        e.removeSelected();
        QCOMPARE(e.connectionCount(), 0);
        e.undo();
        QCOMPARE(e.connectionCount(), 1);
    }
    void selectionNotificationsAndFoldedNavigation() {
        Engine e;
        QSignalSpy changed(&e, &Engine::changed), outline(&e, &Engine::outlineChanged);
        e.selectMany({2, 3, 4});
        QCOMPARE(e.selection().size(), 3);
        QCOMPARE(e.selectedId(), 4);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(outline.size(), 0);
        e.select(2);
        e.toggleFold();
        int count = e.visibleCount();
        e.navigate("right");
        QCOMPARE(e.selectedId(), 2);
        QVERIFY(e.selectedFolded());
        QCOMPARE(e.visibleCount(), count);
        QVERIFY(e.setText(2, "Updated"));
        QVERIFY(!e.setText(2, QString(16385, 'x')));
        QCOMPARE(e.selectedText(), QString("Updated"));
        QVERIFY(outline.size() > 0);
    }
    void deselection() {
        Engine e;
        int count = e.nodeCount();
        e.select(-1);
        QCOMPARE(e.selectedId(), -1);
        QVERIFY(e.selection().isEmpty());
        e.toggleTask();
        e.toggleChecked();
        e.setNotes("none");
        QCOMPARE(e.nodeCount(), count);
        e.setSpacing("Wide");
        QCOMPARE(e.selectedId(), -1);
        QVERIFY(e.selection().isEmpty());
        e.navigate("right");
        QCOMPARE(e.selectedId(), 1);
    }
    void compactInvariants() {
        Engine e;
        e.setSpacing("Wide");
        e.setManual(true);
        e.setLayout("Compact");
        QVERIFY(!e.manual());
        e.setManual(true);
        QVERIFY(!e.manual());
        e.setSpacing("Narrow");
        QCOMPARE(e.spacing(), QString("Wide"));
    }
    void parentRelativeManualOffsets() {
        Engine e;
        e.setManual(true);
        int child = e.nodes()[1].children.first();
        int grandchild = e.nodes()[child].children.first();
        QPointF root = e.nodes()[1].rect.topLeft(), c = e.nodes()[child].rect.topLeft(),
                g = e.nodes()[grandchild].rect.topLeft();
        e.moveManual(1, 40, 60);
        QCOMPARE(e.nodes()[1].rect.topLeft(), root + QPointF(40, 60));
        QCOMPARE(e.nodes()[child].rect.topLeft(), c + QPointF(40, 60));
        QCOMPARE(e.nodes()[grandchild].rect.topLeft(), g + QPointF(40, 60));
        e.moveManual(child, 10, 20);
        QCOMPARE(e.nodes()[grandchild].rect.topLeft(), g + QPointF(50, 80));
        e.undo();
        QCOMPARE(e.nodes()[grandchild].rect.topLeft(), g + QPointF(40, 60));
    }
    void rejectsOversizedRichText() {
        Engine e;
        QString before = e.selectedText();
        QString huge = "<span style='font-size:100000px'>Huge</span>";
        QVERIFY(!e.setText(1, huge));
        QCOMPARE(e.selectedText(), before);
        QVERIFY(!e.canUndo());
        QVERIFY(e.error().contains("too tall"));
        QVERIFY(!e.setText(1, QString("Line<br>").repeated(1000)));
        QCOMPARE(e.selectedText(), before);
        QVERIFY(e.setText(1, "<b>Normal rich text</b>"));
        QTemporaryDir dir;
        QString path = dir.filePath("huge.json");
        QVERIFY(e.save(path));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        QJsonArray nodes = obj["nodes"].toArray();
        QJsonObject root = nodes[0].toObject();
        root["text"] = huge;
        nodes[0] = root;
        obj["nodes"] = nodes;
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(obj).toJson());
        f.close();
        QVERIFY(!e.open(path));
        QCOMPARE(e.selectedText(), QString("<b>Normal rich text</b>"));
        QVERIFY(e.error().contains("too tall"));
    }
    void stableCompactPacking() {
        Engine narrow, wide;
        narrow.setSpacing("Narrow");
        wide.setSpacing("Wide");
        narrow.setLayout("Compact");
        wide.setLayout("Compact");
        QCOMPARE(narrow.bounds(), wide.bounds());
        Engine manual;
        manual.setManual(true);
        manual.setSpacing("Wide");
        QCOMPARE(manual.spacing(), QString("Standard"));
    }
    void textMeasurementUpdates() {
        Engine e(nullptr, Engine::InitialContent::Blank);
        e.setText(1, "Short title");
        QSizeF shortSize = e.nodes()[1].rect.size();
        e.setText(1, QString(350, 'W'));
        QSizeF longSize = e.nodes()[1].rect.size();
        QVERIFY(longSize.height() > shortSize.height());
        e.setLayout("Vertical");
        QCOMPARE(e.nodes()[1].rect.size(), longSize);
        e.toggleTask();
        QCOMPARE(e.nodes()[1].rect.width(), longSize.width() + 20);
        QCOMPARE(e.nodes()[1].rect.height(), longSize.height());
        QTextDocument doc;
        QFont font("sans-serif", 11);
        font.setPixelSize(15);
        doc.setDefaultFont(font);
        doc.setDocumentMargin(0);
        doc.setDefaultStyleSheet("body,p {color:#e9eff4; margin:0;} a {color:#8be4cf;}");
        doc.setHtml(e.selectedText());
        doc.setTextWidth(e.nodes()[1].rect.width() - 50);
        QVERIFY(e.nodes()[1].rect.height() >= doc.size().height() + 16);
        e.undo();
        QCOMPARE(e.nodes()[1].rect.size(), longSize);
        e.setText(1, "Short title");
        QCOMPARE(e.nodes()[1].rect.size(), shortSize);
        e.toggleChecked();
        QVERIFY(e.selectedTask());
        QCOMPARE(e.nodes()[1].rect.width(), shortSize.width() + 20);
    }
    void boundedHistory() {
        Engine e;
        for (int i = 0; i < 60; ++i)
            e.setText(1, QString("Revision %1").arg(i));
        int steps = 0;
        while (e.canUndo()) {
            e.undo();
            ++steps;
            QVERIFY(steps <= 40);
        }
        QCOMPARE(steps, 40);
        QVERIFY(e.canRedo());
        e.setText(1, "New history branch");
        QVERIFY(!e.canRedo());
    }
    void fixtureLimitAndPerformance() {
        Engine e;
        for (int count : {1000, 10000}) {
            e.loadFixture(count);
            QCOMPARE(e.nodeCount(), count);
            QCOMPARE(e.visibleCount(), count);
            qInfo() << count << "initial fixture layout milliseconds" << e.layoutMs();
            e.setLayout("Vertical");
            for (QString layout : {"Horizontal", "Vertical", "Compact"}) {
                e.setLayout(layout);
                qInfo() << count << layout << "layout milliseconds" << e.layoutMs();
            }
        }
        e.loadFixture(100001);
        QCOMPARE(e.nodeCount(), 10000);
        QVERIFY(!e.error().isEmpty());
    }
};
QTEST_MAIN(EngineTest)
#include "engine_test.moc"
