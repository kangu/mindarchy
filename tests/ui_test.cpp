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
    void cleanupTestCase() {
        delete qml;
        qml = nullptr;
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
