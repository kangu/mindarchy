#include "sharesettings.h"

#include "../appidentity.h"

ShareSettings::ShareSettings(QObject *parent) : QObject(parent) {
    m_serverUrl = AppIdentity::windowSettings()->value("sharing/serverUrl", "share.mindarchy.xyz").toString();
}

void ShareSettings::setServerUrl(const QString &url) {
    if (m_serverUrl == url) return;
    m_serverUrl = url;
    AppIdentity::windowSettings()->setValue("sharing/serverUrl", url);
    emit serverUrlChanged();
}

void ShareSettings::setCustomUrl(const QString &url) { setServerUrl(url); }

QString shareServerOverride(int &argc, char *argv[]) {
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "--share-server") == 0) return QString::fromLocal8Bit(argv[i + 1]);
    return {};
}
