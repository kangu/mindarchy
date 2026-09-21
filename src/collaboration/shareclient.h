#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>

class ShareClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY signedInChanged)
public:
    explicit ShareClient(QObject *parent = nullptr);

    Q_INVOKABLE virtual void setBaseUrl(const QString &url);
    Q_INVOKABLE virtual void login(const QString &username, const QString &password);
    Q_INVOKABLE void account();
    Q_INVOKABLE virtual void maps();
    Q_INVOKABLE virtual void createMap(const QByteArray &snapshot);
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
    void loginFailed(QString code);
    void mapsReady();
    void mapStateReady(QString mapId, QByteArray state, quint64 seq);
    void mapCreated(QString mapId);
    void inviteAccepted(QString mapId, QString role);
    void inviteSent();
    void error(QString message);

private:
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
