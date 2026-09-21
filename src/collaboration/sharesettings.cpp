#include "sharesettings.h"

#include "../appidentity.h"

namespace {
QString g_commandLineOverride;
}

ShareSettings::ShareSettings(QObject *parent) : QObject(parent) {
    if (!g_commandLineOverride.isEmpty()) {
        m_serverUrl = g_commandLineOverride;
        return;
    }
    m_serverUrl = AppIdentity::windowSettings()->value("sharing/serverUrl", "share.mindarchy.xyz").toString();
}

void ShareSettings::setServerUrl(const QString &url) {
    if (m_serverUrl == url) return;
    m_serverUrl = url;
    AppIdentity::windowSettings()->setValue("sharing/serverUrl", url);
    emit serverUrlChanged();
}

void ShareSettings::setCustomUrl(const QString &url) { setServerUrl(url); }

void ShareSettings::applyCommandLineOverride(const QString &url) {
    g_commandLineOverride = url;
}

QString shareServerOverride(int &argc, char *argv[]) {
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "--share-server") == 0) return QString::fromLocal8Bit(argv[i + 1]);
    return {};
}

QString normalizeShareServerUrl(const QString &url) {
    const QString trimmed = url.trimmed();
    if (trimmed.contains(QStringLiteral("://"))) return trimmed;
    const QString host = trimmed.section('/', 0, 0).section(':', 0, 0);
    const bool local = host == QLatin1String("localhost") || host.startsWith(QLatin1String("127."))
                           || host.startsWith(QLatin1String("192."));
    return (local ? QStringLiteral("http://") : QStringLiteral("https://")) + trimmed;
}
