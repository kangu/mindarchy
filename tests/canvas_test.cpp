#include "canvas.h"
#include "engine.h"
#include <QQuickWindow>
#include <QtTest>
class CanvasTest : public QObject {
    Q_OBJECT
  private slots:
    void taskCheckboxInteraction() {
        Engine engine;
        engine.select(1);
        engine.toggleChecked();
        QQuickWindow window;
        window.resize(1000, 700);
        auto *canvas = new MindCanvas(window.contentItem());
        canvas->setSize({1000, 700});
        canvas->setEngine(&engine);
        canvas->resetZoom();
        canvas->revealNode(1);
        window.show();
        QTest::qWait(250);
        const QRectF original = canvas->nodeRect(1);
        const bool checked = engine.nodes().value(1).checked;
        const QPoint target = canvas->mapFromWorld(
            QPointF(original.left() + 13, original.center().y())).toPoint();
        // Ten pixels below the glyph is still comfortably clickable.
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, target + QPoint(0, 10));
        QCOMPARE(engine.nodes().value(1).checked, !checked);
        QCOMPARE(canvas->nodeRect(1), original);
        engine.undo();
        QCOMPARE(engine.nodes().value(1).checked, checked);
        QTest::mouseMove(&window, target + QPoint(0, 10));
        QTRY_COMPARE(canvas->cursor().shape(), Qt::PointingHandCursor);
        const QPoint text = canvas->mapFromWorld(
            QPointF(original.left() + 45, original.center().y())).toPoint();
        QTest::mouseMove(&window, text);
        QTRY_COMPARE(canvas->cursor().shape(), Qt::ArrowCursor);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, text);
        QCOMPARE(engine.nodes().value(1).checked, checked);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::ShiftModifier, target);
        QCOMPARE(engine.nodes().value(1).checked, checked);
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, target);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, text);
        QCOMPARE(engine.nodes().value(1).checked, checked);
        engine.setManual(true);
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, target);
        QTest::mouseMove(&window, target + QPoint(70, 50), 30);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, target + QPoint(70, 50));
        QCOMPARE(engine.nodes().value(1).checked, checked);
        QVERIFY(!canvas->editing());
    }
    void cursorAnchoredZoom() {
        Engine engine;
        MindCanvas canvas;
        canvas.setWidth(1000);
        canvas.setHeight(700);
        canvas.setEngine(&engine);
        canvas.fit();
        QPointF p(370, 240);
        auto before = canvas.mapToWorld(p);
        double oldZoom = canvas.zoom();
        canvas.zoomAt(p, 1.5);
        QVERIFY(canvas.zoom() > oldZoom);
        QVERIFY(QLineF(before, canvas.mapToWorld(p)).length() < 0.00001);
    }
    void keyboardModes() {
        Engine engine;
        QQuickWindow window;
        window.resize(1000, 700);
        auto *canvas = new MindCanvas(window.contentItem());
        canvas->setSize({1000, 700});
        canvas->setEngine(&engine);
        window.show();
        canvas->forceActiveFocus();
        const int original = engine.nodeCount();
        engine.select(1);
        QTest::keyClick(&window, Qt::Key_Tab);
        QCOMPARE(engine.nodeCount(), original + 1);
        QVERIFY(canvas->editing());
        canvas->endEdit();
        int count = engine.nodeCount();
        QTest::keyClick(&window, Qt::Key_Return);
        QCOMPARE(engine.nodeCount(), count + 1);
        QVERIFY(canvas->editing());
    }
    void dragManualIsOneUndoableChange() {
        Engine engine;
        QQuickWindow window;
        window.resize(1000, 700);
        auto *canvas = new MindCanvas(window.contentItem());
        canvas->setSize({1000, 700});
        canvas->setEngine(&engine);
        canvas->fit();
        window.show();
        engine.setManual(true);
        int id = engine.visibleIds().last();
        engine.select(id);
        QPointF original = engine.nodes().value(id).manualOffset;
        QPointF p = canvas->mapFromWorld(engine.nodes().value(id).rect.center());
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, p.toPoint());
        QTest::mouseMove(&window, (p + QPointF(40, 50)).toPoint(), 30);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier,
                            (p + QPointF(40, 50)).toPoint());
        QVERIFY(engine.nodes().value(id).manualOffset != original);
        engine.undo();
        QCOMPARE(engine.nodes().value(id).manualOffset, original);
    }
};
QTEST_MAIN(CanvasTest)
#include "canvas_test.moc"
