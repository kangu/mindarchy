#pragma once

#include "localstore.h"
#include <QObject>
#include <memory>

class CollaborationSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString mapId READ mapId NOTIFY mapChanged)
    Q_PROPERTY(QString role READ role NOTIFY mapChanged)
public:
    explicit CollaborationSession(QObject *parent = nullptr);
    QString status() const { return m_status; }
    QString mapId() const { return m_mapId; }
    QString role() const { return m_role; }
    Q_INVOKABLE bool openOffline(const QString &accountId, const QString &directory, const QString &mapId, const QString &role);
    Q_INVOKABLE bool saveLocal(const QByteArray &state, quint64 sequence, quint64 nextCounter);
    Q_INVOKABLE bool queueChange(quint64 counter, const QString &hash, const QByteArray &changes);
    Q_INVOKABLE void markSyncing();
    Q_INVOKABLE void markAccessRemoved();
signals:
    void statusChanged();
    void mapChanged();
private:
    void setStatus(const QString &status);
    std::unique_ptr<CollaborationLocalStore> m_store;
    QString m_status = "Local save failed";
    QString m_mapId;
    QString m_role;
};
