#pragma once
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QVariant>

// Installed only for Windows. A bare Alt key is not a QShortcut sequence;
// intercept it before Qt Quick's menu navigation handles the event.
class WindowMenuBar : public QObject {
public:
    explicit WindowMenuBar(QQuickWindow *window) : QObject(window), m_window(window) {
        window->installEventFilter(this);
    }
protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::ShortcutOverride) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (!visible() && canvasAltShortcut(key)) {
                event->accept();
                return true;
            }
        }
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Alt && !(key->modifiers() & (Qt::ControlModifier | Qt::GroupSwitchModifier))) {
                if (key->isAutoRepeat()) return true;
                if (event->type() == QEvent::KeyPress) {
                    m_altDown = true; m_altUsed = false;
                    m_wasVisible = visible();
                } else if (m_altDown) {
                    // Bare Alt toggles the menu; Alt+letter is a canvas shortcut
                    // (fold/task) and must not open File first.
                    if (!m_altUsed) {
                        if (m_wasVisible) hide(true);
                        else QMetaObject::invokeMethod(m_window, "showWindowsMenu");
                    }
                    m_altDown = false;
                }
                return true;
            }
            if (event->type() == QEvent::KeyPress) {
                if (m_altDown) m_altUsed = true;
                if (key->key() == Qt::Key_Escape && visible()) { hide(true); return true; }
            }
        }
        if (event->type() == QEvent::MouseButtonPress && visible() && !popupOpen()) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->position().y() >= m_window->property("windowsMenuHeight").toDouble()) hide(false);
        }
        if (event->type() == QEvent::WindowDeactivate && visible() && !popupOpen()) hide(false);
        return false;
    }
private:
    static bool canvasAltShortcut(const QKeyEvent *key) {
        if (!(key->modifiers() & Qt::AltModifier) || (key->modifiers() & Qt::ControlModifier)) return false;
        return key->key() == Qt::Key_F || key->key() == Qt::Key_T;
    }
    bool visible() const { return m_window->property("windowsMenuVisible").toBool(); }
    bool popupOpen() const { return m_window->property("windowsMenuPopupOpen").toBool(); }
    void hide(bool restore) { QMetaObject::invokeMethod(m_window, "hideWindowsMenu", Q_ARG(QVariant, QVariant(restore))); }
    QQuickWindow *m_window;
    bool m_altDown = false, m_altUsed = false, m_wasVisible = false;
};
