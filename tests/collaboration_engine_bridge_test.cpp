#include "collaboration/enginebridge.h"
#include <QSignalSpy>
#include <QtTest>

class CollaborationEngineBridgeTest : public QObject {
    Q_OBJECT
private slots:
    void remoteTransactionsDoNotEcho() {
        CollaborationEngineBridge bridge(nullptr);
        QSignalSpy transactions(&bridge, &CollaborationEngineBridge::localTransaction);
        bridge.beginRemoteApply();
        bridge.recordLocalTransaction("remote");
        QCOMPARE(transactions.size(), 0);
        bridge.endRemoteApply();
        bridge.recordLocalTransaction("local");
        QCOMPARE(transactions.size(), 1);
        QCOMPARE(transactions.first().first().toByteArray(), QByteArray("local"));
    }
    void undoConflictIsObservable() {
        CollaborationEngineBridge bridge(nullptr);
        QVERIFY(!bridge.undoConflict());
        bridge.reportUndoConflict();
        QVERIFY(bridge.undoConflict());
        bridge.clearUndoConflict();
        QVERIFY(!bridge.undoConflict());
    }
};

QTEST_MAIN(CollaborationEngineBridgeTest)
#include "collaboration_engine_bridge_test.moc"
