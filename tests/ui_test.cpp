#include "../src/canvas.h"
#include "../src/engine.h"
#include <QGuiApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextDocument>
#include <QtTest>

class UiTest : public QObject {
    Q_OBJECT
    Engine *document = nullptr;
    QQmlApplicationEngine *qml = nullptr;
    QQuickWindow *window = nullptr;
    MindCanvas *canvas = nullptr;
    QQuickItem *editor = nullptr;
    QQuickItem *findVisual(QQuickItem *item, const QString &name) {
        if (item->objectName() == name) return item;
        for (auto *child : item->childItems())
            if (auto *found = findVisual(child, name)) return found;
        return nullptr;
    }
    void stage(const QString &name) {
        const int delay = qEnvironmentVariableIntValue("MINDMAP_LIVE_TEST_MS");
        if (delay > 0) {
            window->setTitle("LIVE TEST — " + name);
            QTest::qWait(delay);
        }
    }
    QPoint screenCenter(int id) const {
        return canvas->mapToScene(canvas->mapFromWorld(document->nodes().value(id).rect.center()))
            .toPoint();
    }
    void drag(const QPoint &from, const QPoint &to) {
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        for (int step = 1; step <= 8; ++step)
            QTest::mouseMove(window, from + (to - from) * step / 8, 30);
        stage("Dragging branch — release to apply");
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTest::qWait(300);
    }
    QString plain(int id) const {
        QTextDocument text;
        text.setHtml(document->nodes().value(id).text);
        return text.toPlainText();
    }
    void type(const QString &text) {
        for (QChar c : text)
            QTest::keyClick(window, c.toLatin1());
        stage("Editing: " + text);
    }
  private slots:
    void integratedColorPickerPresetsCustomAndCancel() {
        auto *button=window->findChild<QObject *>("style-fill-picker");
        QVERIFY(button);
        const auto original=document->appearance(1).fill;
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QObject *picker=nullptr;
        for (auto *candidate : window->findChildren<QObject *>("appColorPicker"))
            if (candidate->property("visible").toBool()) picker=candidate;
        QVERIFY(picker);
        if (qEnvironmentVariableIsSet("MINDMAP_COLOR_SCREENSHOT")) {
            QTest::qWait(200); QVERIFY(window->grabWindow().save(qEnvironmentVariable("MINDMAP_COLOR_SCREENSHOT")));
        }
        auto *content=picker->property("contentItem").value<QQuickItem *>();
        QVERIFY(content);
        auto *preset=findVisual(content,"preset-ef8585");
        QVERIFY(preset); QVERIFY(QMetaObject::invokeMethod(preset, "clicked"));
        QCOMPARE(document->appearance(1).fill, original);
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QCOMPARE(document->appearance(1).fill.name(), QString("#ef8585"));
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        auto *hex=findVisual(content,"colorHex");
        QVERIFY(hex); hex->setProperty("text", "#8040a0e0");
        QVERIFY(QMetaObject::invokeMethod(hex, "textEdited"));
        QCOMPARE(picker->property("selectedColor").value<QColor>().name(QColor::HexArgb), QString("#8040a0e0"));
        QVERIFY(QMetaObject::invokeMethod(picker, "reject"));
        QCOMPARE(document->appearance(1).fill.name(), QString("#ef8585"));
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        hex->setProperty("text", "not a color");
        QVERIFY(!picker->property("validHex").toBool());
        hex->setProperty("text", "#8040a0e0"); QMetaObject::invokeMethod(hex,"textEdited");
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QCOMPARE(document->appearance(1).fill.name(QColor::HexArgb), QString("#8040a0e0"));
    }
    void documentHeaderTracksSaveAndEditing() {
        QTemporaryDir dir;
        auto *name = window->findChild<QQuickItem *>("documentNameLabel");
        auto *edited = window->findChild<QQuickItem *>("documentEditedLabel");
        QVERIFY(name); QVERIFY(edited);
        QVERIFY(document->save(dir.filePath("My project.omm")));
        QTRY_COMPARE(name->property("text").toString(), QString("My project"));
        QTRY_VERIFY(!edited->isVisible());
        QTRY_VERIFY(qAbs(name->mapToScene(QPointF(0, name->height()/2)).y() - 30) < 1);
        canvas->beginEdit(1);
        QTRY_VERIFY(canvas->editing());
        QVERIFY(!window->property("documentEdited").toBool());
        type("Changed title");
        QTRY_VERIFY(window->property("documentEdited").toBool());
        QVERIFY(QMetaObject::invokeMethod(window, "saveDocument", Q_ARG(QVariant, false)));
        QTRY_VERIFY(!edited->isVisible());
        QVERIFY(!document->edited());
        if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS) {
            auto *mouse = window->findChild<QQuickItem *>("documentTitleMouse");
            QVERIFY(mouse);
            const QPoint position = mouse->mapToScene(QPointF(mouse->width()/2, mouse->height()/2)).toPoint();
            QTest::mouseMove(window, QPoint(window->width()/2, 150));
            QTest::qWait(180);
            const qreal initialX = name->mapToScene(QPointF()).x();
            QTest::mouseMove(window, position);
            QTRY_VERIFY(name->mapToScene(QPointF()).x() > initialX + 19);
            QSignalSpy menu(document, &Engine::nativeFolderMenuRequested);
            QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, position);
            QCOMPARE(menu.count(), 1);
            QTest::mouseMove(window, QPoint(window->width()/2, 150));
            QTRY_VERIFY(qAbs(name->mapToScene(QPointF()).x()-initialX) < 1);
        }
    }
    void quitConfirmationWaitsForSessionAndCloseShortcutForgets() {
        QTemporaryDir dir;
        QVERIFY(document->save(dir.filePath("shortcuts.omm")));
        QSignalSpy votes(document, &Engine::quitDecision);
        QSignalSpy closes(document, &Engine::windowCloseApproved);
        QVERIFY(QMetaObject::invokeMethod(window, "requestClose", Q_ARG(QVariant, false), Q_ARG(QVariant, true)));
        QCOMPARE(votes.count(), 1); QVERIFY(votes.first().first().toBool());
        QVERIFY(window->isVisible()); QCOMPARE(closes.count(), 0);
        QVERIFY(QMetaObject::invokeMethod(window, "abortSessionQuit"));
        QVERIFY(window->contentItem()->isEnabled());
        QTest::keySequence(window, QKeySequence(QKeySequence::Close));
        QTRY_COMPARE(closes.count(), 1);
        QVERIFY(closes.first().first().toBool());
        QTRY_VERIFY(!window->isVisible());
        window->setProperty("allowClose", false);
        window->show();
    }
    void saveOverwritesOpenedDocument() {
        QTemporaryDir dir;
        const QString path = dir.filePath("existing.omm");
        QVERIFY(document->save(path));
        QVERIFY(document->open(path));
        document->setText(1, "Saved by shortcut");
        QTest::keySequence(window, QKeySequence(QKeySequence::Save));
        QTRY_VERIFY(!document->hasUnsavedChanges());
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.selectedText(), QString("Saved by shortcut"));
        document->setText(1, "Saved by toolbar");
        auto *button = window->findChild<QQuickItem *>("saveDocumentButton");
        QVERIFY(button);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
        QTRY_VERIFY(!document->hasUnsavedChanges());
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.selectedText(), QString("Saved by toolbar"));
        QVERIFY(window->isVisible());
    }
    void closeConfirmationCanCancelOrSave() {
        QTemporaryDir dir;
        QVERIFY(!window->close());
        auto *dialog = window->findChild<QObject *>("closeConfirmation");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QVERIFY(window->isVisible());
        const QString path = dir.filePath("close.omm");
        QVERIFY(document->save(path));
        document->setText(1, "Save on close");
        QVERIFY(!window->close());
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(window, "saveBeforeClosing"));
        QTRY_VERIFY(!window->isVisible());
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.selectedText(), QString("Save on close"));
        window->setProperty("allowClose", false);
        window->show();
    }
    void initTestCase() {
        qmlRegisterUncreatableType<Engine>("MindmapLab", 1, 0, "Engine", "Provided by application");
        qmlRegisterType<MindCanvas>("MindmapLab", 1, 0, "MindCanvas");
        document = new Engine(this);
        qml = new QQmlApplicationEngine(this);
        qml->rootContext()->setContextProperty("engine", document);
        qml->load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY2(!qml->rootObjects().isEmpty(), "The full application QML must load");
        window = qobject_cast<QQuickWindow *>(qml->rootObjects().first());
        QVERIFY(window);
        canvas = window->findChild<MindCanvas *>("mindCanvas");
        editor = window->findChild<QQuickItem *>("titleEditor");
        QVERIFY(canvas);
        QVERIFY(editor);
        if (qEnvironmentVariableIntValue("MINDMAP_LIVE_TEST_MS") > 0)
            window->showMaximized();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }
    void init() {
        canvas->endEdit();
        document->loadFixture(15);
        document->setLayout("Horizontal");
        document->setManual(false);
        document->setThemeId("lab");
        document->select(1);
        canvas->fit();
        canvas->forceActiveFocus();
        QTest::qWait(350);
        QVERIFY(canvas->hasActiveFocus());
        QCOMPARE(window->activeFocusItem(), canvas);
        stage(QString::fromLatin1(QTest::currentTestFunction()));
    }
    void toolbarGroupsAndNewDocument() {
        auto *left=window->findChild<QQuickItem *>("documentActions");
        auto *center=window->findChild<QQuickItem *>("editingActions");
        auto *right=window->findChild<QQuickItem *>("panelActions");
        auto *button=window->findChild<QQuickItem *>("newDocumentButton");
        QVERIFY(left); QVERIFY(center); QVERIFY(right); QVERIFY(button);
        auto x=[](QQuickItem *item) {return item->mapToScene(QPointF()).x();};
        QVERIFY(x(left)+left->width()<x(center));
        QVERIFY(x(center)+center->width()<x(right));
        QVERIFY(qAbs(x(center)+center->width()/2-window->width()/2)<1);
        QSignalSpy requested(document,&Engine::newDocumentRequested);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        QCOMPARE(requested.count(),1); QCOMPARE(document->nodeCount(),15);
        QTest::keySequence(window,QKeySequence(QKeySequence::New));
        QTRY_COMPARE(requested.count(),2);
        const auto originalSize=window->size();
        for(int width:{600,950,1380}) {
            window->resize(width,900); QTest::qWait(100);
            QVERIFY(x(left)+left->width()<x(center));
            QVERIFY(x(center)+center->width()<x(right));
            QVERIFY(x(left)>=0); QVERIFY(x(right)+right->width()<=window->width());
        }
        window->resize(originalSize);
    }
    void cleanupTestCase() {
        delete qml;
        qml = nullptr;
    }
    void convertNodeUsingInspector() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs); tabs->setProperty("currentIndex",1);
        document->select(2); const auto title=document->selectedText(); const auto count=document->nodeCount();
        auto choose=[&](QString name) {
            auto *item=findVisual(window->contentItem(),"node-type-"+name);
            if(!item) return false; item->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space); return true;
        };
        QVERIFY(choose("Task")); QTRY_VERIFY(document->selectedTask());
        auto *completed=window->findChild<QQuickItem *>("taskCompleted"); QVERIFY(completed); QVERIFY(completed->isVisible());
        completed->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space); QVERIFY(document->selectedChecked());
        const QString dir=qEnvironmentVariable("MINDMAP_TYPE_SCREENSHOTS");
        if(!dir.isEmpty()) {QDir().mkpath(dir); QTest::qWait(150); QVERIFY(window->grabWindow().save(dir+"/task.png"));}
        QVERIFY(choose("Date")); QTRY_COMPARE(document->selectedKind(),QString("date")); QCOMPARE(document->nodeCount(),count);
        QVERIFY(!document->selectedTask()); QVERIFY(!completed->isVisible());
        if(!dir.isEmpty()) {canvas->fit(); QTest::qWait(250); QVERIFY(window->grabWindow().save(dir+"/date.png"));}
        document->undo(); QVERIFY(document->selectedTask()); QVERIFY(document->selectedChecked());
        QVERIFY(choose("Text")); QTRY_COMPARE(document->selectedKind(),QString("text"));
        QVERIFY(!document->selectedTask()); QCOMPARE(document->selectedText(),title);
        auto *options=window->findChild<QQuickItem *>("nodeTypeOptions"); QVERIFY(options); QVERIFY(!options->isVisible());
        tabs->setProperty("currentIndex",0);
    }
    void dateNodeCreateEditHoverAndDrag() {
        document->addDateNode("month"); canvas->revealNode(document->selectedId());
        const int id=document->selectedId(); QCOMPARE(document->nodes().value(id).kind,QString("date"));
        QVERIFY(!canvas->editing()); QVERIFY(document->configureDateNode(id,"month","2026-09-08"));
        canvas->fit(); QTest::qWait(250);
        auto *dialog=window->findChild<QObject *>("dateEntryDialog"); QVERIFY(dialog);
        auto *field=window->findChild<QQuickItem *>("dateEntryText"); QVERIFY(field);
        auto button=[this](const QString &name) { auto *item=window->findChild<QQuickItem *>(name); if(item) {item->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);} return item!=nullptr; };
        const auto days=Calendar::days(document->nodes().value(id).calendar);
        const int index=days.indexOf(QDate(2026,9,8)); QVERIFY(index>=0);
        auto point=[&] { return canvas->mapToScene(canvas->mapFromWorld(canvas->nodeRect(id).topLeft()+Calendar::cell(index).center())).toPoint(); };
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point());
        QTRY_VERIFY(dialog->property("opened").toBool()); QVERIFY(!dialog->property("existing").toBool());
        field->setProperty("text","Design review"); QVERIFY(button("dateEntryCancel"));
        QTRY_VERIFY(!dialog->property("opened").toBool()); QVERIFY(document->dateEntry(id,"2026-09-08").isEmpty());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point()); QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text","Design review"); QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Design review"));
        QTest::mouseMove(window,point()+QPoint(0,40)); QTest::mouseMove(window,point());
        QTRY_COMPARE(canvas->dateHoverText(),QString("Design review"));
        auto *tooltip=window->findChild<QObject *>("dateEntryTooltip"); QVERIFY(tooltip);
        QCOMPARE(tooltip->property("delay").toInt(),0);
        QVERIFY(tooltip->property("visible").toBool());
        QCOMPARE(tooltip->property("text").toString(),QString("Design review"));
        const QString dir=qEnvironmentVariable("MINDMAP_DATE_SCREENSHOTS");
        if(!dir.isEmpty()) {QDir().mkpath(dir); QTest::qWait(600); QVERIFY(window->grabWindow().save(dir+"/month-hover.png"));}
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point()); QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(dialog->property("existing").toBool()); QCOMPARE(field->property("text").toString(),QString("Design review"));
        field->setProperty("text","Updated review");
        if(!dir.isEmpty()) {QTest::qWait(100); QVERIFY(window->grabWindow().save(dir+"/edit-entry.png"));}
        QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Updated review"));
        document->setManual(true); QTest::qWait(250); const auto before=document->nodes().value(id).rect;
        const auto from=point(); QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(window,from+QPoint(35,20),60); QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from+QPoint(35,20));
        QVERIFY(!dialog->property("opened").toBool()); QVERIFY(document->nodes().value(id).rect!=before);
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Updated review"));
        QVERIFY(document->configureDateNode(id,"week","2026-09-08")); canvas->fit(); QTest::qWait(250);
        if(!dir.isEmpty()) QVERIFY(window->grabWindow().save(dir+"/week.png"));
        auto clickControl=[&](QRectF control) {
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                canvas->mapToScene(canvas->mapFromWorld(canvas->nodeRect(id).topLeft()+control.center())).toPoint());
            QTest::qWait(200);
        };
        clickControl(Calendar::next());
        QCOMPARE(document->nodes().value(id).calendar.anchor,QDate(2026,9,15));
        QVERIFY(!dialog->property("opened").toBool());
        clickControl(Calendar::previous());
        QCOMPARE(document->nodes().value(id).calendar.anchor,QDate(2026,9,8));
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs); tabs->setProperty("currentIndex",1);
        auto *view=window->findChild<QQuickItem *>("dateNodeView"); QVERIFY(view);
        view->forceActiveFocus(); QTest::keyClick(window,Qt::Key_End);
        QTRY_COMPARE(document->nodes().value(id).calendar.view,QString("month"));
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Updated review"));
        if(!dir.isEmpty()) {QTest::qWait(250); QVERIFY(window->grabWindow().save(dir+"/date-inspector.png"));}
        tabs->setProperty("currentIndex",0);
        canvas->editDateEntry(id,"2026-09-08"); QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(button("dateEntryRemove")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QVERIFY(document->dateEntry(id,"2026-09-08").isEmpty());
        canvas->editDateEntry(id,"2026-09-08"); QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text","12.5"); QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(Calendar::totals(document->nodes().value(id).calendar).month,12.5);
        const auto numericSize=document->nodes().value(id).rect.size();
        canvas->editDateEntry(id,"2026-09-08"); QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text","7.5"); QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(Calendar::totals(document->nodes().value(id).calendar).month,7.5);
        QCOMPARE(document->nodes().value(id).rect.size(),numericSize);
        QVERIFY(document->setDateEntry(id,"2026-09-01","10"));
        QVERIFY(document->setDateEntry(id,"2026-09-16","-2.5"));
        canvas->fit(); QTest::qWait(250);
        if(!dir.isEmpty()) QVERIFY(window->grabWindow().save(dir+"/month-sums.png"));
    }
    void nodePanelAppliesStylesAndProtectsDraft() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); tabs->setProperty("currentIndex",1);
        auto *panel=window->findChild<QQuickItem *>("nodeStylePanel"); QVERIFY(panel);
        document->select(2);
        auto *shape=window->findChild<QQuickItem *>("style-shape"); QVERIFY(shape);
        shape->forceActiveFocus(); QTest::keyClick(window,Qt::Key_End);
        QTRY_COMPARE(document->appearance(2).shape,NodeShape::Octagon);
        auto *fixed=window->findChild<QQuickItem *>("style-fixedWidth"); QVERIFY(fixed);
        fixed->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(document->nodes().value(2).style.value("width").toDouble()>0);
        auto apply=[panel](QString key,QVariant value) {
            QVariant accepted;
            const bool called=QMetaObject::invokeMethod(panel,"apply",Q_RETURN_ARG(QVariant,accepted),
                Q_ARG(QVariant,key),Q_ARG(QVariant,value));
            return called && accepted.toBool();
        };
        QVERIFY(apply("fontSize",28)); QVERIFY(apply("bold",true)); QVERIFY(apply("width",240));
        QVERIFY(apply("borderWidth",3)); QVERIFY(apply("borderStyle",2));
        QVERIFY(apply("branchStroke",3)); QVERIFY(apply("branchWidth",4));
        QVERIFY(apply("fill",QString("#d7e9b4"))); QVERIFY(apply("textColor",QString("#24311a")));
        QVERIFY(apply("alignment",1));
        QCOMPARE(document->selectedStyle()["fontSize"].toDouble(),28.);
        QVERIFY(document->selectedStyle()["bold"].toBool());
        auto *family=window->findChild<QQuickItem *>("style-fontFamily"); QVERIFY(family);
        QCOMPARE(family->property("editText").toString(),document->selectedStyle()["fontFamily"].toString());
        canvas->fit(); canvas->beginEdit(2);
        QTest::keyClick(window,Qt::Key_Right);
        type(" with a longer title");
        const auto before=canvas->editingRect();
        QTest::qWait(150);
        const QString dir=qEnvironmentVariable("MINDMAP_NODE_SCREENSHOTS");
        if(!dir.isEmpty()) { QDir().mkpath(dir); QVERIFY(window->grabWindow().save(dir+"/node-panel-editing.png")); }
        QTest::keyClick(window,Qt::Key_Return); QTRY_VERIFY(!canvas->editing());
        QCOMPARE(document->nodes().value(2).rect.size()*canvas->zoom(),before.size());
        QCOMPARE(document->selectedStyle()["fontSize"].toDouble(),28.);
        QVERIFY(document->selectedStyle()["bold"].toBool());
        QTest::qWait(200);
        if(!dir.isEmpty()) {
            QVERIFY(window->grabWindow().save(dir+"/node-panel-committed.png"));
            auto *scroll=window->findChild<QQuickItem *>("inspectorScroll"); QVERIFY(scroll);
            auto *flick=qvariant_cast<QObject *>(scroll->property("contentItem")); QVERIFY(flick);
            flick->setProperty("contentY",650); QTest::qWait(150);
            QVERIFY(window->grabWindow().save(dir+"/node-panel-font.png"));
            flick->setProperty("contentY",0);
        }
        canvas->beginEdit(2); editor->setProperty("text",QString(17000,'x'));
        QVERIFY(!apply("shape",1)); QCOMPARE(document->appearance(2).shape,NodeShape::Octagon);
        QVERIFY(canvas->editing()); canvas->endEdit();
        tabs->setProperty("currentIndex",0);
    }
    void researchThemesApplyOptionalLayouts() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); tabs->setProperty("currentIndex",2);
        auto *toggle=window->findChild<QQuickItem *>("useThemeLayouts"); QVERIFY(toggle);
        toggle->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QVERIFY(window->property("useThemeLayouts").toBool());
        for(const auto &id:QStringList{"canopy","atlas","studio","nocturne"}) {
            auto *card=findVisual(window->contentItem(),"theme-"+id); QVERIFY(card);
            card->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
            QTRY_COMPARE(document->themeId(),id);
            QCOMPARE(document->layout(),Themes::layoutRecipe(id)["layout"].toString());
            QTest::qWait(250);
            const QString dir=qEnvironmentVariable("MINDMAP_RESEARCH_SCREENSHOTS");
            if(!dir.isEmpty()) {
                auto *scroll=window->findChild<QQuickItem *>("inspectorScroll"); QVERIFY(scroll);
                auto *flick=qvariant_cast<QObject *>(scroll->property("contentItem")); QVERIFY(flick);
                flick->setProperty("contentY",flick->property("contentY").toDouble()+card->mapToScene({0,0}).y()-scroll->mapToScene({0,0}).y());
                QTest::qWait(100); QDir().mkpath(dir);
                QVERIFY(window->grabWindow().save(dir+"/"+id+".png"));
            }
        }
        window->setProperty("useThemeLayouts",false);
        document->setLayout("Vertical");
        auto *card=findVisual(window->contentItem(),"theme-canopy"); card->forceActiveFocus();
        QTest::keyClick(window,Qt::Key_Space); QCOMPARE(document->themeId(),QString("canopy"));
        QCOMPARE(document->layout(),QString("Vertical"));
        canvas->beginEdit(document->selectedId()); editor->setProperty("text",QString(17000,'x'));
        window->setProperty("useThemeLayouts",true);
        QVariant accepted; QVERIFY(QMetaObject::invokeMethod(window,"applyTheme",Q_RETURN_ARG(QVariant,accepted),Q_ARG(QVariant,QString("atlas"))));
        QVERIFY(!accepted.toBool()); QCOMPARE(document->themeId(),QString("canopy"));
        canvas->endEdit(); window->setProperty("useThemeLayouts",false); tabs->setProperty("currentIndex",0);
    }
    void themeCardsApplyAllPresets() {
        window->setProperty("inspectorVisible", true);
        auto *tabs = window->findChild<QQuickItem *>("inspectorTabs");
        QVERIFY(tabs);
        tabs->setProperty("currentIndex", 2);
        for (const QString &id : {QString("beach-day"), QString("holographic"),
                                  QString("retro"), QString("arcade")}) {
            auto *card = findVisual(window->contentItem(), "theme-" + id);
            QVERIFY(card);
            // Activating a keyboard-focused Button uses its actual clicked signal path.
            card->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Space);
            QTRY_COMPARE(document->themeId(), id);
            QVERIFY(card->property("checked").toBool());
            QTest::qWait(150);
            QTemporaryDir exportDir;
            QVERIFY(exportDir.isValid());
            const QString png = exportDir.filePath("theme.png");
            QSignalSpy exported(canvas, &MindCanvas::exportFinished);
            QVERIFY(canvas->exportPng(png));
            QTRY_COMPARE(exported.count(), 1);
            QVERIFY(exported.first().at(1).toBool());
            QImage captured(png);
            QVERIFY(!captured.isNull());
            QCOMPARE(captured.pixelColor(5,5), document->canvasColor());
            const QString dir = qEnvironmentVariable("MINDMAP_THEME_SCREENSHOTS");
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                QVERIFY(window->grabWindow().save(dir + "/theme-" + id + ".png"));
            }
        }
        tabs->setProperty("currentIndex", 0);
        canvas->forceActiveFocus();
    }
    void themesPreservePendingNotes() {
        auto *notes = window->findChild<QQuickItem *>("notesEditor");
        QVERIFY(notes);
        notes->setProperty("text", "Pending notes");
        document->setThemeId("retro");
        QCOMPARE(notes->property("text").toString(), QString("Pending notes"));
        QCOMPARE(document->selectedNotes(), QString());
        document->select(2);
        QCOMPARE(notes->property("text").toString(), document->selectedNotes());
    }
    void themesApplyThroughUiAndProtectDraft() {
        const int count = document->nodeCount();
        const auto selected = document->selectedIds();
        QVariant accepted;
        QVERIFY(QMetaObject::invokeMethod(window, "applyTheme", Q_RETURN_ARG(QVariant, accepted),
                                         Q_ARG(QVariant, QString("beach-day"))));
        QVERIFY(accepted.toBool());
        QCOMPARE(document->property("themeId").toString(), QString("beach-day"));
        QCOMPARE(document->nodeCount(), count);
        QCOMPARE(document->selectedIds(), selected);
        document->undo();
        QVERIFY(document->property("themeId").toString() != QString("beach-day"));
        document->redo();
        QCOMPARE(document->property("themeId").toString(), QString("beach-day"));
        canvas->beginEdit(document->selectedId());
        editor->setProperty("text", QString(17000, 'x'));
        QVERIFY(QMetaObject::invokeMethod(window, "applyTheme", Q_RETURN_ARG(QVariant, accepted),
                                         Q_ARG(QVariant, QString("retro"))));
        QVERIFY(!accepted.toBool());
        QCOMPARE(document->property("themeId").toString(), QString("beach-day"));
        QVERIFY(canvas->editing());
        QTextDocument draft;
        draft.setHtml(editor->property("text").toString());
        QCOMPARE(draft.toPlainText(), QString(17000, 'x'));
    }
    void inlineEditingKeepsViewportAndDocumentStable() {
        canvas->zoomAt({canvas->width()/2, canvas->height()/2}, .6 / canvas->zoom());
        const double zoomBefore = canvas->zoom();
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(canvas->editing());
        QCOMPARE(canvas->zoom(), zoomBefore);
        const int id = canvas->editingId();
        const QString stored = document->nodes().value(id).text;
        const auto beforeNodes = document->nodes();
        const QPointF anchor = canvas->editingRect().topLeft();
        QSignalSpy outlineChanges(document, &Engine::outlineChanged);
        editor->setProperty("text", "A longer title typed directly into its node");
        QTest::qWait(50);
        QCOMPARE(document->nodes().value(id).text, stored);
        QCOMPARE(outlineChanges.count(), 0);
        QCOMPARE(canvas->editingRect().topLeft(), anchor);
        for (auto it=beforeNodes.cbegin(); it!=beforeNodes.cend(); ++it)
            QCOMPARE(document->nodes().value(it.key()).rect, it->rect);
        const QSizeF preview = canvas->editingRect().size();
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!canvas->editing());
        const QRectF finalRect = document->nodes().value(id).rect;
        QVERIFY(QLineF(canvas->mapFromWorld(finalRect.topLeft()),anchor).length() < .5);
        QVERIFY(std::abs(finalRect.width()*canvas->zoom()-preview.width()) < .5);
        QVERIFY(std::abs(finalRect.height()*canvas->zoom()-preview.height()) < .5);
        QCOMPARE(plain(id), QString("A longer title typed directly into its node"));
    }
    void inlineEditingAcrossThemes() {
        for (const QString &theme : {QString("beach-day"), QString("holographic"),
                                     QString("retro"), QString("arcade")}) {
            document->setThemeId(theme);
            document->select(1);
            canvas->zoomAt({canvas->width()/2, canvas->height()/2}, 1 / canvas->zoom());
            canvas->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Tab);
            QTRY_VERIFY(editor->hasActiveFocus());
            type("An idea taking shape");
            QTest::qWait(100);
            const int id = canvas->editingId();
            const QRectF preview = canvas->editingRect();
            const QString dir = qEnvironmentVariable("MINDMAP_INLINE_SCREENSHOTS");
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                QVERIFY(window->grabWindow().save(dir + "/" + theme + "-editing.png"));
            }
            QTest::keyClick(window, Qt::Key_Return);
            QTRY_VERIFY(!canvas->editing());
            QTest::qWait(100);
            const QRectF result = document->nodes().value(id).rect;
            QVERIFY(QLineF(canvas->mapFromWorld(result.topLeft()), preview.topLeft()).length() < .5);
            QCOMPARE(result.size() * canvas->zoom(), preview.size());
            if (!dir.isEmpty())
                QVERIFY(window->grabWindow().save(dir + "/" + theme + "-committed.png"));
        }
    }
    void tabCreatesChildAndFocusesEditor() {
        int parent = document->selectedId();
        int count = document->nodeCount();
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(document->nodes().value(document->selectedId()).parent, parent);
        QVERIFY(canvas->editing());
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Tab child");
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QVERIFY(!canvas->editing());
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(plain(document->selectedId()), QString("Tab child"));
        QVERIFY(canvas->hasActiveFocus());
        stage("Title committed — no extra sibling created");
    }
    void returnCommitsThenNextReturnCreatesSibling() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        int parent = document->nodes().value(id).parent;
        int count = document->nodeCount();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Committed title");
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QCOMPARE(document->nodeCount(), count);
        QCOMPARE(plain(id), QString("Committed title"));
        QVERIFY(!canvas->editing());
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(document->nodes().value(document->selectedId()).parent, parent);
        QTRY_VERIFY(editor->hasActiveFocus());
        stage("Second Return created a sibling");
    }
    void tabDuringEditingCommitsAndCreatesChild() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        int count = document->nodeCount();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Parent title");
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QCOMPARE(plain(id), QString("Parent title"));
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(document->nodes().value(document->selectedId()).parent, id);
        QTRY_VERIFY(editor->hasActiveFocus());
        QCOMPARE(canvas->editingId(), document->selectedId());
        stage("Tab committed parent and opened a child");
    }
    void richTextShortcutPersistsFormatting() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        int id = document->selectedId();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Bold title");
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        stage("Bold formatting applied to selected title");
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QCOMPARE(plain(id), QString("Bold title"));
        QVERIFY(document->nodes().value(id).text.contains("font-weight:700"));
    }
    void rejectedTitleStaysInEditor() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        QString original = document->nodes().value(id).text;
        QTRY_VERIFY(editor->hasActiveFocus());
        editor->setProperty("text", QString(17000, QLatin1Char('x')));
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        QVERIFY(editor->hasActiveFocus());
        QCOMPARE(document->nodes().value(id).text, original);
        QVERIFY(!document->error().isEmpty());
    }
    void canvasClickCommitsPendingTitle() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Click committed");
        QPointF point = canvas->mapToScene(QPointF(16, canvas->height() - 16));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QTRY_VERIFY(!canvas->editing());
        QCOMPARE(plain(id), QString("Click committed"));
        stage("Canvas click saved the draft");
    }
    void layoutsAndFoldingPreserveHierarchy() {
        const int count = document->nodeCount();
        const int root = 1;
        QVERIFY(!document->nodes().value(root).children.isEmpty());
        const int branch = document->nodes().value(root).children.first();
        const auto children = document->nodes().value(branch).children;
        QVERIFY(!children.isEmpty());
        for (const QString &layout :
             {QString("Horizontal"), QString("Vertical"), QString("Compact")}) {
            document->setLayout(layout);
            QTest::qWait(300);
            canvas->fit();
            QCOMPARE(document->nodeCount(), count);
            QCOMPARE(document->nodes().value(branch).children, children);
            stage(layout + " layout — hierarchy preserved");
        }
        document->select(branch);
        const int visible = document->visibleCount();
        QTest::keyClick(window, Qt::Key_F);
        QTest::qWait(300);
        QVERIFY(document->selectedFolded());
        QVERIFY(document->visibleCount() < visible);
        QCOMPARE(document->nodeCount(), count);
        canvas->fit();
        stage("Folded branch — descendants retained");
        QTest::keyClick(window, Qt::Key_F);
        QTest::qWait(300);
        QVERIFY(!document->selectedFolded());
        QCOMPARE(document->visibleCount(), visible);
        canvas->fit();
        stage("Expanded branch — descendants restored");
    }
    void manualDragMirrorsLiveAndMatchesDrop() {
        document->setManual(true);
        const int branch=document->nodes().value(1).children.first();
        const int child=document->nodes().value(branch).children.first();
        QTest::qWait(250);
        canvas->zoomAt({canvas->width()/2,canvas->height()/2},.6/canvas->zoom());
        canvas->panBy(canvas->width()*.5-canvas->mapFromWorld(document->nodes().value(1).rect.center()).x(),0);
        const auto original=document->nodes();
        const QPoint from=screenCenter(branch);
        const QPoint to(screenCenter(1).x()-110,from.y()+20);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(window,to,60);
        QTRY_VERIFY(canvas->dragging());
        QVERIFY(canvas->nodeRect(child).center().x()<canvas->nodeRect(branch).center().x());
        QCOMPARE(document->nodes().value(child).rect,original.value(child).rect);
        QTest::mouseMove(window,from,60);
        QVERIFY(canvas->nodeRect(child).center().x()>canvas->nodeRect(branch).center().x());
        QTest::mouseMove(window,to,60);
        const auto previewBranch=canvas->nodeRect(branch), previewChild=canvas->nodeRect(child);
        const QString dir=qEnvironmentVariable("MINDMAP_MIRROR_SCREENSHOTS");
        if(!dir.isEmpty()) { QDir().mkpath(dir); QTest::qWait(100); QVERIFY(window->grabWindow().save(dir+"/dragging-left.png")); }
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(document->nodes().value(branch).rect,previewBranch);
        QCOMPARE(document->nodes().value(child).rect,previewChild);
        QCOMPARE(canvas->nodeRect(child),previewChild);
        QTest::qWait(250); QCOMPARE(canvas->nodeRect(child),previewChild);
        if(!dir.isEmpty()) QVERIFY(window->grabWindow().save(dir+"/dropped-left.png"));
        document->undo(); QCOMPARE(document->nodes().value(child).rect,original.value(child).rect);
    }
    void manualDragMovesSubtree() {
        const int root = 1;
        QVERIFY(!document->nodes().value(root).children.isEmpty());
        const int branch = document->nodes().value(root).children.first();
        const int child = document->nodes().value(branch).children.first();
        document->setManual(true);
        QTest::qWait(300);
        canvas->fit();
        const QPointF before = document->nodes().value(branch).rect.center();
        const QPointF childBefore = document->nodes().value(child).rect.center();
        const QPoint from = screenCenter(branch);
        stage("Manual placement — dragging a whole subtree");
        drag(from, from + QPoint(40, 35));
        const QPointF delta = document->nodes().value(branch).rect.center() - before;
        QVERIFY(QLineF(QPointF(), delta).length() > 5);
        QVERIFY(QLineF(document->nodes().value(child).rect.center() - childBefore, delta).length() <
                0.01);
        stage("Subtree moved together");
        document->undo();
        QCOMPARE(document->nodes().value(branch).rect.center(), before);
        stage("Undo restored manual placement");
    }
    void automaticDragReordersSiblingsInGaps() {
        document->setManual(false);
        for (const QString layout : {QString("Horizontal"),QString("Vertical"),QString("Compact")}) {
            document->setLayout(layout); QTest::qWait(250); canvas->fit();
            const auto original=document->nodes().value(1).children;
            QVERIFY(original.size()>=3);
            const int moving=original[1], first=original[0];
            const auto rect=document->nodes().value(first).rect;
            const QPointF before=layout=="Vertical" ? QPointF(rect.left()-10,rect.center().y())
                                                       : QPointF(rect.center().x(),rect.top()-10);
            drag(screenCenter(moving),canvas->mapToScene(canvas->mapFromWorld(before)).toPoint());
            auto expected=original; expected.removeAll(moving); expected.prepend(moving);
            QCOMPARE(document->nodes().value(1).children,expected);
            QCOMPARE(document->nodes().value(moving).parent,1);
            document->undo(); QCOMPARE(document->nodes().value(1).children,original);
            QTest::qWait(250);
            const auto last=document->nodes().value(original.last()).rect;
            const QPointF after=layout=="Vertical" ? QPointF(last.right()+10,last.center().y())
                                                      : QPointF(last.center().x(),last.bottom()+10);
            drag(screenCenter(moving),canvas->mapToScene(canvas->mapFromWorld(after)).toPoint());
            expected=original; expected.removeAll(moving); expected.append(moving);
            QCOMPARE(document->nodes().value(1).children,expected);
            document->undo(); QCOMPARE(document->nodes().value(1).children,original);
            QTest::qWait(250);
            const auto third=document->nodes().value(original[2]).rect;
            const QPointF middle=layout=="Vertical" ? QPointF(third.left()-10,third.center().y())
                                                       : QPointF(third.center().x(),third.top()-10);
            drag(screenCenter(first),canvas->mapToScene(canvas->mapFromWorld(middle)).toPoint());
            expected=original; expected.removeAll(first); expected.insert(1,first);
            QCOMPARE(document->nodes().value(1).children,expected);
            document->undo(); QCOMPARE(document->nodes().value(1).children,original);
            document->redo(); QCOMPARE(document->nodes().value(1).children,expected);
            document->undo();
        }
        document->setLayout("Horizontal");
    }
    void automaticDragReparentsBranch() {
        const int root = 1;
        const auto siblings = document->nodes().value(root).children;
        QVERIFY(siblings.size() >= 2);
        const int branch = siblings.first(), target = siblings.at(1);
        document->setManual(false);
        document->setThemeId("lab");
        QTest::qWait(300);
        canvas->fit();
        stage("Automatic layout — drop branch onto another parent");
        drag(screenCenter(branch), screenCenter(target));
        QCOMPARE(document->nodes().value(branch).parent, target);
        stage("Branch attached to highlighted parent");
        document->undo();
        QCOMPARE(document->nodes().value(branch).parent, root);
        stage("Undo restored original hierarchy");
    }
    void rejectedDraftCannotBeReplacedByAnotherEditor() {
        const int originalId = document->selectedId();
        const int otherId = document->nodes().value(originalId).children.first();
        canvas->beginEdit(originalId);
        QTRY_VERIFY(editor->hasActiveFocus());
        const QString original = document->nodes().value(originalId).text;
        editor->setProperty("text", QString(17000, QLatin1Char('x')));
        canvas->beginEdit(otherId);
        QCOMPARE(canvas->editingId(), originalId);
        QVERIFY(editor->property("text").toString().size() >= 17000);
        QCOMPARE(document->nodes().value(originalId).text, original);
        QVERIFY(!document->error().isEmpty());
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    UiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ui_test.moc"
