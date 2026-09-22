#include "credentialstore.h"
#include <QCryptographicHash>
#ifdef Q_OS_MACOS
#include <Security/Security.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <wincred.h>
#else
#include <QProcess>
#include <QStandardPaths>
#endif

namespace {
QString keyFor(const QString &server) {
    return QString::fromLatin1(QCryptographicHash::hash(server.toUtf8(), QCryptographicHash::Sha256).toHex());
}
class DeviceVault final : public ShareCredentialStore {
public:
#ifdef Q_OS_MACOS
    CFMutableDictionaryRef query(const QString &server) {
        auto query = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        const auto bytes = keyFor(server).toUtf8();
        auto account = CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8 *>(bytes.constData()), bytes.size(), kCFStringEncodingUTF8, false);
        CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(query, kSecAttrService, CFSTR("org.mindarchy.sharing"));
        CFDictionarySetValue(query, kSecAttrAccount, account);
        CFRelease(account);
        return query;
    }
    QByteArray read(const QString &server, QString *error) override {
        auto q = query(server);
        CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue);
        CFTypeRef result = nullptr;
        const auto status = SecItemCopyMatching(q, &result);
        CFRelease(q);
        if (status == errSecItemNotFound) return {};
        if (status != errSecSuccess) { *error = "Could not unlock the device Keychain. Sign in or unlock your Keychain to reconnect."; return {}; }
        const auto data = static_cast<CFDataRef>(result);
        QByteArray value(reinterpret_cast<const char *>(CFDataGetBytePtr(data)), CFDataGetLength(data));
        CFRelease(result);
        return value;
    }
    bool write(const QString &server, const QByteArray &secret, QString *error) override {
        auto q = query(server);
        auto data = CFDataCreate(nullptr, reinterpret_cast<const UInt8 *>(secret.constData()), secret.size());
        auto attrs = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(attrs, kSecValueData, data);
        auto status = SecItemUpdate(q, attrs);
        if (status == errSecItemNotFound) {
            CFDictionarySetValue(q, kSecValueData, data);
            status = SecItemAdd(q, nullptr);
        }
        CFRelease(attrs); CFRelease(data); CFRelease(q);
        if (status != errSecSuccess) *error = "Signed in, but the device Keychain could not remember this login.";
        return status == errSecSuccess;
    }
    bool remove(const QString &server, QString *error) override {
        auto q = query(server); const auto status = SecItemDelete(q); CFRelease(q);
        if (status != errSecSuccess && status != errSecItemNotFound) *error = "Could not remove the saved login from Keychain. Unlock it and try signing out again.";
        return status == errSecSuccess || status == errSecItemNotFound;
    }
#elif defined(Q_OS_WIN)
    QByteArray read(const QString &server, QString *error) override {
        const auto key = ("Mindarchy/sharing/" + keyFor(server)).toStdWString();
        PCREDENTIALW credential = nullptr;
        if (!CredReadW(key.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
            if (GetLastError() != ERROR_NOT_FOUND) *error = "Could not read the saved login from Windows Credential Manager.";
            return {};
        }
        QByteArray value(reinterpret_cast<const char *>(credential->CredentialBlob), credential->CredentialBlobSize);
        CredFree(credential); return value;
    }
    bool write(const QString &server, const QByteArray &secret, QString *error) override {
        auto key = ("Mindarchy/sharing/" + keyFor(server)).toStdWString();
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC; credential.TargetName = key.data();
        credential.CredentialBlobSize = secret.size();
        credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(secret.constData()));
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        if (CredWriteW(&credential, 0)) return true;
        *error = "Signed in, but Windows Credential Manager could not remember this login."; return false;
    }
    bool remove(const QString &server, QString *error) override {
        const auto key = ("Mindarchy/sharing/" + keyFor(server)).toStdWString();
        if (CredDeleteW(key.c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND) return true;
        *error = "Could not remove the saved login from Windows Credential Manager. Try signing out again."; return false;
    }
#else
    QByteArray run(const QStringList &args, const QByteArray &input, QString *error, bool allowMissing = false) {
        const QString tool = QStandardPaths::findExecutable("secret-tool");
        if (tool.isEmpty()) { *error = "Remembering sign-in needs libsecret and an unlocked Secret Service keyring."; return {}; }
        QProcess process;
        process.start(tool, args);
        if (!process.waitForStarted(3000)) { *error = "Could not open the device keyring."; return {}; }
        if (!input.isEmpty()) process.write(input);
        process.closeWriteChannel();
        if (!process.waitForFinished(10000)) { process.kill(); process.waitForFinished(); *error = "Unlock your device keyring and try again."; return {}; }
        if (process.exitStatus() != QProcess::NormalExit || (process.exitCode() != 0 && !(allowMissing && process.exitCode() == 1 && process.readAllStandardError().isEmpty())))
            *error = "Could not access the device keyring. Check that your Secret Service keyring is unlocked.";
        return process.readAllStandardOutput().trimmed();
    }
    QByteArray read(const QString &server, QString *error) override {
        return run({"lookup", "application", "org.mindarchy.sharing", "server", keyFor(server)}, {}, error, true);
    }
    bool write(const QString &server, const QByteArray &secret, QString *error) override {
        run({"store", "--label=Mindarchy sharing login", "application", "org.mindarchy.sharing", "server", keyFor(server)}, secret, error);
        return error->isEmpty();
    }
    bool remove(const QString &server, QString *error) override {
        run({"clear", "application", "org.mindarchy.sharing", "server", keyFor(server)}, {}, error, true);
        return error->isEmpty();
    }
#endif
};
}
std::shared_ptr<ShareCredentialStore> systemShareCredentialStore() {
    static auto store = std::make_shared<DeviceVault>();
    return store;
}
