#pragma once

#include <QByteArray>
#include <QString>
#include <memory>

// Only the device vault stores remembered logins. Never fall back to settings/files.
class ShareCredentialStore {
public:
    virtual ~ShareCredentialStore() = default;
    virtual QByteArray read(const QString &server, QString *error) = 0;
    virtual bool write(const QString &server, const QByteArray &secret, QString *error) = 0;
    virtual bool remove(const QString &server, QString *error) = 0;
};
std::shared_ptr<ShareCredentialStore> systemShareCredentialStore();
