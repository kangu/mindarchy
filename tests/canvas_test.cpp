#include "canvas.h"
#include "engine.h"
#include <QQuickWindow>
#include <QtTest>
class CanvasTest : public QObject {
    Q_OBJECT
  private slots:
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
