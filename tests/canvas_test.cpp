#include "canvas.h"
#include "viewportstate.h"
#include <QTemporaryDir>
#include "engine.h"
#include "drawing.h"
#include <QQuickWindow>
#include <QtTest>
class CanvasTest : public QObject {
    Q_OBJECT
  private slots:
    void viewportPersistsPerFileOutsideDocument() {
        QTemporaryDir dir;
        const auto path = dir.filePath("first.omm"), second = dir.filePath("second.omm");
        QSettings settings(dir.filePath("views.ini"), QSettings::IniFormat);
        Engine engine; QVERIFY(engine.save(path));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes=file.readAll(); file.close();
        QPointF center;
        double zoom;
        {
            MindCanvas canvas; canvas.setSize({1000,700}); canvas.setEngine(&engine);
            ViewportState state(&canvas,&engine,&settings);
            canvas.initializeView(); canvas.zoomIn(); canvas.panBy(213,-127);
            center=canvas.mapToWorld({500,350}); zoom=canvas.zoom();
        }
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),bytes); file.close();
        QVERIFY(engine.open(path));
        MindCanvas canvas; canvas.setSize({800,600}); canvas.setEngine(&engine);
        ViewportState state(&canvas,&engine,&settings); canvas.initializeView();
        QCOMPARE(canvas.zoom(),zoom); QVERIFY(QLineF(canvas.mapToWorld({400,300}),center).length()<.000001);
        canvas.setSize({1200,900});
        QVERIFY(QLineF(canvas.mapToWorld({600,450}),center).length()<.000001);
        QVERIFY(engine.save(second)); canvas.panBy(50,30); state.flush();
        QVERIFY(engine.open(path)); canvas.initializeView();
        QCOMPARE(canvas.zoom(),zoom); QVERIFY(QLineF(canvas.mapToWorld({600,450}),center).length()<.000001);
        settings.setValue(ViewportState::keyFor(path),QVariantList{0.,0.,0.});
        // A separate new canvas must fall back to fitting an invalid stored view.
        Engine other; QVERIFY(other.open(path));
        MindCanvas fallback; fallback.setSize({1000,700}); fallback.setEngine(&other);
        ViewportState invalid(&fallback,&other,&settings); fallback.initializeView();
        QVERIFY(fallback.zoom()>0); QVERIFY(std::isfinite(fallback.panX()));
    }
    void automaticReorderAcceptsDistantEdgeDrops() {
        Engine engine; engine.loadFixture(15); engine.setManual(false);
        MindCanvas canvas; canvas.setSize({1000,700}); canvas.setEngine(&engine);
        for (const QString layout : {QString("Horizontal"),QString("Vertical"),QString("Compact")}) {
            engine.setLayout(layout); canvas.m_animating=false; canvas.fit();
            const auto siblings=engine.nodes().value(1).children;
            QVERIFY(siblings.size()>=3);
            canvas.m_pressedId=siblings[1]; canvas.m_dragIds={siblings[1]};
            QRectF bounds;
            for (int id : siblings) if (id!=siblings[1]) bounds=bounds.united(engine.nodes().value(id).rect);
            for (qreal distance : {100.,1000.}) {
                for (qreal crossOffset : {-1000.,0.,1000.}) {
                    for (bool before : {true,false}) {
                        const QPointF world=layout=="Vertical"
                            ? QPointF(before ? bounds.left()-distance : bounds.right()+distance,bounds.center().y()+crossOffset)
                            : QPointF(bounds.center().x()+crossOffset,before ? bounds.top()-distance : bounds.bottom()+distance);
                        canvas.updateDrop(canvas.mapFromWorld(world));
                        QCOMPARE(canvas.m_dropParent,1);
                        QCOMPARE(canvas.m_before,before ? siblings.first() : -1);
                        QVERIFY(!canvas.m_dropLine.isNull());
                    }
                }
            }
        }
    }
    void calendarCellsHaveUniformFilledBounds() {
        CalendarData data; data.anchor=QDate::currentDate();
        for (const auto &day : Calendar::days(data)) data.entries.insert(day.toString(Qt::ISODate), "Entry");
        NodeAppearance style; style.branch=QColor("#faab78"); style.text=Qt::black;
        constexpr int scale=4;
        QImage image(294*scale,114*scale,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        QPainter painter(&image); painter.scale(scale,scale);
        MapDrawing::paintCalendar(painter,data,style); painter.end();
        for (int i=0;i<7;++i) {
            const auto cell=Calendar::cell(i);
            QRect painted;
            for (int y=68*scale;y<100*scale;++y)
                for (int x=int(cell.left()-2)*scale;x<int(cell.right()+2)*scale;++x)
                    if (image.pixelColor(x,y).alpha()>0) painted=painted.united(QRect(x,y,1,1));
            QCOMPARE(painted, QRect(int(cell.x())*scale,int(cell.y())*scale,34*scale,28*scale));
        }
    }
    void zoomDetailAndJoinedStrokes() {
        using namespace MapDrawing;
        const QPolygonF corner{QPointF(0,0), QPointF(20,0), QPointF(20,20)};
        const auto mesh=strokeTriangles(corner,4);
        QCOMPARE(mesh.size(), 12);
        QCOMPARE(mesh[2], mesh[6]);
        QCOMPARE(mesh[5], mesh[7]);
        QVERIFY(mesh.contains(QPointF(22,-2)));
        const auto low=edgePath({0,0},{400,180},false,false,1);
        const auto high=edgePath({0,0},{400,180},false,false,16);
        QVERIFY(high.size()>low.size());
        QCOMPARE(high.first(), low.first()); QCOMPARE(high.last(), low.last());
        QVERIFY(shapePolygon({0,0,200,60},NodeShape::Rounded,12,16).size()>
                shapePolygon({0,0,200,60},NodeShape::Rounded,12,1).size());
        QVERIFY(!edgePath({0,0},{0,0},false,false,16).isEmpty());
        Engine engine(nullptr, Engine::InitialContent::Blank);
        MindCanvas canvas; canvas.setSize({1000,700}); canvas.setEngine(&engine); canvas.fit();
        const auto normal=canvas.m_cache.value(1).image;
        canvas.m_zoom=8; canvas.revealNode(1); canvas.refresh();
        const auto zoomed=canvas.m_cache.value(1).image;
        QVERIFY(zoomed.devicePixelRatio()>normal.devicePixelRatio());
        QVERIFY(zoomed.width()>normal.width());
        QVERIFY(zoomed.width()<=4096 && zoomed.height()<=4096);
        const QString screenshot=qEnvironmentVariable("MINDMAP_ZOOM_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QQuickWindow window; window.resize(1280,900);
            auto format=window.format(); format.setSamples(4); window.setFormat(format);
            auto *visual=new MindCanvas(window.contentItem()); visual->setSize({1280,900});
            engine.loadFixture(15); engine.select(1);
            visual->setEngine(&engine); visual->m_animating=false; visual->m_zoom=6;
            const auto root=engine.nodes().value(1).rect;
            visual->m_pan=QPointF(600,450)-QPointF(root.right(),root.center().y())*6;
            visual->refresh(); window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window)); QTest::qWait(250);
            QVERIFY(window.grabWindow().save(screenshot));
        }
    }
    void taskTransitionKeepsTextLayoutStable() {
        Engine engine(nullptr, Engine::InitialContent::Blank);
        engine.setText(1, "seems to be working");
        MindCanvas canvas;
        canvas.setSize({1000, 700}); canvas.setEngine(&engine); canvas.fit();
        const qreal plainWidth = engine.nodes().value(1).rect.width();
        for (bool enabled : {true, false}) {
            engine.toggleTask();
            const QSizeF targetSize = engine.nodes().value(1).rect.size();
            QCOMPARE(canvas.m_cache.value(1).size, targetSize);
            const auto texture = canvas.m_cache.value(1).key;
            for (int frame=0; frame<5; ++frame) {
                QTest::qWait(22); canvas.refresh();
                QCOMPARE(canvas.m_cache.value(1).key, texture);
                QCOMPARE(canvas.m_cache.value(1).size, targetSize);
                const auto node = canvas.m_draw.first();
                const auto label = canvas.m_labels.first();
                QVERIFY(node.taskOpacity > 0 && node.taskOpacity < 1);
                QVERIFY(qAbs(node.rect.width()-plainWidth-20*node.taskOpacity) < 1);
                QVERIFY(qAbs(label.rect.left()-node.rect.left()+20*(enabled?1:0)-20*node.taskOpacity) < 1);
            }
            QTest::qWait(100); canvas.refresh();
            QCOMPARE(canvas.m_draw.first().taskOpacity, enabled ? 1. : 0.);
            QCOMPARE(canvas.m_cache.value(1).key, texture);
        }
    }
    void parentTaskUsesReadOnlyProgressRing() {
        Engine engine(nullptr,Engine::InitialContent::Blank);
        engine.addChild(); const int first=engine.selectedId();
        engine.addSibling(); engine.select(1); engine.toggleTask();
        engine.select(first); engine.toggleChecked();
        MindCanvas canvas; canvas.setSize({1000,700}); canvas.setEngine(&engine); canvas.resetZoom(); canvas.revealNode(1);
        canvas.m_animating=false; canvas.refresh();
        bool found=false;
        for(const auto &node:canvas.m_draw) if(node.id==1) {
            found=true; QCOMPARE(node.completion,.5);
            const QPointF point=canvas.mapFromWorld(QPointF(node.rect.left()+13,node.rect.center().y()));
            QCOMPARE(canvas.taskHit(point),-1);
        }
        QVERIFY(found);
        if(qEnvironmentVariableIsSet("MINDMAP_TASK_RING_SCREENSHOT")) {
            QQuickWindow window; window.resize(1000,700);
            canvas.setParentItem(window.contentItem()); window.show(); QTest::qWait(250);
            QVERIFY(window.grabWindow().save(qEnvironmentVariable("MINDMAP_TASK_RING_SCREENSHOT")));
            canvas.setParentItem(nullptr);
        }
    }
    void taskCheckboxInteraction() {
        Engine engine(nullptr,Engine::InitialContent::Blank);
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
