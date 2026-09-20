#include "session.h"

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

void CollaborationSession::markSyncing() { setStatus("Syncing"); }
void CollaborationSession::markAccessRemoved() { setStatus("Access removed"); }
void CollaborationSession::setStatus(const QString &status) { if (m_status == status) return; m_status = status; emit statusChanged(); }
