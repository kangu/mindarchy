#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QTimer>
#include "credentialstore.h"

class ShareClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY signedInChanged)
public:
    explicit ShareClient(QObject *parent = nullptr);
    ~ShareClient() override;
    void enableRememberedLogin(std::shared_ptr<ShareCredentialStore> store = systemShareCredentialStore());
    bool restoreLogin();
    bool rememberedLogin() const { return m_rememberedLogin; }
    bool reconnecting() const { return m_loginInFlight; }

    Q_INVOKABLE virtual void setBaseUrl(const QString &url);
    Q_INVOKABLE virtual void login(const QString &username, const QString &password);
    Q_INVOKABLE void account();
    void signOut();
    Q_INVOKABLE virtual void maps();
    Q_INVOKABLE virtual void createMap(const QByteArray &snapshot, const QString &name = {});
    Q_INVOKABLE void invite(const QString &mapId, const QString &account, const QString &role);
    Q_INVOKABLE virtual void acceptInvite(const QString &token);
    Q_INVOKABLE virtual void fetchMapState(const QString &mapId);
    Q_INVOKABLE QString mapState(const QString &mapId) const;

    virtual QVariantList mapSummaries() const;

    virtual bool signedIn() const;
    virtual QString accountName() const;
    QString baseUrl() const { return m_baseUrl; }
    QNetworkCookieJar *cookieJar() const { return m_network->cookieJar(); }

signals:
    void signedInChanged();
    void sessionRenewed();
    void reconnectingChanged();
    void rememberedLoginChanged();
    void sessionMessage(const QString &message);
    void loginFailed(QString code);
    void mapsReady();
    void mapStateReady(QString mapId, QByteArray state, quint64 seq);
    void mapCreated(QString mapId);
    void inviteAccepted(QString mapId, QString role);
    void inviteSent();
    void invitationReady(const QString &code);
    void error(QString message);

private:
    void performLogin(const QString &username, const QString &password, bool automatic);
    void clearLocalSession();
    std::shared_ptr<ShareCredentialStore> m_credentialStore;
    QTimer m_loginTimer;
    bool m_loginInFlight = false;
    bool m_rememberedLogin = false;
    struct MapState {
        QByteArray state;
        quint64 seq = 0;
    };

    QNetworkRequest makeRequest(const QString &path) const;
    bool handleHttpError(QNetworkReply *reply, const QString &action);

    QString m_baseUrl;
    QNetworkAccessManager *m_network = nullptr;
    bool m_signedIn = false;
    QString m_accountName;
    QHash<QString, MapState> m_mapStates;
    QVariantList m_mapSummaries;
};
