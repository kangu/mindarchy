#include "enginebridge.h"

#include "../engine.h"

CollaborationEngineBridge::CollaborationEngineBridge(Engine *engine, QObject *parent)
    : QObject(parent), m_engine(engine) {}

void CollaborationEngineBridge::beginRemoteApply() {
    if (m_applyingRemote) return;
    m_applyingRemote = true;
    emit applyingRemoteChanged();
}

void CollaborationEngineBridge::endRemoteApply() {
    if (!m_applyingRemote) return;
    m_applyingRemote = false;
    emit applyingRemoteChanged();
}

void CollaborationEngineBridge::recordLocalTransaction(const QByteArray &changes) {
    if (m_applyingRemote || changes.isEmpty()) return;
    emit localTransaction(changes);
}

void CollaborationEngineBridge::reportUndoConflict() {
    if (m_undoConflict) return;
    m_undoConflict = true;
    emit undoConflictChanged();
}

void CollaborationEngineBridge::clearUndoConflict() {
    if (!m_undoConflict) return;
    m_undoConflict = false;
    emit undoConflictChanged();
}
