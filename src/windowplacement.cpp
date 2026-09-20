#include "windowplacement.h"
#include "appidentity.h"
#include <QGuiApplication>
#include <QQmlProperty>
#include <QScreen>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

PlacementResult resolveWindowPlacement(const QVariantMap &saved, const QVector<PlacementScreen> &screens) {
    if(screens.isEmpty()) return {{80,80,1000,700},0,false};
    for(int i=0;i<screens.size();++i) {
        if(screens[i].id!=saved.value("screen").toString()) continue;
        const QRect rect=saved.value("rect").toRect();
        if(rect.width()>=std::min(600,screens[i].available.width()) &&
           rect.height()>=std::min(640,screens[i].available.height()) &&
           rect.width()<=100000 && rect.height()<=100000 &&
           std::abs(qint64(rect.x()))<1000000 && std::abs(qint64(rect.y()))<1000000 &&
           screens[i].available.contains(rect)) return {rect,i,true};
    }
    const auto area=screens.first().available;
    const QSize size(std::max(1,std::min(1380,area.width()-32)),std::max(1,std::min(900,area.height()-32)));
    return {QRect(area.topLeft()+QPoint((area.width()-size.width())/2,(area.height()-size.height())/2),size),0,false};
}

QByteArray WindowPlacement::hypr(const QStringList &args) const {
    if(m_hyprctl.isEmpty()) return {};
    QProcess process; process.setProcessEnvironment(m_environment); process.start(m_hyprctl,args);
    if(!process.waitForFinished(200)) { process.kill(); process.waitForFinished(50); return {}; }
    return process.exitCode()==0 ? process.readAllStandardOutput() : QByteArray();
}
QVector<PlacementScreen> WindowPlacement::screens() const {
    QVector<PlacementScreen> result;
    if(m_hyprland) {
        const auto monitors=QJsonDocument::fromJson(hypr({"-j","monitors"})).array();
        for(const auto &value:monitors) {
            const auto m=value.toObject(); if(m["disabled"].toBool()) continue;
            const double scale=m["scale"].toDouble(1); if(scale<=0) continue;
            int width=m["width"].toInt(),height=m["height"].toInt();
            if(m["transform"].toInt()%2) std::swap(width,height);
            const auto reserved=m["reserved"].toArray();
            QRect area(m["x"].toInt(),m["y"].toInt(),qRound(width/scale),qRound(height/scale));
            if(reserved.size()==4) area.adjust(reserved[0].toInt(),reserved[1].toInt(),-reserved[2].toInt(),-reserved[3].toInt());
            if(!area.isValid()) continue;
            PlacementScreen screen{m["name"].toString(),area};
            if(m["focused"].toBool()) result.prepend(screen); else result.append(screen);
        }
        if(!result.isEmpty()) return result;
    }
    auto list=QGuiApplication::screens(); const auto primary=QGuiApplication::primaryScreen();
    if(list.removeOne(primary)) list.prepend(primary);
    for(auto *screen:list) result.append({screen->name()+"|"+screen->serialNumber(),screen->availableGeometry()});
    return result;
}
WindowPlacement::WindowPlacement(QWindow *window, const QString &settingsFile)
    : QObject(window),m_window(window),
      m_settings(settingsFile.isEmpty() ? AppIdentity::windowSettings() : new QSettings(settingsFile,QSettings::IniFormat)),
      m_environment(QProcessEnvironment::systemEnvironment()) {
    m_wayland=QGuiApplication::platformName().startsWith("wayland");
    if(m_wayland) {
        m_hyprctl=QStandardPaths::findExecutable("hyprctl");
        if(!m_environment.contains("HYPRLAND_INSTANCE_SIGNATURE")) {
            const auto instances=QJsonDocument::fromJson(hypr({"instances","-j"})).array();
            for(const auto &value:instances) {
                const auto instance=value.toObject();
                if(instance["wl_socket"].toString()==m_environment.value("WAYLAND_DISPLAY","wayland-0")) {
                    m_environment.insert("HYPRLAND_INSTANCE_SIGNATURE",instance["instance"].toString()); break;
                }
            }
        }
        m_hyprland=!m_hyprctl.isEmpty() && !QJsonDocument::fromJson(hypr({"-j","monitors"})).array().isEmpty();
    }
    if(m_hyprland) m_lua=hypr({"repl","type(hl.dsp.window.float)"}).trimmed()=="function";
    m_saved=m_settings->value("windowPlacement/v1").toMap();
    if(m_saved.value("backend").toString()!=(m_hyprland ? "hyprland" : m_wayland ? "wayland" : "qt")) m_saved.clear();
    const auto available=screens(); m_target=resolveWindowPlacement(m_saved,available);
    window->setMinimumSize({std::min(600,m_target.rect.width()),std::min(640,m_target.rect.height())});
    // An already-visible host belongs to a tab group; do not re-apply a
    // child document's saved rectangle onto that shared window.
    if(!window->isVisible()) {
        window->resize(m_target.rect.size());
        if(!m_wayland) window->setPosition(m_target.rect.topLeft());
    }
    for (const char *property : {"outlineVisible", "inspectorVisible"}) {
        const QString key=QStringLiteral("panels/v1/")+QString::fromLatin1(property);
        if (m_settings->contains(key) && window->property(property).isValid())
            // QQmlProperty also removes the initial width-based binding.
            QQmlProperty::write(window,QString::fromLatin1(property),m_settings->value(key).toBool());
    }
    m_current={{"rect",m_target.rect},{"screen",available.isEmpty() ? QString() : available[m_target.screen].id},
               {"backend",m_hyprland ? "hyprland" : m_wayland ? "wayland" : "qt"}, {"state","normal"}};
    if(!m_hyprland && m_target.restored) {
        const auto state=m_saved.value("state").toString();
        if(state=="maximized") m_pendingQtState=Qt::WindowMaximized;
        else if(state=="fullscreen") m_pendingQtState=Qt::WindowFullScreen;
        m_restorePending=m_pendingQtState!=Qt::WindowNoState;
    }
    if(QGuiApplication::platformName()=="cocoa" && m_pendingQtState==Qt::WindowMaximized) {
        // Prepare the native zoom and its restore-down rectangle while hidden.
        // The caller must use setVisible(true): QWindow::show() calls
        // showNormal(), which would undo this prepared state.
#ifdef Q_OS_MACOS
        void prepareMacMaximizedWindow(QWindow *window);
        prepareMacMaximizedWindow(window);
#endif
        m_current["state"]="maximized";
        m_pendingQtState=Qt::WindowNoState;
        m_restorePending=false;
    }
    window->installEventFilter(this);
    connect(qGuiApp,&QCoreApplication::aboutToQuit,this,&WindowPlacement::save);
    if(m_hyprland) {
        m_restorePending=!m_saved.isEmpty();
        m_query.setProcessEnvironment(m_environment);
        connect(&m_query,&QProcess::finished,this,[this] {
            m_timeout.stop();
            if(m_query.exitCode()==0) captureHypr(m_query.readAllStandardOutput());
        });
        m_timeout.setSingleShot(true); m_timeout.setInterval(200);
        connect(&m_timeout,&QTimer::timeout,&m_query,&QProcess::kill);
        m_poll.setInterval(750); connect(&m_poll,&QTimer::timeout,this,&WindowPlacement::queryHypr); m_poll.start();
        QTimer::singleShot(100,this,&WindowPlacement::queryHypr);
    } else {
        auto capture=[this] { QTimer::singleShot(0,this,&WindowPlacement::captureQt); };
        connect(window,&QWindow::xChanged,this,capture); connect(window,&QWindow::yChanged,this,capture);
        connect(window,&QWindow::widthChanged,this,capture); connect(window,&QWindow::heightChanged,this,capture);
        connect(window,&QWindow::windowStateChanged,this,capture);
    }
}
WindowPlacement::~WindowPlacement() {
    if(!m_address.isEmpty()) s_claimed.remove(m_address);
}
void WindowPlacement::restoreQtState() {
    if(m_hyprland || !m_restorePending || !m_window->isVisible()) return;
    // Apply deferred states after the initial show (including native
    // fullscreen). macOS maximization is prepared separately while hidden.
    // Retain the validated normal rectangle for restore-down.
    if(m_pendingQtState==Qt::WindowMaximized) m_window->showMaximized();
    else if(m_pendingQtState==Qt::WindowFullScreen) m_window->showFullScreen();
    m_restorePending=false;
    m_pendingQtState=Qt::WindowNoState;
    captureQt();
}
#ifdef Q_OS_MACOS
bool macWindowIsZoomed(QWindow *window);
#endif
void WindowPlacement::captureQt() {
    if(m_restorePending) return;
    const auto state=m_window->windowState();
    if(state==Qt::WindowMinimized || !m_window->isVisible()) return;
#ifdef Q_OS_MACOS
    const bool zoomed=state==Qt::WindowMaximized ||
        (QGuiApplication::platformName()=="cocoa" && macWindowIsZoomed(m_window));
#else
    const bool zoomed=state==Qt::WindowMaximized;
#endif
    m_current["state"]=zoomed ? "maximized" : state==Qt::WindowFullScreen ? "fullscreen" : "normal";
    if(zoomed || state!=Qt::WindowNoState) return;
    const auto rect=m_window->geometry();
    for(const auto &screen:screens()) if(screen.available.intersects(rect)) {
        m_current["rect"]=rect; m_current["screen"]=screen.id;
        if(screen.available.contains(rect)) break;
    }
}
void WindowPlacement::queryHypr() {
    if(m_query.state()!=QProcess::NotRunning || m_settling) return;
    m_query.start(m_hyprctl,{"-j","clients"}); m_timeout.start();
}
void WindowPlacement::captureHypr(const QByteArray &data) {
    if(m_settling) return;
    for(const auto &value:QJsonDocument::fromJson(data).array()) {
        const auto client=value.toObject();
        if(client["pid"].toInteger()!=QCoreApplication::applicationPid() || !client["mapped"].toBool()) continue;
        const QString address=client["address"].toString();
        if(!QRegularExpression("^0x[0-9a-fA-F]+$").match(address).hasMatch()) return;
        // Claim this instance's client once, at map time, then track it by its
        // stable address: titles change with the document, and sibling windows
        // can share one, so title matching alone would freeze or cross wires.
        if(m_address.isEmpty()) {
            if(s_claimed.contains(address) || client["initialTitle"].toString()!=m_window->title()) continue;
            m_address=address; s_claimed.insert(address);
        } else if(address!=m_address) continue;
        if(m_restorePending) {
            m_restorePending=false; m_settling=true;
            // Only floating windows have an application-restorable pixel rectangle.
            // Tiled windows remain part of the user's current layout.
            if(m_saved.value("floating").toBool()) {
                m_target=resolveWindowPlacement(m_saved,screens());
                const auto r=m_target.rect; const QString selector="address:"+address;
                if(m_lua) {
                    hypr({"eval",QString("hl.dispatch(hl.dsp.window.float({window='%1',action='enable'})); "
                        "hl.dispatch(hl.dsp.window.resize({window='%1',x=%2,y=%3,relative=false})); "
                        "hl.dispatch(hl.dsp.window.move({window='%1',x=%4,y=%5,relative=false}))")
                        .arg(selector).arg(r.width()).arg(r.height()).arg(r.x()).arg(r.y())});
                } else {
                    hypr({"--batch",QString("dispatch setfloating %1; dispatch resizewindowpixel exact %2 %3,%1; dispatch movewindowpixel exact %4 %5,%1")
                        .arg(selector).arg(r.width()).arg(r.height()).arg(r.x()).arg(r.y())});
                }
            }
            QTimer::singleShot(300,this,[this] { m_settling=false; queryHypr(); }); return;
        }
        const auto at=client["at"].toArray(),size=client["size"].toArray();
        if(at.size()!=2 || size.size()!=2) return;
        const QRect rect(at[0].toInt(),at[1].toInt(),size[0].toInt(),size[1].toInt());
        if(!rect.isValid()) return;
        m_current["floating"]=client["floating"].toBool();
        if(client["fullscreen"].toInt()!=0) return; // retain the normal rectangle
        m_current["rect"]=rect;
        for(const auto &screen:screens()) if(screen.available.contains(rect.center())) {m_current["screen"]=screen.id; break;}
        return;
    }
}
void WindowPlacement::save() {
    for (const char *property : {"outlineVisible", "inspectorVisible"}) {
        const auto value=m_window->property(property);
        if (value.isValid())
            m_settings->setValue(QStringLiteral("panels/v1/")+QString::fromLatin1(property),value.toBool());
    }
    m_settings->sync();
    if(m_restorePending || m_settling) return;
    if(m_hyprland) captureHypr(hypr({"-j","clients"})); else captureQt();
    if(m_current.value("rect").toRect().isValid()) {
        m_settings->setValue("windowPlacement/v1",m_current); m_settings->sync();
    }
}
bool WindowPlacement::eventFilter(QObject *object, QEvent *event) {
    if(object==m_window) {
        if(event->type()==QEvent::Show && !m_hyprland && m_restorePending)
            QTimer::singleShot(0,this,&WindowPlacement::restoreQtState);
        else if(event->type()==QEvent::Close) save();
    }
    return QObject::eventFilter(object,event);
}
