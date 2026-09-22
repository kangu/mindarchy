#include "collaboration/localstore.h"
#include "collaboration/session.h"
#include <QTemporaryDir>
#include <QtTest>

class CollaborationStoreTest : public QObject {
    Q_OBJECT
private slots:
    void operationPayloadSurvivesRestart() {
        QTemporaryDir directory;const QByteArray payload="{\"version\":2,\"id\":\"stable-id\",\"ops\":[]}";
        {CollaborationSession session;QVERIFY(session.openOffline("server-account",directory.path(),"map","owner"));QVERIFY(session.queueChange(42,"sha",payload));}
        CollaborationSession session;QVERIFY(session.openOffline("server-account",directory.path(),"map","owner"));QCOMPARE(session.nextCounter(),quint64(43));QCOMPARE(session.pendingForSubmit().first().changes,payload);
        session.clearOperation("stable-id","wrong");QCOMPARE(session.pendingForSubmit().size(),1);session.clearOperation("stable-id","sha");QVERIFY(session.pendingForSubmit().isEmpty());QCOMPARE(session.nextCounter(),quint64(43));
    }
    void persistsAcceptedAndPendingState() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        {
            CollaborationLocalStore store("account-a", directory.path());
            QVERIFY2(store.open(), qPrintable(store.error()));
            QVERIFY(store.saveAccepted("map-a", "accepted", 7, 9));
            QVERIFY(store.enqueue("map-a", 9, "hash", "change"));
            const auto pending = store.pending("map-a");
            QCOMPARE(pending.size(), 1);
            QCOMPARE(pending.first().counter, quint64(9));
        }
        CollaborationLocalStore reopened("account-a", directory.path());
        QVERIFY2(reopened.open(), qPrintable(reopened.error()));
        quint64 sequence = 0, counter = 0;
        QCOMPARE(reopened.accepted("map-a", &sequence, &counter), QByteArray("accepted"));
        QCOMPARE(sequence, quint64(7));
        QCOMPARE(counter, quint64(10));
        const auto pending = reopened.pending("map-a");
        QCOMPARE(pending.size(), 1);
        QVERIFY(reopened.removePending(pending.first().id));
        QVERIFY(reopened.pending("map-a").isEmpty());
        QVERIFY(reopened.saveRejected("map-a", "rejected", "access_revoked"));
    }
};

QTEST_MAIN(CollaborationStoreTest)
#include "collaboration_store_test.moc"
