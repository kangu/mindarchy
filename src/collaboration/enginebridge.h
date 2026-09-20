#pragma once

#include <QObject>
#include <QByteArray>

class Engine;

// Boundary between the existing local Engine and collaborative transactions.
// Remote application is explicitly scoped so model signals cannot echo edits
// back into the network outbox.
class CollaborationEngineBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool applyingRemote READ applyingRemote NOTIFY applyingRemoteChanged)
    Q_PROPERTY(bool undoConflict READ undoConflict NOTIFY undoConflictChanged)
public:
    explicit CollaborationEngineBridge(Engine *engine, QObject *parent = nullptr);
    bool applyingRemote() const { return m_applyingRemote; }
    bool undoConflict() const { return m_undoConflict; }
    Q_INVOKABLE void beginRemoteApply();
    Q_INVOKABLE void endRemoteApply();
    Q_INVOKABLE void recordLocalTransaction(const QByteArray &changes);
    Q_INVOKABLE void reportUndoConflict();
    Q_INVOKABLE void clearUndoConflict();
signals:
    void applyingRemoteChanged();
    void undoConflictChanged();
    void localTransaction(const QByteArray &changes);
private:
    Engine *m_engine = nullptr;
    bool m_applyingRemote = false;
    bool m_undoConflict = false;
};
