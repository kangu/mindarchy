#include "canvas.h"
#include "drawing.h"
using namespace MapDrawing;
#include <QAbstractTextDocumentLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QQuickItemGrabResult>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QSGTransformNode>
#include <QSGVertexColorMaterial>
#include <QStyleHints>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {
struct Vertex {
    float x, y;
    unsigned char r, g, b, a;
};
void vertex(QVector<Vertex> &v, QPointF p, QColor c) {
    v.append({float(p.x()), float(p.y()), (uchar)c.red(), (uchar)c.green(), (uchar)c.blue(),
              (uchar)c.alpha()});
}
void triangle(QVector<Vertex> &v, QPointF a, QPointF b, QPointF c, QColor col) {
    vertex(v, a, col);
    vertex(v, b, col);
    vertex(v, c, col);
}
void line(QVector<Vertex> &v, QPointF a, QPointF b, qreal width, QColor c) {
    QPointF d = b - a;
    qreal len = std::hypot(d.x(), d.y());
    if (len < .0001)
        return;
    QPointF n(-d.y() / len * width / 2, d.x() / len * width / 2);
    triangle(v, a + n, a - n, b + n, c);
    triangle(v, b + n, a - n, b - n, c);
}
void strokePath(QVector<Vertex> &v, const QPolygonF &points, qreal width, QColor color, Qt::PenStyle style) {
    if(width<=0 || color.alpha()==0) return;
    qreal distance=0;
    const qreal unit=std::max(1.,width), on=style==Qt::DotLine ? unit : 4*unit, period=on+2*unit;
    qreal totalLength=0;
    for(int i=1;i<points.size();++i) totalLength+=QLineF(points[i-1],points[i]).length();
    // Bound dash tessellation for imported maps with extreme manual offsets.
    if(totalLength/period > 4096) style=Qt::SolidLine;
    auto appendStroke = [&](const QPolygonF &path) {
        const auto mesh=strokeTriangles(path,width);
        for (const auto &point : mesh) vertex(v,point,color);
    };
    if (style==Qt::SolidLine) { appendStroke(points); return; }
    QPolygonF dash;
    for(int i=1;i<points.size();++i) {
        const QPointF a=points[i-1], delta=points[i]-a; const qreal len=QLineF(a,points[i]).length();
        qreal t=0;
        while(t<len-.0001) {
            const qreal phase=std::fmod(distance+t,period);
            const qreal step=std::min(len-t,phase<on ? on-phase : period-phase);
            if(step<.0001) {t+=.0001;continue;}
            if(phase<on) {
                if(dash.isEmpty()) dash << a+delta*(t/len);
                dash << a+delta*((t+step)/len);
            } else if(!dash.isEmpty()) { appendStroke(dash); dash.clear(); }
            t+=step;
        }
        distance+=len;
    }
    if(!dash.isEmpty()) appendStroke(dash);
}
void box(QVector<Vertex> &v, QRectF r, QColor c, qreal radius = 0) {
    if (radius <= 0) {
        triangle(v, r.topLeft(), r.topRight(), r.bottomLeft(), c);
        triangle(v, r.topRight(), r.bottomRight(), r.bottomLeft(), c);
        return;
    }
    radius = std::min({radius, r.width() / 2, r.height() / 2});
    QVector<QPointF> pts;
    const QPointF centres[] = {
        r.topRight() + QPointF(-radius, radius), r.bottomRight() + QPointF(-radius, -radius),
        r.bottomLeft() + QPointF(radius, -radius), r.topLeft() + QPointF(radius, radius)};
    for (int corner = 0; corner < 4; corner++)
        for (int j = 0; j <= 5; j++) {
            qreal a = (-90 + corner * 90 + j * 18) * std::numbers::pi / 180.;
            pts << centres[corner] + QPointF(std::cos(a) * radius, std::sin(a) * radius);
        }
    for (int j = 0; j < pts.size(); j++)
        triangle(v, r.center(), pts[j], pts[(j + 1) % pts.size()], c);
}
bool hasShapedBorder(const NodeAppearance &style) {
    return style.borderWidth > 0 && style.border.alpha() > 0 &&
           style.shape != NodeShape::Underline && style.shape != NodeShape::Embedded;
}
void shapeOutline(QVector<Vertex> &vertices, QRectF rect, const NodeAppearance &style,
                  QColor color, qreal thickness, qreal detail = 1) {
    auto polygon = shapePolygon(rect, style.shape, style.radius, detail);
    if (polygon.size() > 1 && polygon.first() == polygon.last()) polygon.removeLast();
    if (polygon.size() < 3) return;
    qreal area = 0;
    for (int i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
        area += a.x() * b.y() - b.x() * a.y();
    }
    auto normal = [area](QPointF edge) {
        const qreal length = std::hypot(edge.x(), edge.y());
        return length > .00001 ? QPointF(edge.y(), -edge.x()) * ((area > 0 ? 1. : -1.) / length) : QPointF();
    };
    QVector<QPointF> inner, outer;
    for (int i = 0; i < polygon.size(); ++i) {
        const auto p = polygon[i];
        const auto a = normal(p - polygon[(i + polygon.size() - 1) % polygon.size()]);
        const auto b = normal(polygon[(i + 1) % polygon.size()] - p);
        const auto offset = (a + b) / std::max(.25, 1. + QPointF::dotProduct(a, b));
        inner << p + offset * (style.borderWidth / 2);
        outer << p + offset * (style.borderWidth / 2 + thickness);
    }
    // A ring outside the actual border leaves transparent node interiors intact.
    for (int i = 0; i < polygon.size(); ++i) {
        const int j = (i + 1) % polygon.size();
        triangle(vertices, inner[i], outer[i], outer[j], color);
        triangle(vertices, inner[i], outer[j], inner[j], color);
    }
}
void themedShape(QVector<Vertex> &vertices, QRectF rect, const NodeAppearance &style, qreal detail) {
    if(style.shape == NodeShape::Embedded) return;
    if (style.shape == NodeShape::Underline) {
        strokePath(vertices, QPolygonF{rect.bottomLeft(), rect.bottomRight()}, style.branchWidth,style.branch,style.branchStroke);
        return;
    }
    const QPolygonF polygon=shapePolygon(rect,style.shape,style.radius,detail);
    for(int i=0;i<polygon.size();++i) {
        const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];
        if(style.fill.alpha()) triangle(vertices,rect.center(),a,b,style.fill);

    }
    auto border=polygon; if(!border.isEmpty()) border << border.first();
    strokePath(vertices,border,style.borderWidth,style.border,style.borderStyle);
}
struct TextureEntry {
    QSGSimpleTextureNode *node = nullptr;
    quint64 key = 0;
};
struct Scene : QSGNode {
    quint64 revision = std::numeric_limits<quint64>::max();
    QSGTransformNode *world = new QSGTransformNode;
    QSGGeometryNode *shapes = new QSGGeometryNode;
    QHash<int, TextureEntry> texts;
    Scene() {
        appendChildNode(world);
        world->appendChildNode(shapes);
        auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        g->setDrawingMode(QSGGeometry::DrawTriangles);
        shapes->setGeometry(g);
        shapes->setFlag(OwnsGeometry);
        shapes->setMaterial(new QSGVertexColorMaterial);
        shapes->setFlag(OwnsMaterial);
    }
};
} // namespace
MindCanvas::MindCanvas(QQuickItem *p) : QQuickItem(p) {
    setFlag(ItemHasContents);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    setFocus(true);
    setClip(true);
    m_metricsTimer.setInterval(400);
    connect(&m_metricsTimer, &QTimer::timeout, this, [this] { emit metricsChanged(); });
    m_metricsTimer.start();
    m_animationTimer.setInterval(16);
    connect(&m_animationTimer, &QTimer::timeout, this, [this] {
        if (m_animationClock.elapsed() >= 180) {
            m_animating = false;
            m_animationTimer.stop();
            m_previous = m_target;
        }
        refresh();
    });
}
void MindCanvas::setEngine(Engine *e) {
    if (m_engine == e)
        return;
    if (m_engine)
        disconnect(m_engine, nullptr, this, nullptr);
    m_engine = e;
    if (e) {
        connect(e, &Engine::changed, this, &MindCanvas::documentChanged);
        connect(e, &Engine::editRequested, this, &MindCanvas::beginEdit);
        documentChanged();
    }
    emit engineChanged();
}
QRectF MindCanvas::displayRect(int id) const {
    if (id == m_editingId && !m_editPreview.isEmpty()) return m_editPreview;
    if(m_dragging && m_manualPreview.contains(id)) return m_manualPreview.value(id);
    QRectF r = m_target.value(id, m_engine ? m_engine->nodes().value(id).rect : QRectF());
    if (m_animating && m_previous.contains(id)) {
        double t = std::clamp(m_animationClock.elapsed() / 180., 0., 1.);
        t = 1 - std::pow(1 - t, 3);
        QRectF a = m_previous.value(id);
        r = QRectF(a.topLeft() + (r.topLeft() - a.topLeft()) * t,
                   a.size() + (r.size() - a.size()) * t);
    }
    if (m_dragging && m_dragIds.contains(id))
        r.translate(m_dragDelta);
    return r;
}
qreal MindCanvas::taskProgress(int id) const {
    const qreal target = m_targetTasks.value(id);
    if (!m_animating) return target;
    const qreal t = 1 - std::pow(1 - std::clamp(m_animationClock.elapsed()/180., 0., 1.), 3);
    return m_previousTasks.value(id, target) + (target-m_previousTasks.value(id, target))*t;
}
void MindCanvas::documentChanged() {
    if(!m_dateHoverText.isEmpty()) { m_dateHoverText.clear(); emit interactionChanged(); }
    if (!m_engine)
        return;
    QHash<int, QRectF> next;
    QHash<int, qreal> nextTasks;
    bool moved = false;
    for (int id : m_engine->visibleIds()) {
        QRectF r = m_engine->nodes().value(id).rect;
        next.insert(id, r);
        const qreal task = m_engine->nodes().value(id).task ? 1 : 0;
        nextTasks.insert(id, task);
        if (m_targetTasks.contains(id) && m_targetTasks.value(id) != task) moved = true;
        if (m_target.contains(id) && m_target.value(id) != r)
            moved = true;
    }
    if (moved && next.size() < 1500 && !m_dragging) {
        QHash<int, QRectF> current;
        for (int id : m_target.keys())
            current.insert(id, displayRect(id));
        QHash<int, qreal> currentTasks;
        for (int id : m_targetTasks.keys()) currentTasks.insert(id, taskProgress(id));
        m_previousTasks = currentTasks;
        m_previous = current;
        m_animating = true;
        m_animationClock.restart();
        m_animationTimer.start();
    } else if (moved || next.size() >= 1500) {
        m_animating = false;
        m_animationTimer.stop();
    }
    m_target = next;
    m_targetTasks = nextTasks;
    if (editing() && (!m_engine->nodes().contains(m_editingId) || !m_target.contains(m_editingId)))
        endEdit();
    refresh();
    emit editingChanged();
}
QColor MindCanvas::nodeColor(int id) const {
    return m_engine ? m_engine->appearance(id).branch : QColor("#3da995");
}
QVariantMap MindCanvas::appearanceForNode(int id) const {
    if(!m_engine) return {};
    const auto a=m_engine->appearance(id);
    return {{"fill",a.fill.alpha() ? a.fill : m_engine->canvasColor()},
            {"text",a.text},{"border",a.border}};
}
void MindCanvas::refresh() {
    ++m_geometryRevision;
    m_canvasColor = m_engine ? m_engine->canvasColor() : QColor("#111920");
    m_draw.clear();
    m_edges.clear();
    m_labels.clear();
    if (!m_engine || width() <= 0 || height() <= 0) {
        update();
        return;
    }
    QRectF viewport(mapToWorld({-100, -100}), mapToWorld({width() + 100, height() + 100}));
    m_cullViewport = viewport;
    const auto &nodes = m_engine->nodes();
    auto selected = m_engine->selectedIds();
    bool vertical = m_engine->layout() == "Vertical";
    bool compact = m_engine->layout() == "Compact";
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.;
    qsizetype labelBytes = 0;
    constexpr qsizetype LabelBudget = 64 * 1024 * 1024;
    int bucket = std::clamp(int(std::ceil(std::log2(std::max(1., m_zoom*dpr))*2)), 0, 12);
    double rasterScale = std::pow(2., bucket/2.);
    for (int id : m_engine->visibleIds()) {
        const auto &n = nodes[id];
        QRectF r = displayRect(id);
        const auto appearance = m_engine->appearance(id);
        QColor color = appearance.branch;
        if (n.parent != -1 && m_target.contains(n.parent)) {
            QRectF parent = displayRect(n.parent);
            QPointF a, b;
            if (vertical) {
                a = {parent.center().x(), parent.bottom()};
                b = {r.center().x(), r.top()};
            } else if (compact) {
                a = {parent.left() + 12, parent.bottom()};
                b = {r.left(), r.center().y()};
            } else {
                const bool left=m_engine->manual() && r.center().x()<parent.center().x();
                a = {left ? parent.left() : parent.right(), parent.center().y()};
                b = {left ? r.right() : r.left(), r.center().y()};
            }
            if (!vertical) {
                if (appearance.shape == NodeShape::Underline) b.setY(r.bottom());
                if (!compact && m_engine->appearance(n.parent).shape == NodeShape::Underline)
                    a.setY(parent.bottom());
            }
            if (QRectF(a, b).normalized().adjusted(-4, -4, 4, 4).intersects(viewport))
                m_edges.append({a, b, color, m_engine->branchStyle() == "Angular" || compact,
                                vertical || compact, appearance.branchWidth, appearance.branchStroke});
        }
        if (!r.intersects(viewport))
            continue;
        const qreal taskOpacity = taskProgress(id);
        m_draw.append({id, r, color, appearance, selected.contains(id), n.folded, taskOpacity > 0, n.checked,
            m_engine->manual() && m_engine->layout()=="Horizontal" && r.center().x()<displayRect(1).center().x(), taskOpacity});
        if (m_zoom < .28 || editingId() == id)
            continue;
        // Task conversion changes padding, not the text's wrapping width.
        // Rasterize at the destination size and slide the label with the box.
        QRectF labelRect = r;
        if (m_animating && m_previousTasks.value(id, m_targetTasks.value(id)) != m_targetTasks.value(id)) {
            labelRect.setSize(m_target.value(id).size());
            labelRect.translate(20 * (taskOpacity - (n.task ? 1 : 0)), 0);
        }
        const QString labelKey=n.kind=="date" ? Calendar::key(n.calendar)+appearance.branch.name(QColor::HexArgb) : n.text;
        auto it = m_cache.find(id);
        if (it == m_cache.end() || it->text != labelKey || it->size != labelRect.size() ||
            it->bucket != bucket || it->task != n.task || it->textColor != appearance.text) {
            // Bound per-label raster memory even for very tall rich-text nodes.
            const double safeScale =
                std::min({rasterScale, 4096. / labelRect.width(), 4096. / labelRect.height(),
                          std::sqrt(4. * 1024. * 1024. / (labelRect.width() * labelRect.height()))});
            if (!std::isfinite(safeScale) || safeScale <= 0)
                continue;
            QSize pixels(std::max(1, int(labelRect.width() * safeScale)),
                         std::max(1, int(labelRect.height() * safeScale)));
            if (labelBytes + qsizetype(pixels.width()) * pixels.height() * 4 > LabelBudget)
                continue;
            QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(safeScale);
            image.fill(Qt::transparent);
            if(n.kind=="date") {
                QPainter painter(&image); paintCalendar(painter,n.calendar,appearance);
            } else {
            QTextDocument doc;
            QFont font("sans-serif", 11);
            font.setPixelSize(15);
            doc.setDefaultFont(font);
            doc.setDocumentMargin(0);
            doc.setDefaultStyleSheet(QString("body,p {color:%1; margin:0;}").arg(appearance.text.name()));
            doc.setHtml(n.text);
            doc.setTextWidth(std::max(20., labelRect.width() - 30 - (n.task ? 20 : 0)));
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.translate(15 + (n.task ? 20 : 0),
                              std::max(8., (labelRect.height() - doc.size().height()) / 2));
            QAbstractTextDocumentLayout::PaintContext ctx;
            ctx.palette.setColor(QPalette::Text, appearance.text);
            doc.documentLayout()->draw(&painter, ctx);
            painter.end();
            }
            m_cache.insert(id, {labelKey, appearance.text, labelRect.size(), bucket, n.task, image, m_nextTextureKey++});
            it = m_cache.find(id);
        }
        if (labelBytes + it->image.sizeInBytes() > LabelBudget)
            continue;
        labelBytes += it->image.sizeInBytes();
        m_labels.append({id, labelRect, it->image, it->key});
    }
    for (const auto &link : m_engine->connections()) {
        if (!m_target.contains(link.first) || !m_target.contains(link.second))
            continue;
        QPointF a = displayRect(link.first).center(), b = displayRect(link.second).center();
        if (QRectF(a, b).normalized().adjusted(-4, -4, 4, 4).intersects(viewport))
            m_edges.append({a, b, QColor("#efb86f"), false, false});
    }
    // Bound inactive label memory; textures are culled separately on the render thread.
    qsizetype cachedBytes = 0;
    for (const auto &entry : m_cache)
        cachedBytes += entry.image.sizeInBytes();
    if (m_cache.size() > 1200 || cachedBytes > LabelBudget) {
        QSet<int> keep;
        for (const auto &l : m_labels)
            keep.insert(l.id);
        for (auto it = m_cache.begin(); it != m_cache.end();)
            if (!keep.contains(it.key()))
                it = m_cache.erase(it);
            else
                ++it;
    }
    emit viewChanged();
    update();
}
QSGNode *MindCanvas::updatePaintNode(QSGNode *old, UpdatePaintNodeData *) {
    QElapsedTimer timer;
    timer.start();
    if (window()->rendererInterface()->graphicsApi() == QSGRendererInterface::Software) {
        // Qt's software adaptation does not support custom vertex-color materials.
        const qreal dpr = window()->effectiveDevicePixelRatio();
        QImage image(QSize(qMax(1, int(width()*dpr)), qMax(1, int(height()*dpr))),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(m_canvasColor);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(m_pan);
        painter.scale(m_zoom, m_zoom);
        for (const auto &e : m_edges) {
            painter.setPen(QPen(e.color, e.width, e.stroke));
            if(e.width>0) painter.drawPolyline(edgePath(e.a,e.b,e.angular,e.vertical,m_zoom*window()->effectiveDevicePixelRatio()));
        }
        for (const auto &n : m_draw) {
            if ((n.selected || n.id == m_hovered) && hasShapedBorder(n.appearance)) {
                QVector<Vertex> outline;
                shapeOutline(outline, n.rect, n.appearance,
                             QColor(n.selected ? "#7065CE" : "#ACA4DC"), (n.selected ? 2. : 1.) / m_zoom, m_zoom*window()->effectiveDevicePixelRatio());
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(n.selected ? "#7065CE" : "#ACA4DC"));
                QPainterPath outlinePath;
                outlinePath.setFillRule(Qt::WindingFill);
                for (int i = 0; i + 2 < outline.size(); i += 3)
                    outlinePath.addPolygon(QPolygonF{QPointF(outline[i].x, outline[i].y),
                        QPointF(outline[i+1].x, outline[i+1].y), QPointF(outline[i+2].x, outline[i+2].y)});
                painter.drawPath(outlinePath);
            } else if(n.selected) {
                painter.setPen(QPen(QColor("#7065CE"),2));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(n.rect.adjusted(-4,-4,4,4),10,10);
            }
            painter.setPen(n.appearance.borderWidth>0 ? QPen(n.appearance.border,n.appearance.borderWidth,n.appearance.borderStyle) : QPen(Qt::NoPen));
            painter.setBrush(n.appearance.fill);
            if(n.appearance.shape==NodeShape::Underline) {
                painter.setPen(QPen(n.appearance.branch,n.appearance.branchWidth,n.appearance.branchStroke));
                if(n.appearance.branchWidth>0) painter.drawLine(n.rect.bottomLeft(),n.rect.bottomRight());
            } else if(n.appearance.shape!=NodeShape::Embedded) painter.drawPolygon(shapePolygon(n.rect,n.appearance.shape,n.appearance.radius,m_zoom*window()->effectiveDevicePixelRatio()));
            if(m_zoom > .28 && n.task) {
                const QRectF check(n.rect.left()+8,n.rect.center().y()-5,10,10);
                if (n.id == m_hoveredTask && !m_dragging && !m_panning) {
                    QColor tint = n.appearance.text;
                    tint.setAlphaF(.04);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(tint);
                    painter.drawRoundedRect(check.adjusted(-3,-3,3,3),3,3);
                }
                painter.save();
                painter.setOpacity(n.taskOpacity);
                paintTask(painter, check, n.appearance.text, n.checked);
                painter.restore();
            }
        }
        for (const auto &l : m_labels)
            painter.drawImage(l.rect, l.image);
        if (m_dragging && !m_dropLine.isNull()) {
            painter.setPen(QPen(QColor("#f3cc79"),2./m_zoom));
            painter.drawLine(m_dropLine);
        }
        painter.end();
        auto *node = static_cast<QSGSimpleTextureNode *>(old);
        if (!node) {
            node = new QSGSimpleTextureNode;
            node->setOwnsTexture(true);
        }
        node->setTexture(window()->createTextureFromImage(image));
        node->setRect(boundingRect());
        m_sceneMs.store(timer.nsecsElapsed() / 1000000.);
        return node;
    }
    auto *scene = static_cast<Scene *>(old);
    if (!scene)
        scene = new Scene;
    QMatrix4x4 transform;
    transform.translate(m_pan.x(), m_pan.y());
    transform.scale(m_zoom);
    scene->world->setMatrix(transform);
    if (scene->revision == m_geometryRevision) {
        m_sceneMs.store(timer.nsecsElapsed() / 1000000.);
        return scene;
    }
    QVector<Vertex> vertices;
    vertices.reserve(m_draw.size() * 12 + m_edges.size() * 24);
    box(vertices, m_cullViewport, m_canvasColor);
    for (const auto &e : m_edges) {
        strokePath(vertices,edgePath(e.a,e.b,e.angular,e.vertical,m_zoom*window()->effectiveDevicePixelRatio()),e.width,e.color,e.stroke);
    }
    for (const auto &n : m_draw) {
        auto style=n.appearance;
        if(m_zoom < .25 && style.shape != NodeShape::Underline && style.shape != NodeShape::Embedded) {
            style.shape=NodeShape::Rectangle;
            style.radius=0;
        }
        const bool shapedOutline = hasShapedBorder(style);
        if ((n.selected || n.id == m_hovered) && shapedOutline)
            shapeOutline(vertices, n.rect, style, QColor(n.selected ? "#7065CE" : "#ACA4DC"),
                         (n.selected ? 2. : 1.) / m_zoom, m_zoom*window()->effectiveDevicePixelRatio());
        else if (n.selected)
            box(vertices, n.rect.adjusted(-4,-4,4,4), QColor("#7065CE"), 10);
        else if (n.id == m_hovered)
            box(vertices, n.rect.adjusted(-2,-2,2,2), QColor("#ACA4DC"), 9);
        if(!shapedOutline && (n.selected || n.id == m_hovered))
            box(vertices,n.rect.adjusted(-1,-1,1,1),m_canvasColor,8);
        themedShape(vertices,n.rect,style,m_zoom*window()->effectiveDevicePixelRatio());
        if (m_zoom > .28 && n.task) {
            QRectF check(n.rect.left() + 8, n.rect.center().y() - 5, 10, 10);
            if (n.id == m_hoveredTask && !m_dragging && !m_panning) {
                QColor tint = n.appearance.text;
                tint.setAlphaF(.04);
                box(vertices, check.adjusted(-3,-3,3,3), tint, 3);
            }
            QColor frame = n.appearance.text;
            frame.setAlphaF(frame.alphaF() * n.taskOpacity);
            if (n.checked) frame.setAlphaF(frame.alphaF() * completedTaskFrameOpacity);
            box(vertices, check, frame, 2);
            box(vertices, check.adjusted(1.5,1.5,-1.5,-1.5),
                n.appearance.fill.alpha() ? n.appearance.fill : m_canvasColor,1);
            if (n.checked) {
                const auto tick = taskCheckPath(check);
                QColor tickColor = taskCheckColor; tickColor.setAlphaF(n.taskOpacity);
                for (int i = 1; i < tick.size(); ++i)
                    line(vertices, tick[i-1], tick[i], taskCheckWidth, tickColor);
                const qreal radius = taskCheckWidth / 2;
                for (const auto &point : tick)
                    box(vertices, QRectF(point-QPointF(radius,radius), QSizeF(taskCheckWidth,taskCheckWidth)), tickColor, radius);
            }
        }
        if (n.folded)
            box(vertices, QRectF(n.expandsLeft ? n.rect.left()-12 : n.rect.right()+4, n.rect.center().y() - 4, 8, 8), n.color, 4);
    }
    if (m_dragging && !m_dropLine.isNull()) {
        line(vertices,m_dropLine.p1(),m_dropLine.p2(),2./m_zoom,QColor("#f3cc79"));
    } else if (m_dragging && m_dropParent >= 0 && m_target.contains(m_dropParent)) {
        QRectF r = displayRect(m_dropParent).adjusted(-6, -6, 6, 6);
        QColor col("#f3cc79");
        line(vertices, r.topLeft(), r.topRight(), 2. / m_zoom, col);
        line(vertices, r.topRight(), r.bottomRight(), 2. / m_zoom, col);
        line(vertices, r.bottomRight(), r.bottomLeft(), 2. / m_zoom, col);
        line(vertices, r.bottomLeft(), r.topLeft(), 2. / m_zoom, col);
    }
    if (m_marquee) {
        QColor c("#92dcc7");
        QRectF r = m_marqueeRect;
        line(vertices, r.topLeft(), r.topRight(), 1. / m_zoom, c);
        line(vertices, r.topRight(), r.bottomRight(), 1. / m_zoom, c);
        line(vertices, r.bottomRight(), r.bottomLeft(), 1. / m_zoom, c);
        line(vertices, r.bottomLeft(), r.topLeft(), 1. / m_zoom, c);
    }
    auto *g = scene->shapes->geometry();
    g->allocate(vertices.size());
    auto *out = g->vertexDataAsColoredPoint2D();
    for (int i = 0; i < vertices.size(); i++) {
        const auto &v = vertices[i];
        out[i].set(v.x, v.y, v.r, v.g, v.b, v.a);
    }
    scene->shapes->markDirty(QSGNode::DirtyGeometry);
    QSet<int> alive;
    for (const auto &l : m_labels) {
        alive.insert(l.id);
        auto &entry = scene->texts[l.id];
        if (!entry.node) {
            entry.node = new QSGSimpleTextureNode;
            entry.node->setOwnsTexture(true);
            entry.node->setTexture(window()->createTextureFromImage(l.image));
            entry.key = l.key;
            entry.node->setFiltering(QSGTexture::Linear);
            entry.node->setRect(l.rect);
            scene->world->appendChildNode(entry.node);
        } else {
            if (entry.key != l.key) {
                entry.node->setTexture(window()->createTextureFromImage(l.image));
                entry.key = l.key;
            }
            entry.node->setRect(l.rect);
        }
    }
    for (auto it = scene->texts.begin(); it != scene->texts.end();)
        if (!alive.contains(it.key())) {
            scene->world->removeChildNode(it->node);
            delete it->node;
            it = scene->texts.erase(it);
        } else
            ++it;
    scene->revision = m_geometryRevision;
    m_sceneMs.store(timer.nsecsElapsed() / 1000000.);
    return scene;
}
void MindCanvas::geometryChange(const QRectF &a, const QRectF &b) {
    QQuickItem::geometryChange(a, b);
    m_pan += QPointF((a.width() - b.width()) / 2, (a.height() - b.height()) / 2);
    refresh();
}
void MindCanvas::zoomAt(QPointF p, double factor) {
    QPointF world = mapToWorld(p);
    m_zoom = std::clamp(m_zoom * factor, .00001, 4.);
    m_pan = p - world * m_zoom;
    refresh();
}
void MindCanvas::zoomIn() { zoomAt({width() / 2, height() / 2}, 1.25); }
void MindCanvas::zoomOut() { zoomAt({width() / 2, height() / 2}, .8); }
void MindCanvas::resetZoom() { zoomAt({width() / 2, height() / 2}, 1. / m_zoom); }
void MindCanvas::fit() {
    if (!m_engine || width() < 10 || height() < 10)
        return;
    m_animating = false;
    m_animationTimer.stop();
    QRectF b = m_engine->bounds();
    if (b.isEmpty())
        return;
    m_zoom = std::clamp(std::min((width() - 100) / b.width(), (height() - 180) / b.height()),
                        .00001, 1.4);
    m_pan = QPointF(width() / 2, (height() - 50) / 2) - b.center() * m_zoom;
    refresh();
}
void MindCanvas::ensureVisible(int id) {
    if (!m_target.contains(id))
        return;
    QRectF r = m_target.value(id);
    QRectF sr(mapFromWorld(r.topLeft()), r.size() * m_zoom);
    QPointF shift;
    if (sr.left() < 40)
        shift.setX(40 - sr.left());
    else if (sr.right() > width() - 40)
        shift.setX(width() - 40 - sr.right());
    // Leave room for the editing tools above and floating zoom controls below.
    const double topMargin = editing() ? 64 : 40;
    const double bottomMargin = editing() ? 96 : 40;
    if (sr.top() < topMargin)
        shift.setY(topMargin - sr.top());
    else if (sr.bottom() > height() - bottomMargin)
        shift.setY(height() - bottomMargin - sr.bottom());
    m_pan += shift;
    refresh();
}
QRectF MindCanvas::editingRect() const {
    if (!m_target.contains(m_editingId))
        return {};
    QRectF r = displayRect(m_editingId);
    return {mapFromWorld(r.topLeft()), r.size() * m_zoom};
}
void MindCanvas::setEditing(bool value) {
    if (value)
        editSelected();
    else
        endEdit();
}
void MindCanvas::beginEdit(int id) {
    if (editing()) {
        if (m_editingId == id)
            return;
        emit commitRequested();
        if (editing())
            return;
    }
    if (!m_engine || !m_engine->nodes().contains(id))
        return;
    if (m_engine->selectedId() != id)
        m_engine->select(id);
    if(m_engine->nodes().value(id).kind=="date") {
        ensureVisible(id);
        editDateEntry(id,m_engine->nodes().value(id).calendar.anchor.toString(Qt::ISODate));
        return;
    }
    // Settle the one creation layout before accepting keystrokes. Editing itself
    // neither animates the graph nor changes the user's chosen zoom level.
    m_animating = false;
    m_animationTimer.stop();
    m_previous = m_target;
    m_editingId = id;
    m_editPreview = m_target.value(id);
    ensureVisible(id);
    emit editingChanged();
    refresh();
    emit editRequested(id, m_engine->nodes().value(id).text);
}
void MindCanvas::editSelected() {
    if (m_engine)
        beginEdit(m_engine->selectedId());
}
void MindCanvas::updateEditingText(QString text) {
    if (!editing() || !m_engine) return;
    const QSizeF size = m_engine->previewTextSize(m_editingId, text);
    if (size.isEmpty() || size == m_editPreview.size()) return;
    m_editPreview.setSize(size);
    refresh();
}
bool MindCanvas::commitEditing(QString text) {
    if (!editing()) return true;
    if (!m_engine) return false;
    const int id = m_editingId;
    const QPointF anchor = mapFromWorld(m_editPreview.topLeft());
    if (!m_engine->setText(id, text)) return false;
    // Lay out once on commit, keeping the edited node's text origin in place.
    m_animating = false;
    m_animationTimer.stop();
    m_previous = m_target;
    m_pan = anchor - m_engine->nodes().value(id).rect.topLeft() * m_zoom;
    endEdit();
    return true;
}
void MindCanvas::endEdit() {
    m_editingId = -1;
    m_editPreview = {};
    emit editingChanged();
    refresh();
    forceActiveFocus();
}
bool MindCanvas::exportPng(QString path) {
    if (path.startsWith("file:"))
        path = QUrl(path).toLocalFile();
    auto result = grabToImage();
    if (!result)
        return false;
    connect(result.data(), &QQuickItemGrabResult::ready, this, [this, result, path] {
        bool ok = result->saveToFile(path);
        emit exportFinished(path, ok);
    });
    return true;
}
int MindCanvas::hit(QPointF p, bool excludeDrag) const {
    QPointF w = mapToWorld(p);
    for (auto it = m_draw.crbegin(); it != m_draw.crend(); ++it)
        if ((!excludeDrag || !m_dragIds.contains(it->id)) && it->rect.contains(w))
            return it->id;
    return -1;
}
int MindCanvas::taskHit(QPointF screen) const {
    if (!m_engine || m_zoom <= .28)
        return -1;
    const QPointF world = mapToWorld(screen);
    for (auto it = m_draw.crbegin(); it != m_draw.crend(); ++it) {
        if (it->task && m_engine->nodes().value(it->id).kind != "date") {
            const QPointF center = mapFromWorld(QPointF(it->rect.left() + 13, it->rect.center().y()));
            const qreal half = std::max(16., 8. * m_zoom);
            QRectF target(center - QPointF(half, half), QSizeF(half * 2, half * 2));
            // Preserve a gap before the text, especially when zoomed out.
            target.setRight(std::min(target.right(), mapFromWorld(it->rect.topLeft()).x() + 32 * m_zoom));
            if (target.contains(screen))
                return it->id;
        }
        // An overlapping node in front owns its own pointer events.
        if (it->rect.contains(world))
            return -1;
    }
    return -1;
}
void MindCanvas::mousePressEvent(QMouseEvent *e) {
    if (!m_engine)
        return;
    if (editing()) {
        emit commitRequested();
        if (editing()) {
            e->ignore();
            return;
        }
    }
    forceActiveFocus();
    m_press = m_last = e->position();
    m_pressedId = hit(m_press);
    m_extend = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier);
    m_panning = e->button() == Qt::MiddleButton || m_space || e->button() == Qt::RightButton;
    m_pressedTask = !m_panning && !m_extend && e->button() == Qt::LeftButton
                        ? taskHit(m_press) : -1;
    if (m_pressedTask >= 0)
        m_pressedId = m_pressedTask;
    m_marquee = false;
    m_dragging = false;
    m_manualPreview.clear();
    m_dragDelta = {};
    m_dropParent = -1;
    m_before = -1;
    if (!m_panning) {
        if (m_pressedId >= 0) {
            m_engine->select(m_pressedId, m_extend);
            m_dragIds = {m_pressedId};
            for (int id : m_engine->visibleIds())
                if (m_engine->isDescendant(id, m_pressedId))
                    m_dragIds.insert(id);
        } else {
            if (!m_extend)
                m_engine->select(-1);
        }
    }
    e->accept();
}
void MindCanvas::updateDrop(QPointF screen) {
    m_dropParent = -1;
    m_before = -1;
    m_dropLine = {};
    const QPointF world = mapToWorld(screen);
    const bool verticalLayout = m_engine->layout() == "Vertical";
    int target = hit(screen, true);
    if (target >= 0) {
        const auto &n = m_engine->nodes()[target];
        QRectF r = displayRect(target);
        QPointF p = mapToWorld(screen);
        bool vertical = m_engine->layout() == "Vertical";
        double fraction =
            vertical ? (p.x() - r.left()) / r.width() : (p.y() - r.top()) / r.height();
        if ((fraction < .25 || fraction > .75) && n.parent >= 0) {
            m_dropParent = n.parent;
            if (fraction < .25)
                m_before = target;
            else {
                auto siblings = m_engine->nodes()[n.parent].children;
                int index = siblings.indexOf(target);
                m_before = index + 1 < siblings.size() ? siblings[index + 1] : -1;
            }
        } else
            m_dropParent = target;
    }
    // Empty space in the sibling lane is an insertion target too. Node centers
    // retain their existing meaning (attach as a child).
    const int currentParent=m_engine->nodes().value(m_pressedId).parent;
    if (!m_engine->manual() && target < 0 && currentParent >= 0) {
        QVector<int> siblings;
        QRectF lane;
        for (int id : m_engine->nodes().value(currentParent).children) {
            if (id == m_pressedId) continue;
            siblings.append(id);
            lane=lane.united(displayRect(id));
        }
        const qreal margin=24./m_zoom;
        // Beyond either end, accept the entire open canvas rather than requiring
        // proximity to the insertion marker or alignment with the sibling column.
        const bool beyondEnds=verticalLayout
            ? world.x()<lane.left() || world.x()>lane.right()
            : world.y()<lane.top() || world.y()>lane.bottom();
        if (!siblings.isEmpty() && (beyondEnds || lane.adjusted(-margin,-margin,margin,margin).contains(world))) {
            m_dropParent=currentParent;
            for (int id : siblings) {
                const auto center=displayRect(id).center();
                if ((verticalLayout ? world.x()<center.x() : world.y()<center.y())) {
                    m_before=id; break;
                }
            }
        }
    }
    if (!m_engine->manual() && m_dropParent >= 0 &&
        (m_before >= 0 || (target >= 0 && m_engine->nodes().value(target).parent == m_dropParent) || target < 0)) {
        QVector<int> siblings;
        for (int id : m_engine->nodes().value(m_dropParent).children)
            if (id != m_pressedId) siblings.append(id);
        if (!siblings.isEmpty()) {
            // Skip the dragged node when an edge hit points at its old position.
            if (m_before == m_pressedId) {
                const auto all=m_engine->nodes().value(m_dropParent).children;
                const int next=all.indexOf(m_pressedId)+1;
                m_before=next<all.size() ? all[next] : -1;
            }
            const QRectF r=displayRect(m_before>=0 ? m_before : siblings.last());
            const qreal offset=6./m_zoom;
            if (verticalLayout) {
                const qreal x=m_before>=0 ? r.left()-offset : r.right()+offset;
                m_dropLine=QLineF(QPointF(x,r.top()),QPointF(x,r.bottom()));
            } else {
                const qreal y=m_before>=0 ? r.top()-offset : r.bottom()+offset;
                m_dropLine=QLineF(QPointF(r.left(),y),QPointF(r.right(),y));
            }
        }
    }
    m_hint = m_engine->manual()  ? "Release to place subtree"
             : !m_dropLine.isNull() ? "Release to reorder at insertion line"
             : m_dropParent >= 0 ? "Release to attach to highlighted parent"
                                 : "Drop on a node to attach · near its edge to reorder";
    emit interactionChanged();
}
void MindCanvas::mouseMoveEvent(QMouseEvent *e) {
    if (!m_engine)
        return;
    QPointF p = e->position();
    if (m_panning) {
        m_pan += p - m_last;
        refreshView();
    } else if (m_pressedId >= 0) {
        if (!m_dragging &&
            QLineF(m_press, p).length() >= QGuiApplication::styleHints()->startDragDistance()) {
            m_dragging = true;
            m_animating = false;
            m_animationTimer.stop();
            emit interactionChanged();
        }
        if (m_dragging) {
            m_dragDelta = (p - m_press) / m_zoom;
            if(m_engine->manual() && m_engine->layout()=="Horizontal")
                m_manualPreview=m_engine->manualGeometry(m_pressedId,m_dragDelta);
            updateDrop(p);
            refresh();
        }
    } else if (QLineF(m_press, p).length() > 4) {
        m_marquee = true;
        m_marqueeRect = QRectF(mapToWorld(m_press), mapToWorld(p)).normalized();
        refresh();
    }
    m_last = p;
    e->accept();
}
void MindCanvas::mouseReleaseEvent(QMouseEvent *e) {
    if (!m_engine)
        return;
    if (m_dragging) {
        updateDrop(e->position());
        int id = m_pressedId, parent = m_dropParent, before = m_before;
        QPointF delta = m_dragDelta;
        // Adopt the rendered preview before notifying the document, avoiding
        // an animation from the pre-drag positions after a successful drop.
        if(!m_manualPreview.isEmpty()) { m_target=m_manualPreview; m_previous=m_target; }
        m_manualPreview.clear();
        m_dragging = false;
        m_dragDelta = {};
        m_dragIds.clear();
        if (m_engine->manual())
            m_engine->moveManual(id, delta.x(), delta.y());
        else if (parent >= 0)
            m_engine->moveNode(id, parent, before);
    } else if (e->button() == Qt::LeftButton && m_pressedTask >= 0 &&
               taskHit(e->position()) == m_pressedTask) {
        m_engine->select(m_pressedTask);
        m_engine->toggleChecked();
    } else if(!m_panning && !m_extend && m_pressedId>=0 && hit(e->position())==m_pressedId &&
              m_engine->nodes().value(m_pressedId).kind=="date") {
        const int id=m_pressedId; const auto n=m_engine->nodes().value(id);
        const QPointF local=mapToWorld(e->position())-displayRect(id).topLeft();
        if(Calendar::previous().contains(local)) m_engine->shiftDateNode(id,-1);
        else if(Calendar::next().contains(local)) m_engine->shiftDateNode(id,1);
        else {
            const auto days=Calendar::days(n.calendar);
            for(int i=0;i<days.size();++i) if(days[i].isValid() && Calendar::cell(i).contains(local)) {
                editDateEntry(id,days[i].toString(Qt::ISODate)); break;
            }
        }
    } else if (m_marquee) {
        QVector<int> ids;
        for (const auto &n : m_draw)
            if (m_marqueeRect.intersects(n.rect))
                ids << n.id;
        QVariantList batch;
        for (int id : ids)
            batch << id;
        m_engine->selectMany(batch, m_extend);
    }
    m_panning = false;
    m_marquee = false;
    m_pressedId = -1;
    m_pressedTask = -1;
    m_dropParent = -1;
    m_hint = "Click to select · double-click to edit · Tab adds a child";
    emit interactionChanged();
    refresh();
    e->accept();
}
void MindCanvas::mouseDoubleClickEvent(QMouseEvent *e) {
    if (editing()) {
        emit commitRequested();
        if (editing()) {
            e->accept();
            return;
        }
    }

    if (taskHit(e->position()) >= 0) {
        m_pressedTask = m_pressedId = -1;
        e->accept();
        return;
    }
    int id = hit(e->position());
    if (id >= 0 && m_engine) {
        m_engine->select(id);
        beginEdit(id);
    }
    e->accept();
}
void MindCanvas::editDateEntry(int id,QString date) {
    if(!m_engine || !m_engine->nodes().contains(id) || m_engine->nodes().value(id).kind!="date" ||
       !QDate::fromString(date,Qt::ISODate).isValid()) return;
    m_dateHoverText.clear(); emit interactionChanged();
    emit dateEditRequested(id,date,m_engine->dateEntry(id,date));
}
void MindCanvas::hoverMoveEvent(QHoverEvent *e) {
    const int task = taskHit(e->position());
    int id=task >= 0 ? task : hit(e->position()); QString text; bool actionable=task >= 0;
    if(m_engine && id>=0 && m_engine->nodes().value(id).kind=="date") {
        const auto n=m_engine->nodes().value(id); const auto days=Calendar::days(n.calendar);
        const QPointF local=mapToWorld(e->position())-displayRect(id).topLeft();
        actionable=Calendar::previous().contains(local) || Calendar::next().contains(local);
        for(int i=0;i<days.size();++i) if(days[i].isValid() && Calendar::cell(i).contains(local)) {
            text=n.calendar.entries.value(days[i].toString(Qt::ISODate)); actionable=true; break;
        }
    }
    if(actionable) setCursor(Qt::PointingHandCursor); else unsetCursor();
    m_dateHoverPosition=e->position(); m_dateHoverText=text; emit interactionChanged();
    if(id!=m_hovered || task!=m_hoveredTask) {m_hovered=id; m_hoveredTask=task; refresh();}
}
void MindCanvas::hoverLeaveEvent(QHoverEvent *e) {
    m_dateHoverText.clear(); m_hovered=-1; m_hoveredTask=-1; unsetCursor(); emit interactionChanged(); refresh(); e->accept();
}
void MindCanvas::wheelEvent(QWheelEvent *e) {
    if (e->modifiers() & Qt::ControlModifier || e->pixelDelta().isNull()) {
        double delta = e->angleDelta().y();
        if (delta == 0)
            delta = e->pixelDelta().y();
        zoomAt(e->position(), std::exp(delta * .0015));
    } else {
        m_pan += e->pixelDelta();
        refreshView();
    }
    e->accept();
}
void MindCanvas::keyPressEvent(QKeyEvent *e) {
    if (!m_engine || editing()) {
        e->ignore();
        return;
    }
    bool cmd = e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier),
         shift = e->modifiers() & Qt::ShiftModifier;
    bool handled = true;
    if (cmd && e->key() == Qt::Key_Z) {
        if (shift)
            m_engine->redo();
        else
            m_engine->undo();
    } else if (cmd && e->key() == Qt::Key_Y)
        m_engine->redo();
    else if ((cmd && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)) ||
             e->key() == Qt::Key_F2)
        editSelected();
    else
        switch (e->key()) {
        case Qt::Key_Tab:
            m_engine->addChild();
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            m_engine->addSibling();
            break;
        case Qt::Key_Left:
            m_engine->navigate("Left", shift);
            ensureVisible(m_engine->selectedId());
            break;
        case Qt::Key_Right:
            m_engine->navigate("Right", shift);
            ensureVisible(m_engine->selectedId());
            break;
        case Qt::Key_Up:
            m_engine->navigate("Up", shift);
            ensureVisible(m_engine->selectedId());
            break;
        case Qt::Key_Down:
            m_engine->navigate("Down", shift);
            ensureVisible(m_engine->selectedId());
            break;
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            m_engine->removeSelected();
            break;
        case Qt::Key_Space:
            m_space = true;
            setCursor(Qt::OpenHandCursor);
            break;
        case Qt::Key_F:
            if (cmd)
                fit();
            else
                m_engine->toggleFold();
            break;
        case Qt::Key_T:
            m_engine->toggleTask();
            break;
        case Qt::Key_Escape:
            m_engine->select(-1);
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
            zoomOut();
            break;
        case Qt::Key_0:
            fit();
            break;
        default:
            handled = false;
            break;
        }
    if (handled)
        e->accept();
    else
        e->ignore();
}
void MindCanvas::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) {
        m_space = false;
        unsetCursor();
        e->accept();
    } else
        e->ignore();
}

void MindCanvas::panBy(double dx, double dy) {
    m_pan += QPointF(dx, dy);
    refreshView();
}
void MindCanvas::formatText(QObject *editor, QString command) {
    if (!editor)
        return;
    auto *quickDoc = qvariant_cast<QQuickTextDocument *>(editor->property("textDocument"));
    if (!quickDoc || !quickDoc->textDocument())
        return;
    QTextCursor cursor(quickDoc->textDocument());
    int start = editor->property("selectionStart").toInt(),
        end = editor->property("selectionEnd").toInt();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    if (start == end)
        cursor.select(QTextCursor::WordUnderCursor);
    QTextCharFormat format;
    if (command == "bold")
        format.setFontWeight(cursor.charFormat().fontWeight() >= QFont::Bold ? QFont::Normal
                                                                             : QFont::Bold);
    else if (command == "italic")
        format.setFontItalic(!cursor.charFormat().fontItalic());
    else if (command == "underline")
        format.setFontUnderline(!cursor.charFormat().fontUnderline());
    else
        return;
    cursor.mergeCharFormat(format);
    if (auto *item = qobject_cast<QQuickItem *>(editor))
        item->forceActiveFocus();
}

void MindCanvas::refreshView() {
    const QRectF view(mapToWorld({0, 0}), mapToWorld({width(), height()}));
    if (m_cullViewport.contains(view)) {
        emit viewChanged();
        update();
    } else
        refresh();
}
