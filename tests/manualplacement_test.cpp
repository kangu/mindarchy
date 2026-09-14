#include "engine.h"
#include <QtTest>
#include <QTemporaryDir>
class ManualPlacementTest:public QObject {
    Q_OBJECT
private slots:
    void foldingPreservesPositions_data() { afterEntireSubtree_data(); }
    void foldingPreservesPositions() {
        QFETCH(bool,vertical); QFETCH(bool,left);
        Engine e(nullptr,Engine::InitialContent::Blank);
        if(vertical) e.setLayout("Vertical");
        e.setManual(true);
        auto p=[&](qreal x,qreal y) { return vertical?QPointF(y,x):QPointF(left?-x:x,y); };
        e.addChildFromPointer(1,p(250,100)); const int parent=e.selectedId();
        e.addChildFromPointer(parent,p(500,50)); const int child=e.selectedId();
        e.addChildFromPointer(child,p(800,-100)); const int grandchild=e.selectedId();
        e.addChildFromPointer(parent,p(500,350));
        e.addChildFromPointer(1,p(250,600));
        const auto before=e.nodes();
        auto checkPositions=[&](const Engine &engine) {
            for(int id:engine.visibleIds())
                QVERIFY2(QLineF(before.value(id).rect.center(),engine.nodes().value(id).rect.center()).length()<.001,
                         qPrintable(QString("Node %1 moved after folding/expanding").arg(id)));
        };
        e.select(child); e.toggleFold(); checkPositions(e);
        QVERIFY(!e.visibleIds().contains(grandchild));
        e.select(parent); e.toggleFold(); checkPositions(e);
        QVERIFY(!e.visibleIds().contains(child));
        e.undo(); checkPositions(e);
        e.redo(); checkPositions(e);
        QTemporaryDir dir; QVERIFY(e.save(dir.filePath("folded.omm")));
        Engine restored; QVERIFY(restored.open(dir.filePath("folded.omm")));
        checkPositions(restored);
        restored.select(parent); restored.toggleFold(); checkPositions(restored);
        QVERIFY(!restored.visibleIds().contains(grandchild));
        restored.select(child); restored.toggleFold(); checkPositions(restored);
        QCOMPARE(restored.visibleCount(),before.size());
    }
    void afterEntireSubtree_data() {
        QTest::addColumn<bool>("vertical"); QTest::addColumn<bool>("left");
        QTest::newRow("right")<<false<<false;
        QTest::newRow("left")<<false<<true;
        QTest::newRow("vertical")<<true<<false;
    }
    void afterEntireSubtree() {
        QFETCH(bool,vertical); QFETCH(bool,left);
        Engine e(nullptr,Engine::InitialContent::Blank);
        if(vertical)e.setLayout("Vertical"); e.setManual(true);
        auto p=[&](qreal x,qreal y){return vertical?QPointF(y,x):QPointF(left?-x:x,y);};
        e.addChildFromPointer(1,p(250,100)); const int parent=e.selectedId();
        e.addChildFromPointer(parent,p(500,100)); const int sibling=e.selectedId();
        e.addChildFromPointer(sibling,p(800,500)); const int descendant=e.selectedId();
        QVERIFY(e.setNodeKind(descendant,"date"));
        QVERIFY(e.configureDateNode(descendant,"month","2026-09-01"));
        const auto old=e.nodes(); e.select(parent); e.addChild(); const int added=e.selectedId();
        const auto rect=e.nodes().value(added).rect;
        for(auto it=old.begin();it!=old.end();++it) {
            QVERIFY(QLineF(it->rect.center(),e.nodes().value(it.key()).rect.center()).length()<.001);
            QVERIFY(!rect.intersects(it->rect));
        }
        QVERIFY(vertical?rect.left()>old.value(descendant).rect.right():rect.top()>old.value(descendant).rect.bottom());
        if(!vertical) QVERIFY(qAbs((left?rect.right():rect.left())-(left?old.value(sibling).rect.right():old.value(sibling).rect.left()))<.001);
        e.undo(); QCOMPARE(e.nodeCount(),old.size());
        e.redo(); QCOMPARE(e.nodes().value(added).rect,rect);
        QTemporaryDir dir; QVERIFY(e.save(dir.filePath("manual.omm")));
        Engine restored; QVERIFY(restored.open(dir.filePath("manual.omm")));
        QCOMPARE(restored.nodes().value(added).rect,rect);
    }
    void unrelatedObstacleAndPointerOverride() {
        Engine e(nullptr,Engine::InitialContent::Blank);e.setManual(true);
        e.addChildFromPointer(1,QPointF(250,100));const int parent=e.selectedId();
        e.addChildFromPointer(parent,QPointF(500,100));
        e.addChildFromPointer(1,QPointF(500,174));const int obstacle=e.selectedId();
        const auto before=e.nodes();e.select(parent);e.addChild();
        const auto added=e.nodes().value(e.selectedId()).rect;
        QVERIFY(!added.intersects(before.value(obstacle).rect));
        for(auto it=before.begin();it!=before.end();++it)
            QVERIFY(QLineF(it->rect.center(),e.nodes().value(it.key()).rect.center()).length()<.001);
        const QPointF explicitPoint(500,174);e.addChildFromPointer(parent,explicitPoint);
        QCOMPARE(e.nodes().value(e.selectedId()).rect.center(),explicitPoint);
    }
    void foldedParentAndAscendingSiblings() {
        Engine e(nullptr,Engine::InitialContent::Blank);e.setManual(true);
        e.addChildFromPointer(1,QPointF(250,100));const int parent=e.selectedId();
        e.addChildFromPointer(parent,QPointF(500,100));
        e.addChildFromPointer(parent,QPointF(500,-100));const int upper=e.selectedId();
        e.select(parent);e.addChild();
        QVERIFY(e.nodes().value(e.selectedId()).rect.bottom()<e.nodes().value(upper).rect.top());
        e.select(parent);e.toggleFold();e.addChild();const int added=e.selectedId();
        QVERIFY(!e.nodes().value(parent).folded);
        const auto rect=e.nodes().value(added).rect;
        for(int id:e.visibleIds()) if(id!=added) QVERIFY(!rect.intersects(e.nodes().value(id).rect));
    }
    void firstChildAndSiblingGap() {
        Engine e(nullptr,Engine::InitialContent::Blank);e.setManual(true);
        const auto root=e.nodes().value(1).rect;e.addChild();
        QVERIFY(e.nodes().value(e.selectedId()).rect.left()>root.right());
        QCOMPARE(e.nodes().value(1).rect,root);
        const int first=e.selectedId();e.setText(first,"First");
        e.addChildFromPointer(1,QPointF(e.nodes().value(first).rect.center().x(),400));
        const auto before=e.nodes();e.select(first);e.addSibling();
        const auto added=e.nodes().value(e.selectedId()).rect;
        QVERIFY(added.top()>before.value(first).rect.bottom());
        QVERIFY(added.bottom()<before.value(before.value(1).children.last()).rect.top());
        for(auto it=before.begin();it!=before.end();++it)
            QVERIFY(QLineF(it->rect.center(),e.nodes().value(it.key()).rect.center()).length()<.001);
    }
};
QTEST_MAIN(ManualPlacementTest)
#include "manualplacement_test.moc"
