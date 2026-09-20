#include "localstore.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

CollaborationLocalStore::CollaborationLocalStore(const QString &accountId, const QString &directory)
    : m_accountId(accountId), m_directory(directory), m_connectionName("mindarchy-collab-" + QUuid::createUuid().toString(QUuid::WithoutBraces)) {}

CollaborationLocalStore::~CollaborationLocalStore() {
    if (m_database.isValid()) {
        const auto name = m_connectionName;
        m_database.close();
        m_database = {};
        QSqlDatabase::removeDatabase(name);
    }
}

bool CollaborationLocalStore::open() {
    if (m_database.isOpen()) return true;
    if (m_accountId.isEmpty() || m_directory.isEmpty()) { fail("account and directory are required"); return false; }
    const auto accountDirectory = QDir(m_directory).filePath(m_accountId);
    if (!QDir().mkpath(accountDirectory)) { fail("could not create account collaboration directory"); return false; }
    m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_database.setDatabaseName(QDir(accountDirectory).filePath("collaboration.sqlite"));
    if (!m_database.open()) { fail(m_database.lastError().text()); return false; }
    if (!exec("PRAGMA foreign_keys = ON") ||
        !exec("CREATE TABLE IF NOT EXISTS maps (map_id TEXT PRIMARY KEY, accepted BLOB NOT NULL, sequence INTEGER NOT NULL, next_counter INTEGER NOT NULL)" ) ||
        !exec("CREATE TABLE IF NOT EXISTS pending (id INTEGER PRIMARY KEY AUTOINCREMENT, map_id TEXT NOT NULL, counter INTEGER NOT NULL, hash TEXT NOT NULL, changes BLOB NOT NULL, UNIQUE(map_id, counter))") ||
        !exec("CREATE TABLE IF NOT EXISTS rejected (id INTEGER PRIMARY KEY AUTOINCREMENT, map_id TEXT NOT NULL, changes BLOB NOT NULL, reason TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)")) return false;
    return true;
}

bool CollaborationLocalStore::saveAccepted(const QString &mapId, const QByteArray &state, quint64 sequence, quint64 nextCounter) {
    if (!open()) return false;
    QSqlQuery query(m_database); query.prepare("INSERT INTO maps(map_id, accepted, sequence, next_counter) VALUES(?, ?, ?, ?) ON CONFLICT(map_id) DO UPDATE SET accepted=excluded.accepted, sequence=excluded.sequence, next_counter=excluded.next_counter");
    query.addBindValue(mapId); query.addBindValue(state); query.addBindValue(QVariant::fromValue<qulonglong>(sequence)); query.addBindValue(QVariant::fromValue<qulonglong>(nextCounter));
    if (!query.exec()) { fail(query.lastError().text()); return false; } return true;
}

QByteArray CollaborationLocalStore::accepted(const QString &mapId, quint64 *sequence, quint64 *nextCounter) const {
    if (!const_cast<CollaborationLocalStore *>(this)->open()) return {};
    QSqlQuery query(m_database); query.prepare("SELECT accepted, sequence, next_counter FROM maps WHERE map_id=?"); query.addBindValue(mapId);
    if (!query.exec() || !query.next()) return {};
    if (sequence) *sequence = query.value(1).toULongLong(); if (nextCounter) *nextCounter = query.value(2).toULongLong(); return query.value(0).toByteArray();
}

bool CollaborationLocalStore::enqueue(const QString &mapId, quint64 counter, const QString &hash, const QByteArray &changes) {
    if (!open()) return false;
    QSqlQuery query(m_database); query.prepare("INSERT INTO pending(map_id, counter, hash, changes) VALUES(?, ?, ?, ?) ON CONFLICT(map_id, counter) DO NOTHING"); query.addBindValue(mapId); query.addBindValue(QVariant::fromValue<qulonglong>(counter)); query.addBindValue(hash); query.addBindValue(changes);
    if (!query.exec()) { fail(query.lastError().text()); return false; } return true;
}

QList<CollaborationPending> CollaborationLocalStore::pending(const QString &mapId) const {
    QList<CollaborationPending> result; if (!const_cast<CollaborationLocalStore *>(this)->open()) return result;
    QSqlQuery query(m_database); query.prepare("SELECT id, map_id, counter, hash, changes FROM pending WHERE map_id=? ORDER BY counter"); query.addBindValue(mapId);
    if (!query.exec()) return result; while (query.next()) result.append({query.value(0).toLongLong(), query.value(1).toString(), query.value(2).toULongLong(), query.value(3).toString(), query.value(4).toByteArray()}); return result;
}

bool CollaborationLocalStore::removePending(qint64 id) { if (!open()) return false; QSqlQuery query(m_database); query.prepare("DELETE FROM pending WHERE id=?"); query.addBindValue(id); if (!query.exec()) { fail(query.lastError().text()); return false; } return true; }
bool CollaborationLocalStore::saveRejected(const QString &mapId, const QByteArray &changes, const QString &reason) { if (!open()) return false; QSqlQuery query(m_database); query.prepare("INSERT INTO rejected(map_id, changes, reason) VALUES(?, ?, ?)"); query.addBindValue(mapId); query.addBindValue(changes); query.addBindValue(reason); if (!query.exec()) { fail(query.lastError().text()); return false; } return true; }
bool CollaborationLocalStore::exec(const QString &sql) const { QSqlQuery query(m_database); if (query.exec(sql)) return true; fail(query.lastError().text()); return false; }
void CollaborationLocalStore::fail(const QString &message) const { m_error = message; }
