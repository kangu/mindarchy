#pragma once
#include "engine.h"
#include "drawing.h"
#include <QElapsedTimer>
#include <QImage>
#include <QQuickItem>
#include <QSet>
#include <QTimer>
#include <QVariantAnimation>
#include <atomic>

class MindCanvas : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(QRectF imageSelectionRect READ imageSelectionRect NOTIFY viewChanged)
    Q_PROPERTY(QRectF imageDropRect READ imageDropRect NOTIFY viewChanged)
    Q_PROPERTY(Engine *engine READ engine WRITE setEngine NOTIFY engineChanged)
    Q_PROPERTY(bool focusActive READ focusActive NOTIFY focusChanged)
    Q_PROPERTY(QVariantList focusBreadcrumb READ focusBreadcrumb NOTIFY focusChanged)
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(QRectF searchResultRect READ searchResultRect NOTIFY viewChanged)
    Q_PROPERTY(double panX READ panX NOTIFY viewChanged)
    Q_PROPERTY(double panY READ panY NOTIFY viewChanged)
    Q_PROPERTY(int visibleRendered READ visibleRendered NOTIFY metricsChanged)
    Q_PROPERTY(double frameMs READ frameMs NOTIFY metricsChanged)
    Q_PROPERTY(int hoveredId READ hoveredId NOTIFY interactionChanged)
    Q_PROPERTY(bool dragging READ dragging NOTIFY interactionChanged)
    Q_PROPERTY(bool editing READ editing WRITE setEditing NOTIFY editingChanged)
    Q_PROPERTY(int editingId READ editingId NOTIFY editingChanged)
    Q_PROPERTY(QRectF editingRect READ editingRect NOTIFY viewChanged)
    Q_PROPERTY(QString dateHoverText READ dateHoverText NOTIFY interactionChanged)
    Q_PROPERTY(QPointF dateHoverPosition READ dateHoverPosition NOTIFY interactionChanged)
    Q_PROPERTY(QString interactionHint READ interactionHint NOTIFY interactionChanged)
  public:
    QRectF imageSelectionRect() const;
    QRectF imageDropRect() const;
    explicit MindCanvas(QQuickItem *parent = nullptr);
    Engine *engine() const { return m_engine; }
    void setEngine(Engine *);
    bool focusActive() const { return m_focusRoot >= 0; }
    QVariantList focusBreadcrumb() const;
    QVariantList persistentView() const {
        const auto zoom=(focusActive() || m_focusReturning) ? m_beforeFocusZoom : m_zoom;
        const auto pan=(focusActive() || m_focusReturning) ? m_beforeFocusPan : m_pan;
        const auto center=(QPointF(width()/2,height()/2)-pan)/zoom;
        return {zoom,center.x(),center.y()};
    }
    Q_INVOKABLE void focusBranch(int id = -1);
    Q_INVOKABLE void exitFocus();
    bool focusIncludes(int id) const { return !focusActive() || m_focusIds.contains(id); }
    double zoom() const { return m_zoom; }
    double panX() const { return m_pan.x(); }
    double panY() const { return m_pan.y(); }
    int visibleRendered() const { return m_draw.size(); }
    double frameMs() const { return m_sceneMs.load(); }
    int hoveredId() const { return m_hovered; }
    bool dragging() const { return m_dragging; }
    bool editing() const { return m_editingId >= 0; }
    int editingId() const { return m_editingId; }
    QRectF editingRect() const;
    QRectF nodeRect(int id) const { return displayRect(id); }
    QString dateHoverText() const { return m_dateHoverText; }
    QPointF dateHoverPosition() const { return m_dateHoverPosition; }
    Q_INVOKABLE void focusSearchResult(int id, QString query = {});
    Q_INVOKABLE void clearSearchHighlight();
    QRectF searchResultRect() const;
    Q_INVOKABLE void revealNode(int id) { ensureVisible(id); }
    Q_INVOKABLE void editDateEntry(int id, QString date);
    QString interactionHint() const { return m_hint; }
    void setEditing(bool value);
    QPointF mapToWorld(QPointF p) const { return (p - m_pan) / m_zoom; }
    QPointF mapFromWorld(QPointF p) const { return p * m_zoom + m_pan; }
    void zoomAt(QPointF p, double factor);
    Q_INVOKABLE void panBy(double dx, double dy);
    Q_INVOKABLE void normalizeEditorSize(QObject *editor);
    Q_INVOKABLE void formatText(QObject *editor, QString command);
    Q_INVOKABLE QVariantMap appearanceForNode(int id) const;
    Q_INVOKABLE void fit();
    Q_INVOKABLE void initializeView() { emit viewInitializing(); fit(); emit viewInitialized(); }
    bool restoreView(double zoom, QPointF center);
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void editSelected();
    Q_INVOKABLE void beginEdit(int id);
    Q_INVOKABLE void endEdit();
    Q_INVOKABLE void updateEditingText(QString text);
    Q_INVOKABLE bool commitEditing(QString text);
    Q_INVOKABLE bool exportPng(QString path);
  signals:
    void imageMenuRequested(int id, double x, double y);
    void imagePreviewRequested(int id);
    void focusChanged();
    void searchResultFocused();
    void viewInitializing();
    void viewInitialized();
    void engineChanged();
    void viewChanged();
    void metricsChanged();
    void interactionChanged();
    void editingChanged();
    void editRequested(int id, QString text);
    void replaceEditingText(QString text);
    void exportFinished(QString path, bool success);
    void commitRequested();
    void dateEditRequested(int id, QString date, QString text);

  protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &, const QRectF &) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void mouseUngrabEvent() override;
    void hoverMoveEvent(QHoverEvent *) override;
    void hoverLeaveEvent(QHoverEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;

  private:
    friend class CanvasTest;
    struct DrawNode {
        int id;
        QRectF rect;
        QColor color;
        NodeAppearance appearance;
        bool selected, folded, task, checked, expandsLeft;
        bool dimmed = false;
        qreal taskOpacity = 0;
        qreal completion = -1;
    };
    struct Label {
        int id;
        QRectF rect;
        QImage image;
        quint64 key;
    };
    struct Edge {
        QPointF a, b;
        QColor color;
        bool angular;
        bool vertical;
        qreal width = 1.5;
        Qt::PenStyle stroke = Qt::SolidLine;
        QVector<MapDrawing::BranchStroke> artistic;
    };
    struct BranchCache {
        QPointF a,b;
        QString style;
        QColor tint,background;
        qreal width=0;
        int depth=0;
        bool vertical=false,detailed=false;
        QVector<MapDrawing::BranchStroke> geometry;
    };
    QHash<int,BranchCache> m_branchCache;
    struct CachedLabel {
        QString text;
        QColor textColor;
        QSizeF size;
        int bucket;
        bool task;
        QImage image;
        quint64 key;
    };
    double imageWidth(int id) const;
    double imageInset(int id) const;
    QRectF contentRect(int id, QRectF node) const;
    QRectF imageWorldRect(int id) const;
    int imageHandleHit(QPointF point) const;
    void cancelImageResize();
    int m_imageSelected=-1, m_imageDrop=-1, m_imageResizeHandle=-1;
    double m_imageResizeWidth=0;
    QRectF m_imageResizeStart;
    void refresh();
    void refreshView();
    void documentChanged();
    void ensureVisible(int id);
    int hit(QPointF screen, bool excludeDrag = false) const;
    int taskHit(QPointF screen) const;
    QRectF displayRect(int id) const;
    qreal taskProgress(int id) const;
    QColor nodeColor(int id) const;
    void updateDrop(QPointF screen);
    void updateMarqueePreview();
    QColor creationHandleColor() const;
    QRectF creationHandleRect() const;
    QPointF creationAnchor(int id, std::optional<QPointF> toward = {}) const;
    QPolygonF creationPreview() const;
    void cancelCreation();
    void updateFocusIds();
    void animateFocusView(double zoom, QPointF pan);
    void stopFocusAnimation();
    QVariantAnimation m_focusAnimation;
    bool m_focusReturning = false;
    int m_focusRoot = -1;
    QSet<int> m_focusIds;
    QPointF m_beforeFocusPan;
    double m_beforeFocusZoom = 1;
    QString m_searchQuery;
    int m_searchResult = -1;
    int m_creatingParent = -1;
    bool m_creationDragged = false;
    QPointF m_creationEnd;
    QColor m_canvasColor = QColor("#111920");
    Engine *m_engine = nullptr;
    double m_zoom = 1.;
    QPointF m_pan;
    QVector<DrawNode> m_draw;
    QVector<Label> m_labels;
    QVector<Edge> m_edges;
    QHash<int, CachedLabel> m_cache;
    quint64 m_nextTextureKey = 1;
    quint64 m_geometryRevision = 0;
    QRectF m_cullViewport;
    std::atomic<double> m_sceneMs{0};
    QTimer m_metricsTimer, m_animationTimer;
    QElapsedTimer m_animationClock;
    QElapsedTimer m_wheelClock;
    bool m_wheelPanning = false;
    QHash<int, QRectF> m_previous, m_target, m_manualPreview;
    QHash<int, qreal> m_previousTasks, m_targetTasks;
    bool m_animating = false;
    int m_hovered = -1, m_pressedId = -1, m_dropParent = -1, m_before = -1, m_editingId = -1;
    int m_hoveredTask = -1, m_pressedTask = -1;
    bool m_dragging = false, m_panning = false, m_marquee = false, m_space = false,
         m_extend = false;
    QPointF m_press, m_last, m_dragDelta;
    QSet<int> m_dragRoots;
    bool m_deferredSelection = false;
    QSet<int> m_dragIds;
    QSizeF m_editContentSize;
    QRectF m_marqueeRect, m_editPreview;
    QSet<int> m_marqueeHits;
    QLineF m_dropLine;
    QString m_dateHoverText;
    QPointF m_dateHoverPosition;
    QString m_hint = "Click to select · double-click to edit · Tab adds a child";
};
