#pragma once

#include <QByteArray>
#include <QList>
#include <QSqlDatabase>
#include <QString>

struct CollaborationPending {
    qint64 id = 0;
    QString mapId;
    quint64 counter = 0;
    QString hash;
    QByteArray changes;
};

// Account-scoped durable state for shared maps. Portable .omm files never use
// this database and therefore never receive credentials or collaboration data.
class CollaborationLocalStore {
public:
    CollaborationLocalStore(const QString &accountId, const QString &directory);
    ~CollaborationLocalStore();
    CollaborationLocalStore(const CollaborationLocalStore &) = delete;
    CollaborationLocalStore &operator=(const CollaborationLocalStore &) = delete;

    bool open();
    bool isOpen() const { return m_database.isOpen(); }
    QString error() const { return m_error; }
    bool saveAccepted(const QString &mapId, const QByteArray &state, quint64 sequence, quint64 nextCounter);
    QByteArray accepted(const QString &mapId, quint64 *sequence = nullptr, quint64 *nextCounter = nullptr) const;
    bool enqueue(const QString &mapId, quint64 counter, const QString &hash, const QByteArray &changes);
    QList<CollaborationPending> pending(const QString &mapId) const;
    bool removePending(qint64 id);
    bool saveRejected(const QString &mapId, const QByteArray &changes, const QString &reason);

private:
    bool exec(const QString &sql) const;
    void fail(const QString &message) const;
    QString m_accountId;
    QString m_directory;
    QString m_connectionName;
    mutable QString m_error;
    QSqlDatabase m_database;
};
