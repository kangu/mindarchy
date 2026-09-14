#pragma once
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QLocalServer>
#include <QLockFile>
#include <QWindow>
#include <memory>
#include <vector>

class QTimer;
class Engine;
class QQuickWindow;
class MacDocumentWindow;

// One Cocoa application owns every document; recovery remains per document.
class MacApplication : public QObject {
public:
    explicit MacApplication(QString directory, QObject *parent = nullptr);
    ~MacApplication() override;
    enum class Launch { Primary, Forwarded, Failed };
    Launch startOrForward(const QStringList &files, bool fresh);
    void start(const QStringList &files, bool fresh, const QString &theme = {});
    QQuickWindow *open(const QString &path = {}, const QString &theme = {});
    bool requestQuit();
    QVariantList windows() const;
    void activate(qint64 id);
    void reopen();
    void tabAction(const QString &action, qint64 target = 0);
    void updateTabs();
    void saveTabs();
    Engine *activeDocument() const;
    QQuickWindow *activeWindow() const;
    QString error() const { return m_error; }
    bool quitting() const { return m_quitting; }
private:
    friend class MacDocumentWindow;
    void remove(MacDocumentWindow *document);
    void command(const QString &action, const QString &path);
    QString m_directory, m_error;
    QQmlEngine m_qml;
    QWindow m_menuOwner;
    QLocalServer m_server;
    std::unique_ptr<QLockFile> m_lock;
    std::vector<MacDocumentWindow *> m_documents;
    QPointer<QQuickWindow> m_active;
    qint64 m_nextId = 1;
    bool m_quitting = false;
    bool m_restoringTabs = false;
    QTimer *m_tabTimer = nullptr;
};
