#pragma once
#include "canvas.h"
#include "engine.h"
#include <QFile>
#include <QFileInfo>
#include <QQuickWindow>
#include <QTimer>
#include <QVariantMap>

// A fixed cadence (not a restarting debounce) also checkpoints continuous typing.
class DocumentRecovery : public QObject {
public:
    DocumentRecovery(Engine *engine, QObject *window, MindCanvas *canvas,
                     const QString &path, const QVariantMap &restored = {})
        : m_engine(engine), m_window(window), m_canvas(canvas), m_path(path) {
        connect(&m_timer,&QTimer::timeout,this,[this] { if(m_ready) checkpoint(); });
        m_timer.start(1000);
        connect(engine,&Engine::documentSaved,this,[this] { m_lastRevision=~quint64(0); if(m_ready) checkpoint(); });
        connect(canvas,&MindCanvas::viewInitialized,this,[this,restored] {
            if(m_ready) return;
            // Let QML finish initializing the editor and viewport first.
            QTimer::singleShot(0,this,[this,restored] {
                QMetaObject::invokeMethod(m_window,"restoreRecoveryDraft",Q_ARG(QVariant,restored));
                const auto view=restored.value("view").toList();
                if(view.size()==3) m_canvas->restoreView(view[0].toDouble(),{view[1].toDouble(),view[2].toDouble()});
                m_ready=true;
                checkpoint();
            });
        });
    }
    bool checkpoint() {
        if(m_removed) return false;
        QVariant draft;
        if(!QMetaObject::invokeMethod(m_window,"recoveryDraft",Q_RETURN_ARG(QVariant,draft))) return false;
        auto state=draft.toMap();
        state.insert("view",m_canvas->persistentView());
        if(m_lastRevision==m_engine->recoveryRevision() && state==m_lastState && QFileInfo(m_path).isFile() && QFileInfo(m_path).isReadable()) return true;
        if(!m_engine->saveRecovery(m_path,state)) return false;
        m_lastRevision=m_engine->recoveryRevision(); m_lastState=state;
        return true;
    }
    bool prepareQuit() {
        if(!QMetaObject::invokeMethod(m_window,"prepareRecoveryQuit")) return false;
        if(checkpoint()) return true;
        QMetaObject::invokeMethod(m_window,"abortSessionQuit");
        return false;
    }
    void remove() { m_removed=true; m_timer.stop(); QFile::remove(m_path); }
private:
    Engine *m_engine;
    QObject *m_window;
    MindCanvas *m_canvas;
    QString m_path;
    QTimer m_timer;
    bool m_ready=false, m_removed=false;
    quint64 m_lastRevision=~quint64(0);
    QVariantMap m_lastState;
};
