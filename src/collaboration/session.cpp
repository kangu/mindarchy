#include "session.h"
#include <QJsonDocument>
#include <QJsonObject>

CollaborationSession::CollaborationSession(QObject *parent) : QObject(parent) {}

bool CollaborationSession::openOffline(const QString &accountId, const QString &directory, const QString &mapId, const QString &role) {
    auto store = std::make_unique<CollaborationLocalStore>(accountId, directory);
    if (!store->open()) { setStatus("Local save failed"); return false; }
    m_store = std::move(store); m_mapId = mapId; m_role = role; setStatus("Saved locally"); emit mapChanged(); return true;
}

bool CollaborationSession::saveLocal(const QByteArray &state, quint64 sequence, quint64 nextCounter) {
    if (!m_store || !m_store->saveAccepted(m_mapId, state, sequence, nextCounter)) { setStatus("Local save failed"); return false; }
    setStatus("Saved locally"); return true;
}

bool CollaborationSession::queueChange(quint64 counter, const QString &hash, const QByteArray &changes) {
    if (!m_store || !m_store->enqueue(m_mapId, counter, hash, changes)) { setStatus("Local save failed"); return false; }
    setStatus("Saved locally / Waiting to sync"); return true;
}

void CollaborationSession::clearPendingByCounter(const QString &mapId, quint64 counter) {
    if (!m_store) return;
    const auto rows = m_store->pending(mapId);
    for (const auto &row : rows)
        if (row.counter == counter) m_store->removePending(row.id);
}

void CollaborationSession::markSyncing() { setStatus("Syncing"); }
void CollaborationSession::markAccessRemoved() { setStatus("Access removed"); }
void CollaborationSession::setStatus(const QString &status) { if (m_status == status) return; m_status = status; emit statusChanged(); }

quint64 CollaborationSession::nextCounter() const {
 quint64 next=1;if(m_store)m_store->accepted(m_mapId,nullptr,&next);
 for(const auto &row:pendingForSubmit())next=qMax(next,row.counter+1);return next;
}
void CollaborationSession::clearOperation(const QString &id,const QString &hash) {
 for(const auto &row:pendingForSubmit())if(row.hash==hash&&QJsonDocument::fromJson(row.changes).object()["id"].toString()==id)m_store->removePending(row.id);
}
