#include <QtTest>
#include "../src/collaboration/sharesettings.h"
#include "../src/appidentity.h"

class ShareSettingsTest : public QObject {
    Q_OBJECT
private slots:
    void defaultsAndPresets() {
        QSettings *settings = AppIdentity::windowSettings();
        settings->remove("sharing/serverUrl");
        settings->sync();
        ShareSettings share;
        QCOMPARE(share.presets(), (QStringList{"share.mindarchy.xyz", "http://localhost:8080"}));
        QCOMPARE(share.serverUrl(), "share.mindarchy.xyz");
    }
    void persistenceAndCustom() {
        ShareSettings share;
        share.setServerUrl("http://localhost:8080");
        QCOMPARE(AppIdentity::windowSettings()->value("sharing/serverUrl").toString(), "http://localhost:8080");
        QCOMPARE(ShareSettings().serverUrl(), "http://localhost:8080");
        share.setCustomUrl("http://192.168.1.10:8080");
        QCOMPARE(share.serverUrl(), "http://192.168.1.10:8080");
        QCOMPARE(AppIdentity::windowSettings()->value("sharing/serverUrl").toString(), "http://192.168.1.10:8080");
        share.setServerUrl("http://localhost:8080");
    }
};
QTEST_MAIN(ShareSettingsTest)
#include "sharesettings_test.moc"
