#pragma once
#include <QObject>
#include <QRect>
#include <QVariantMap>
#include <QVector>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QWindow>
#include <memory>

struct PlacementScreen { QString id; QRect available; };
struct PlacementResult { QRect rect; int screen=0; bool restored=false; };
PlacementResult resolveWindowPlacement(const QVariantMap &saved, const QVector<PlacementScreen> &screens);

class WindowPlacement : public QObject {
public:
    explicit WindowPlacement(QWindow *window, const QString &settingsFile = {});
    void save();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QVector<PlacementScreen> screens() const;
    QByteArray hypr(const QStringList &args) const;
    void captureQt();
    void restoreQtState();
    void captureHypr(const QByteArray &data);
    void queryHypr();
    QWindow *m_window;
    std::unique_ptr<QSettings> m_settings;
    QVariantMap m_saved, m_current;
    QProcessEnvironment m_environment;
    QString m_hyprctl;
    QProcess m_query;
    QTimer m_poll, m_timeout;
    PlacementResult m_target;
    Qt::WindowState m_pendingQtState=Qt::WindowNoState;
    bool m_wayland=false, m_hyprland=false, m_lua=false, m_restorePending=false, m_settling=false;
};
