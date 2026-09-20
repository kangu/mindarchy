#include "collaboration/localstore.h"
#include <QTemporaryDir>
#include <QtTest>

class CollaborationStoreTest : public QObject {
    Q_OBJECT
private slots:
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
        QCOMPARE(counter, quint64(9));
        const auto pending = reopened.pending("map-a");
        QCOMPARE(pending.size(), 1);
        QVERIFY(reopened.removePending(pending.first().id));
        QVERIFY(reopened.pending("map-a").isEmpty());
        QVERIFY(reopened.saveRejected("map-a", "rejected", "access_revoked"));
    }
};

QTEST_MAIN(CollaborationStoreTest)
#include "collaboration_store_test.moc"
