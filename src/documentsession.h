#pragma once
#include "appidentity.h"
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QVariantList>
#include <QUuid>
#include <memory>
#include <algorithm>

// Each document window runs in its own process. Instance locks distinguish live
// windows from stale entries after a crash; the registry lock serializes updates.
class DocumentSession {
public:
    static QString defaultDirectory() {
        return AppIdentity::sessionDirectory();
    }
    static QStringList restorePaths(const QString &directory = defaultDirectory()) {
        QDir().mkpath(directory);
        QLockFile guard(directory + "/registry.lock");
        if (!guard.tryLock(1000)) return {};
        QSettings settings(directory + "/documents.ini", QSettings::IniFormat);
        prune(settings, directory);
        settings.beginGroup("windows");
        const bool running = !settings.childKeys().isEmpty();
        settings.endGroup();
        // Launching another instance while working must not duplicate windows.
        if (running) return {};
        QStringList paths;
        for (const auto &path : settings.value("lastDocuments").toStringList())
            if (QFileInfo(path).isFile() && QFileInfo(path).isReadable()) paths.append(path);
        paths.removeDuplicates();
        return paths;
    }
    explicit DocumentSession(const QString &directory = defaultDirectory())
        : m_directory(directory), m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
        QDir().mkpath(directory);
        m_instance = std::make_unique<QLockFile>(directory + "/" + m_id + ".lock");
        m_instance->setStaleLockTime(0);
        m_locked = m_instance->tryLock();
        if (m_locked) update(QString(), false);
    }
    ~DocumentSession() { if (m_locked) update(QString(), true); }
    QVariantList liveWindows() {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return {};
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        prune(settings, m_directory);
        settings.beginGroup("windows"); const auto ids = settings.childKeys(); settings.endGroup();
        QVariantList windows;
        for (const auto &id : ids) {
            const QString path = settings.value("windows/" + id).toString();
            const qint64 pid = settings.value("processes/" + id).toLongLong();
            if (pid > 0) windows.append(QVariantMap{{"pid", pid}, {"title", path.isEmpty() ? "New mindmap" : QFileInfo(path).completeBaseName()}});
        }
        std::sort(windows.begin(), windows.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value("pid").toLongLong() < b.toMap().value("pid").toLongLong();
        });
        return windows;
    }
    void activateWindow(qint64 pid) {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return;
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        settings.setValue("activate/" + QString::number(pid), true); settings.sync();
    }
    bool takeActivation() {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return false;
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        const QString key = "activate/" + QString::number(QCoreApplication::applicationPid());
        const bool requested = settings.value(key).toBool();
        if (requested) { settings.remove(key); settings.sync(); }
        return requested;
    }
    enum class QuitAction { None, Confirm, Cancel, Close };
    void forgetDocument() {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return;
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        auto paths = settings.value("lastDocuments").toStringList();
        paths.removeAll(m_path);
        settings.setValue("lastDocuments", paths);
        settings.remove("windows/" + m_id);
        settings.sync();
    }
    void beginQuit() {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return;
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        prune(settings, m_directory);
        if (settings.value("quit/state") == "confirming") {
            for (const auto &id : settings.value("quit/participants").toStringList())
                if (settings.contains("windows/" + id)) return;
        }
        settings.remove("quit");
        settings.beginGroup("windows");
        auto ids = settings.childKeys();
        settings.endGroup();
        ids.removeAll(m_id); ids.prepend(m_id);
        settings.setValue("quit/participants", ids);
        settings.setValue("quit/state", "confirming");
        settings.setValue("quit/id", QUuid::createUuid().toString());
        settings.sync();
    }
    void voteToQuit(bool accepted) {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return;
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        if (settings.value("quit/id").toString() != m_quitId) return;
        if (accepted) settings.setValue("quit/votes/" + m_id, true);
        else settings.setValue("quit/state", "cancelled");
        settings.sync();
    }
    QuitAction pollQuit() {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return QuitAction::None;
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        prune(settings, m_directory);
        const QString id = settings.value("quit/id").toString();
        auto participants = settings.value("quit/participants").toStringList();
        QString state = settings.value("quit/state").toString();
        if (state == "confirming") {
            settings.beginGroup("windows");
            const auto active = settings.childKeys();
            settings.endGroup();
            for (const auto &key : active) if (!participants.contains(key)) participants.append(key);
            settings.setValue("quit/participants", participants);
        }
        if (id.isEmpty() || !participants.contains(m_id)) return QuitAction::None;
        if (state == "confirming") {
            QString next;
            for (const auto &participant : participants)
                if (settings.contains("windows/" + participant) &&
                    !settings.value("quit/votes/" + participant).toBool()) { next = participant; break; }
            if (next.isEmpty()) {
                QStringList paths;
                settings.beginGroup("windows");
                for (const auto &key : settings.childKeys()) {
                    const auto path = settings.value(key).toString();
                    if (!path.isEmpty()) paths.append(path);
                }
                settings.endGroup(); paths.removeDuplicates();
                settings.setValue("lastDocuments", paths);
                settings.setValue("quit/state", "closing"); state = "closing";
            } else if (next == m_id && m_quitId != id) {
                m_quitId = id; m_quitFinished = false;
                return QuitAction::Confirm;
            }
        }
        settings.sync();
        if (m_quitId == id && !m_quitFinished && state != "confirming") {
            m_quitFinished = true;
            return state == "closing" ? QuitAction::Close : QuitAction::Cancel;
        }
        return QuitAction::None;
    }
    void setDocument(const QString &path) {
        if (!m_locked || path.isEmpty()) return;
        const QString absolute = QFileInfo(path).absoluteFilePath();
        if (absolute == m_path) return;
        m_path = absolute;
        update(m_path, false);
    }
private:
    static void prune(QSettings &settings, const QString &directory) {
        settings.beginGroup("windows");
        for (const auto &id : settings.childKeys()) {
            QLockFile instance(directory + "/" + id + ".lock");
            instance.setStaleLockTime(0);
            if (instance.tryLock()) settings.remove(id);
        }
        settings.endGroup();
    }
    void update(const QString &path, bool closing) {
        QLockFile guard(m_directory + "/registry.lock");
        if (!guard.tryLock(1000)) return;
        QSettings settings(m_directory + "/documents.ini", QSettings::IniFormat);
        prune(settings, m_directory);
        settings.beginGroup("windows");
        if (closing) settings.remove(m_id);
        else settings.setValue(m_id, path);
        QStringList paths;
        for (const auto &id : settings.childKeys()) {
            const auto file = settings.value(id).toString();
            if (!file.isEmpty()) paths.append(file);
        }
        settings.endGroup();
        if (closing) settings.remove("processes/" + m_id);
        else settings.setValue("processes/" + m_id, QCoreApplication::applicationPid());
        // Preserve the last set as windows close, including the final window.
        // Empty new windows must not erase documents waiting to be restored.
        if (!closing && !path.isEmpty()) {
            paths.removeDuplicates();
            settings.setValue("lastDocuments", paths);
        }
        settings.sync();
    }
    QString m_directory, m_id, m_path, m_quitId;
    bool m_quitFinished = false;
    std::unique_ptr<QLockFile> m_instance;
    bool m_locked = false;
};
