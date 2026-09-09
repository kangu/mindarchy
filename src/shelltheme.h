#pragma once
#include <QObject>
#include <QVariantMap>
#include <QFileSystemWatcher>
#include <QTimer>

class ShellTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
public:
    explicit ShellTheme(QString directory = {}, QObject *parent = nullptr);
    QVariantMap colors() const { return m_colors; }
signals:
    void changed();
private:
    void reload();
    QString m_directory;
    QVariantMap m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce, m_poll;
};
