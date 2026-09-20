#pragma once
#include "canvas.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QSettings>

// Machine-local presentation state, deliberately outside the document/undo model.
class ViewportState : public QObject {
public:
    ViewportState(MindCanvas *canvas, Engine *engine, QSettings *settings, QObject *parent = nullptr)
        : QObject(parent), m_canvas(canvas), m_engine(engine), m_settings(settings) {
        m_timer.setSingleShot(true);
        m_timer.setInterval(300);
        connect(&m_timer, &QTimer::timeout, this, [this] { flush(); });
        connect(engine, &Engine::documentOpening, this, [this] { flush(); m_ready = false; });
        connect(canvas, &MindCanvas::viewInitializing, this, [this] { flush(); m_ready = false; });
        connect(canvas, &MindCanvas::viewInitialized, this, [this] { restore(); });
        connect(canvas, &MindCanvas::viewChanged, this, [this] {
            if (m_ready && !m_key.isEmpty()) {
                capture();
                m_timer.start();
            }
        });
        connect(engine, &Engine::documentSaved, this, [this] {
            flush();
            m_key = keyFor(m_engine->documentPath());
            capture();
            flush();
        });
    }
    ~ViewportState() override { flush(); }
    static QString keyFor(const QString &path) {
        if (path.isEmpty()) return {};
        QFileInfo file(path);
        QString normalized = file.canonicalFilePath();
        if (normalized.isEmpty()) normalized = file.absoluteFilePath();
        return "viewports/v1/" + QString::fromLatin1(QCryptographicHash::hash(
            normalized.toUtf8(), QCryptographicHash::Sha256).toHex());
    }
    void flush() {
        m_timer.stop();
        if (!m_key.isEmpty() && !m_snapshot.isEmpty()) {
            m_settings->setValue(m_key, m_snapshot);
            m_settings->sync();
        }
    }
private:
    void capture() {
        m_snapshot = m_canvas->persistentView();
    }
    void restore() {
        // initializeView fits first, so capture is suspended until restoration.
        m_ready = false;
        m_key = keyFor(m_engine->documentPath());
        m_settings->sync();
        const auto saved = m_key.isEmpty() ? QVariantList{} : m_settings->value(m_key).toList();
        if (!m_key.isEmpty() && saved.size() == 3) {
            bool a, b, c;
            const double zoom = saved[0].toDouble(&a), x = saved[1].toDouble(&b), y = saved[2].toDouble(&c);
            if (a && b && c) m_canvas->restoreView(zoom, {x,y});
        }
        capture();
        m_ready = true;
    }
    MindCanvas *m_canvas;
    Engine *m_engine;
    QSettings *m_settings;
    QTimer m_timer;
    QString m_key;
    QVariantList m_snapshot;
    bool m_ready = false;
};
