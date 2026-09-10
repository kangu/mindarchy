#pragma once
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QSettings>
#include <QVariantList>

class RecentDocuments {
public:
    explicit RecentDocuments(QString directory) : m_directory(std::move(directory)) {}
    QVariantList list() const {
        if(m_directory.isEmpty()) return {};
        QDir().mkpath(m_directory);
        QLockFile lock(m_directory+"/recent.lock"); if(!lock.tryLock(1000)) return {};
        QSettings settings(m_directory+"/recent.ini",QSettings::IniFormat);
        QVariantList result;
        for(const auto &path:settings.value("files").toStringList().mid(0,15)) {
            QFileInfo file(path);
            result.append(QVariantMap{{"path",path},{"name",file.completeBaseName()},
                {"available",file.isFile() && file.isReadable()}});
        }
        return result;
    }
    void record(const QString &path) const {
        if(m_directory.isEmpty() || path.isEmpty()) return;
        QFileInfo file(path); if(!file.isFile() || !file.isReadable()) return;
        const auto normalized=file.canonicalFilePath().isEmpty()?file.absoluteFilePath():file.canonicalFilePath();
        QDir().mkpath(m_directory);
        QLockFile lock(m_directory+"/recent.lock"); if(!lock.tryLock(1000)) return;
        QSettings settings(m_directory+"/recent.ini",QSettings::IniFormat);
        auto paths=settings.value("files").toStringList(); paths.removeAll(normalized); paths.prepend(normalized);
        settings.setValue("files",paths.mid(0,15)); settings.sync();
    }
    void clear() const {
        if(m_directory.isEmpty()) return;
        QDir().mkpath(m_directory);
        QLockFile lock(m_directory+"/recent.lock"); if(!lock.tryLock(1000)) return;
        QSettings settings(m_directory+"/recent.ini",QSettings::IniFormat); settings.remove("files"); settings.sync();
    }
private:
    QString m_directory;
};
