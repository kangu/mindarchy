#include "appfont.h"
#include "canvas.h"
#include "searchmatch.h"
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
#include <QPointingDevice>
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
    setFlag(ItemAcceptsDrops);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
    setAcceptHoverEvents(true);
    setFocus(true);
    setClip(true);
    m_focusAnimation.setDuration(650);
    m_focusAnimation.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_focusAnimation,&QVariantAnimation::finished,this,[this] {
        m_focusReturning=false;
        refresh();
    });
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
    if (focusActive()) exitFocus();
    if(m_focusReturning) { m_zoom=m_beforeFocusZoom; m_pan=m_beforeFocusPan; }
    stopFocusAnimation();
    m_engine = e;
    if (e) {
        connect(e, &Engine::changed, this, &MindCanvas::documentChanged);
        connect(e, &Engine::editRequested, this, &MindCanvas::beginEdit);
        documentChanged();
    }
    emit engineChanged();
}
QRectF MindCanvas::displayRect(int id) const {
    if(id==m_imageSelected && m_imageResizeHandle>=0 && m_engine) {
        const auto n=m_engine->nodes().value(id);
        QRectF resized(QPointF(),n.image.expanded(m_engine->contentSize(id),m_imageResizeWidth));
        resized.moveCenter(m_target.value(id).center());
        return resized;
    }
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
    if(m_imageSelected>=0 && (!m_engine || !m_engine->hasImage(m_imageSelected) || !m_engine->selectedIds().contains(m_imageSelected))) {
        m_imageSelected=-1; m_imageResizeHandle=-1;
    }
    if(!m_dateHoverText.isEmpty()) { m_dateHoverText.clear(); emit interactionChanged(); }
    if (!m_engine)
        return;
    if (focusActive()) {
        if (!m_engine->nodes().contains(m_focusRoot)) exitFocus();
        else { updateFocusIds(); emit focusChanged(); }
    }
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
        if (!focusIncludes(id)) color=QColor::fromRgbF(color.redF()*.18+m_canvasColor.redF()*.82,
            color.greenF()*.18+m_canvasColor.greenF()*.82,color.blueF()*.18+m_canvasColor.blueF()*.82);
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
            m_engine->manual() && m_engine->layout()=="Horizontal" && r.center().x()<displayRect(1).center().x(), !focusIncludes(id), taskOpacity, n.taskChildren>0 ? qreal(n.completedTaskChildren)/n.taskChildren : -1});
        if ((m_zoom < .28 || editingId() == id) && n.image.empty())
            continue;
        // Task conversion changes padding, not the text's wrapping width.
        // Rasterize at the destination size and slide the label with the box.
        QRectF labelRect = r;
        if (m_animating && m_previousTasks.value(id, m_targetTasks.value(id)) != m_targetTasks.value(id)) {
            labelRect.setSize(m_target.value(id).size());
            labelRect.translate(20 * (taskOpacity - (n.task ? 1 : 0)), 0);
        }
        if(m_animating && !n.image.empty()) labelRect.setSize(m_target.value(id).size());
        const QRectF content=contentRect(id,QRectF(QPointF(),labelRect.size()));
        const QString labelKey=(n.kind=="date" ? Calendar::key(n.calendar)+appearance.branch.name(QColor::HexArgb) : n.text)
            + (id==m_searchResult ? "\nsearch:"+m_searchQuery+appearance.fill.name(QColor::HexArgb)+m_canvasColor.name(QColor::HexArgb) : QString()) + (!focusIncludes(id) ? "\nfocus-dim" : "") + "\nimage:" + QString::number(n.image.pixels.cacheKey()) + ":" + QString::number(imageWidth(id)) + n.image.placement + (m_zoom<.28 ? ":overview" : ":detail") + (editingId()==id ? "editing" : "");
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
                QPainter painter(&image); painter.translate(content.topLeft()); paintCalendar(painter,n.calendar,appearance);
            } else if(editingId()!=id && m_zoom>=.28) {
            QTextDocument doc;
            QFont font(mindarchyTextFamily(), 11);
            font.setPixelSize(15);
            doc.setDefaultFont(font);
            doc.setDocumentMargin(0);
            doc.setDefaultStyleSheet(QString("body,p {color:%1; margin:0;}").arg(appearance.text.name()));
            doc.setHtml(n.text);
            doc.setTextWidth(std::max(0., content.width() - 30 - (n.task ? 20 : 0)));
            QPainter painter(&image);
            painter.setRenderHint(QPainter::TextAntialiasing);
            painter.translate(content.left()+15 + (n.task ? 20 : 0),
                              content.top()+std::max(8., (content.height() - doc.size().height()) / 2));
            QAbstractTextDocumentLayout::PaintContext ctx;
            ctx.palette.setColor(QPalette::Text, appearance.text);
            if(id==m_searchResult && !m_searchQuery.isEmpty()) {
                QSet<int> positions;
                for(const auto &term:m_searchQuery.split(' ',Qt::SkipEmptyParts))
                    positions.unite(Search::match(doc.toPlainText(),term).positions);
                const QColor surface=appearance.fill.alpha()>0 ? appearance.fill : m_canvasColor;
                const bool light=surface.lightnessF()>0.5;
                for(int position:positions) {
                    QAbstractTextDocumentLayout::Selection selection;
                    selection.cursor=QTextCursor(&doc);
                    selection.cursor.setPosition(position);
                    selection.cursor.setPosition(position+1,QTextCursor::KeepAnchor);
                    selection.format.setBackground(QColor(light ? "#6040a8" : "#ffe08a"));
                    selection.format.setForeground(QColor(light ? "#ffffff" : "#25212b"));
                    ctx.selections.append(selection);
                }
            }
            doc.documentLayout()->draw(&painter, ctx);
            painter.end();
            }
            if(!n.image.empty()) {
                QPainter painter(&image); painter.setRenderHint(QPainter::SmoothPixmapTransform);
                painter.drawImage(n.image.rect(QRectF(QPointF(),labelRect.size()),imageWidth(id)),n.image.pixels);
            }
            if (!focusIncludes(id)) {
                QPainter dim(&image); dim.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                dim.fillRect(QRectF(QPointF(),labelRect.size()),QColor(0,0,0,46));
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
            m_edges.append({a, b, focusIncludes(link.first) && focusIncludes(link.second) ? QColor("#efb86f")
                : QColor::fromRgbF(.18*.94+.82*m_canvasColor.redF(),.18*.72+.82*m_canvasColor.greenF(),.18*.44+.82*m_canvasColor.blueF()), false, false});
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
            painter.setOpacity(n.dimmed ? .18 : 1.);
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
                const QRectF check(contentRect(n.id,n.rect).left()+8,contentRect(n.id,n.rect).center().y()-5,10,10);
                if (n.id == m_hoveredTask && !m_dragging && !m_panning) {
                    QColor tint = n.appearance.text;
                    tint.setAlphaF(.04);
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(tint);
                    painter.drawRoundedRect(check.adjusted(-3,-3,3,3),3,3);
                }
                painter.save();
                painter.setOpacity(n.taskOpacity * (n.dimmed ? .18 : 1.));
                paintTask(painter, check, n.appearance.text, n.checked, n.completion);
                painter.restore();
            }
        }
        painter.setOpacity(1.);
        for (const auto &l : m_labels)
            painter.drawImage(l.rect, l.image);
        if (m_dragging && !m_dropLine.isNull()) {
            painter.setPen(QPen(QColor("#f3cc79"),2./m_zoom));
            painter.drawLine(m_dropLine);
        }
        const auto handle=creationHandleRect();
        if(!handle.isEmpty()) {
            const int id=m_creatingParent>=0 ? m_creatingParent : m_hovered;
            QColor branch=m_engine->appearance(id).branch;
            QColor shadow=QColor::fromRgbF(branch.redF()*.45+m_canvasColor.redF()*.55,
                branch.greenF()*.45+m_canvasColor.greenF()*.55,branch.blueF()*.45+m_canvasColor.blueF()*.55);
            const auto center=mapToWorld(handle.center());
            painter.setPen(QPen(shadow,2/m_zoom));
            if(m_creationDragged) painter.drawPolyline(creationPreview());
            else painter.drawLine(creationAnchor(id),center);
            painter.setPen(Qt::NoPen); painter.setBrush(creationHandleColor());
            painter.drawEllipse(center,12/m_zoom,12/m_zoom);
            painter.setPen(QPen(m_canvasColor,2/m_zoom,Qt::SolidLine,Qt::RoundCap));
            painter.drawLine(center-QPointF(5/m_zoom,0),center+QPointF(5/m_zoom,0));
            painter.drawLine(center-QPointF(0,5/m_zoom),center+QPointF(0,5/m_zoom));
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
        const auto vertexStart=vertices.size();
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
            QRectF check(contentRect(n.id,n.rect).left() + 8, contentRect(n.id,n.rect).center().y() - 5, 10, 10);
            if (n.id == m_hoveredTask && !m_dragging && !m_panning) {
                QColor tint = n.appearance.text;
                tint.setAlphaF(.04);
                box(vertices, check.adjusted(-3,-3,3,3), tint, 3);
            }
            if(n.completion>=0) {
                const QColor base=n.appearance.fill.alpha() ? n.appearance.fill : m_canvasColor;
                const QColor ink=n.appearance.text;
                QColor track=QColor::fromRgbF(base.redF()*.75+ink.redF()*.25,
                    base.greenF()*.75+ink.greenF()*.25,base.blueF()*.75+ink.blueF()*.25);
                track.setAlphaF(n.taskOpacity);
                strokePath(vertices,taskProgressArc(check,1),2.5,track,Qt::SolidLine);
                if(n.completion>0) {
                    QColor fill=taskCheckColor; fill.setAlphaF(n.taskOpacity);
                    strokePath(vertices,taskProgressArc(check,n.completion),2.5,fill,Qt::SolidLine);
                }
            } else {
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
        }
        if (n.folded)
            box(vertices, QRectF(n.expandsLeft ? n.rect.left()-12 : n.rect.right()+4, n.rect.center().y() - 4, 8, 8), n.color, 4);
        if(n.dimmed) for(auto i=vertexStart;i<vertices.size();++i) {
            auto &v=vertices[i]; v.r=uchar(v.r*.18+m_canvasColor.red()*.82);
            v.g=uchar(v.g*.18+m_canvasColor.green()*.82); v.b=uchar(v.b*.18+m_canvasColor.blue()*.82);
        }
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
    const auto handle=creationHandleRect();
    if(!handle.isEmpty()) {
        const int id=m_creatingParent>=0 ? m_creatingParent : m_hovered;
        const auto style=m_engine->appearance(id);
        const auto branch=style.branch;
        const QColor shadow=QColor::fromRgbF(branch.redF()*.45+m_canvasColor.redF()*.55,
            branch.greenF()*.45+m_canvasColor.greenF()*.55,branch.blueF()*.45+m_canvasColor.blueF()*.55);
        const auto center=mapToWorld(handle.center());
        if(m_creationDragged) strokePath(vertices,creationPreview(),2/m_zoom,shadow,Qt::SolidLine);
        else line(vertices,creationAnchor(id),center,2/m_zoom,shadow);
        box(vertices,QRectF(center-QPointF(12/m_zoom,12/m_zoom),QSizeF(24/m_zoom,24/m_zoom)),creationHandleColor(),12/m_zoom);
        line(vertices,center-QPointF(5/m_zoom,0),center+QPointF(5/m_zoom,0),2/m_zoom,m_canvasColor);
        line(vertices,center-QPointF(0,5/m_zoom),center+QPointF(0,5/m_zoom),2/m_zoom,m_canvasColor);
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
    // Resizing (including panel toggles) preserves user zoom and the world-space center.
    // Never fit here: fit is an explicit action or document initialization.
    m_pan += QPointF((a.width() - b.width()) / 2, (a.height() - b.height()) / 2);
    refresh();
}
void MindCanvas::zoomAt(QPointF p, double factor) {
    stopFocusAnimation();
    QPointF world = mapToWorld(p);
    m_zoom = std::clamp(m_zoom * factor, .00001, 4.);
    m_pan = p - world * m_zoom;
    refresh();
}
void MindCanvas::zoomIn() { zoomAt({width() / 2, height() / 2}, 1.25); }
void MindCanvas::zoomOut() { zoomAt({width() / 2, height() / 2}, .8); }
void MindCanvas::resetZoom() { zoomAt({width() / 2, height() / 2}, 1. / m_zoom); }
bool MindCanvas::restoreView(double zoom, QPointF center) {
    if (!std::isfinite(zoom) || zoom < .00001 || zoom > 4. ||
        !std::isfinite(center.x()) || !std::isfinite(center.y())) return false;
    stopFocusAnimation();
    m_zoom = zoom;
    m_pan = QPointF(width()/2, height()/2) - center * zoom;
    refresh();
    return true;
}
void MindCanvas::fit() {
    stopFocusAnimation();
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
    if(focusActive() && !focusIncludes(id)) exitFocus();
    stopFocusAnimation();
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
    QRectF r = contentRect(m_editingId,displayRect(m_editingId));
    return {mapFromWorld(r.topLeft()), r.size() * m_zoom};
}
void MindCanvas::setEditing(bool value) {
    if (value)
        editSelected();
    else
        endEdit();
}
void MindCanvas::beginEdit(int id) {
    stopFocusAnimation();
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
    m_editContentSize = m_engine->contentSize(id);
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
    m_editContentSize = m_engine->previewContentSize(m_editingId,text);
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
        if (!it->dimmed && (!excludeDrag || !m_dragIds.contains(it->id)) && it->rect.contains(w))
            return it->id;
    return -1;
}
int MindCanvas::taskHit(QPointF screen) const {
    if (!m_engine || m_zoom <= .28)
        return -1;
    const QPointF world = mapToWorld(screen);
    for (auto it = m_draw.crbegin(); it != m_draw.crend(); ++it) {
        if (!it->dimmed && it->task && m_engine->nodes().value(it->id).kind != "date" && m_engine->nodes().value(it->id).taskChildren==0) {
            const QPointF center = mapFromWorld(QPointF(contentRect(it->id,it->rect).left() + 13, contentRect(it->id,it->rect).center().y()));
            const qreal half = std::max(16., 8. * m_zoom);
            QRectF target(center - QPointF(half, half), QSizeF(half * 2, half * 2));
            // Preserve a gap before the text, especially when zoomed out.
            target.setRight(std::min(target.right(), mapFromWorld(contentRect(it->id,it->rect).topLeft()).x() + 32 * m_zoom));
            if (target.contains(screen))
                return it->id;
        }
        // An overlapping node in front owns its own pointer events.
        if (it->rect.contains(world))
            return -1;
    }
    return -1;
}
QPointF MindCanvas::creationAnchor(int id, std::optional<QPointF> toward) const {
    const auto r=displayRect(id);
    if(m_engine->layout()=="Vertical") return {r.center().x(),r.bottom()};
    if(m_engine->layout()=="Compact") return {r.left()+12,r.bottom()};
    const int parent=m_engine->nodes().value(id).parent;
    const bool left=toward ? toward->x()<r.center().x() :
        m_engine->manual() && parent>=0 && r.center().x()<displayRect(parent).center().x();
    return {left ? r.left() : r.right(),
            m_engine->appearance(id).shape==NodeShape::Underline ? r.bottom() : r.center().y()};
}
QColor MindCanvas::creationHandleColor() const {
    // This control belongs to the canvas, not to an individual node's palette.
    const QColor dark("#25212b"), light("#fff8ed");
    auto luminance=[](QColor c) {
        auto linear=[](double v) { return v<=.04045 ? v/12.92 : std::pow((v+.055)/1.055,2.4); };
        return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());
    };
    const double background=luminance(m_canvasColor);
    auto contrast=[&](QColor c) {
        const double value=luminance(c);
        return (std::max(value,background)+.05)/(std::min(value,background)+.05);
    };
    return contrast(dark)>=contrast(light) ? dark : light;
}
QRectF MindCanvas::creationHandleRect() const {
    if(m_imageResizeHandle>=0) return {};
    const int id=m_creatingParent>=0 ? m_creatingParent : m_hovered;
    if(!m_engine || !m_target.contains(id) || m_dragging || m_panning || m_marquee) return {};
    if(m_creatingParent>=0 && m_creationDragged)
        return {mapFromWorld(m_creationEnd)-QPointF(12,12),QSizeF(24,24)};
    const auto anchor=creationAnchor(id);
    auto center=mapFromWorld(anchor);
    if(m_engine->layout()!="Horizontal") center.ry()+=18;
    else center.rx()+=anchor.x()<displayRect(id).center().x() ? -18 : 18;
    return {center-QPointF(12,12),QSizeF(24,24)};
}
QPolygonF MindCanvas::creationPreview() const {
    if(m_creatingParent<0 || !m_creationDragged || !m_target.contains(m_creatingParent)) return {};
    return edgePath(creationAnchor(m_creatingParent,m_creationEnd),m_creationEnd,
                    m_engine->branchStyle()=="Angular" || m_engine->layout()=="Compact",
                    m_engine->layout()!="Horizontal",m_zoom*(window()?window()->effectiveDevicePixelRatio():1));
}
void MindCanvas::cancelCreation() {
    m_creatingParent=-1; m_creationDragged=false;
    unsetCursor(); emit interactionChanged(); refresh();
}
void MindCanvas::mouseUngrabEvent() {
    cancelImageResize();
    if(m_creatingParent>=0) cancelCreation();
    QQuickItem::mouseUngrabEvent();
}
void MindCanvas::mousePressEvent(QMouseEvent *e) {
    stopFocusAnimation();
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
    if(e->button()==Qt::LeftButton && !m_space && imageHandleHit(e->position())>=0) {
        m_imageResizeHandle=imageHandleHit(e->position());
        m_imageResizeStart=imageWorldRect(m_imageSelected);
        m_imageResizeWidth=m_engine->nodes().value(m_imageSelected).image.width;
        m_press=e->position(); m_animating=false; m_animationTimer.stop(); e->accept(); return;
    }
    const int imageHit=hit(e->position());
    const bool onImage=imageHit>=0 && imageWorldRect(imageHit).contains(mapToWorld(e->position()));
    if(onImage && e->button()==Qt::RightButton) {
        m_engine->select(imageHit); m_imageSelected=imageHit; refresh();
        emit imageMenuRequested(imageHit,e->position().x(),e->position().y()); e->accept(); return;
    }
    m_imageSelected=onImage ? imageHit : -1;
    if(e->button()==Qt::LeftButton && !m_space && creationHandleRect().contains(e->position())) {
        m_creatingParent=m_hovered; m_creationDragged=false;
        m_press=m_last=e->position(); m_creationEnd=mapToWorld(m_press);
        m_dateHoverText.clear(); setCursor(Qt::CrossCursor);
        emit interactionChanged(); refresh(); e->accept(); return;
    }
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
    m_dragIds.clear(); m_dragRoots.clear(); m_deferredSelection=false;
    m_manualPreview.clear();
    m_dragDelta = {};
    m_dropParent = -1;
    m_before = -1;
    if (!m_panning) {
        if (m_pressedId >= 0) {
            m_deferredSelection=!m_extend && m_engine->selectedIds().contains(m_pressedId) && m_engine->selectedIds().size()>1;
            if(!m_deferredSelection) m_engine->select(m_pressedId, m_extend);
            const auto roots=m_engine->branchRoots(m_engine->selectedIds());
            for(int root:roots) {
                m_dragRoots.insert(root);
                QVector<int> pending{root};
                while(!pending.isEmpty()) { const int id=pending.takeLast(); m_dragIds.insert(id); pending+=m_engine->nodes().value(id).children; }
            }
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
        if (!m_engine->manual() && (fraction < .25 || fraction > .75) && n.parent >= 0) {
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
            if (m_dragIds.contains(id)) continue;
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
            if (!m_dragIds.contains(id)) siblings.append(id);
        if (!siblings.isEmpty()) {
            // Skip the dragged node when an edge hit points at its old position.
            const auto all=m_engine->nodes().value(m_dropParent).children;
            while(m_dragIds.contains(m_before)) {
                const int next=all.indexOf(m_before)+1;
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
    if(m_dragIds.contains(m_dropParent) || m_dragRoots.contains(1)) { m_dropParent=-1; m_before=-1; m_dropLine={}; }
    m_hint = !m_dropLine.isNull() ? "Release to reorder selected branches"
             : m_dropParent >= 0 ? "Release to attach selected branches to highlighted parent"
             : m_engine->manual() ? "Release to place selected branches"
                                 : "Drop on a node to attach · near its edge to reorder";
    emit interactionChanged();
}
void MindCanvas::mouseMoveEvent(QMouseEvent *e) {
    if (!m_engine)
        return;
    QPointF p = e->position();
    if(m_imageResizeHandle>=0) {
        const auto image=m_engine->nodes().value(m_imageSelected).image;
        const auto delta=(p-m_press)/m_zoom;
        // Center-anchored scaling: any edge/corner preserves the aspect ratio.
        static const QPointF directions[]={{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
        const auto d=directions[m_imageResizeHandle];
        const QPointF v(d.x()*m_imageResizeStart.width()/2,d.y()*m_imageResizeStart.height()/2);
        const double factor=1+(delta.x()*v.x()+delta.y()*v.y())/(v.x()*v.x()+v.y()*v.y());
        m_imageResizeWidth=image.boundedWidth(m_imageResizeStart.width()*factor);
        refresh(); emit viewChanged(); e->accept(); return;
    }
    if(m_creatingParent>=0) {
        m_creationDragged |= QLineF(m_press,p).length()>=QGuiApplication::styleHints()->startDragDistance();
        m_creationEnd=mapToWorld(p); m_last=p;
        emit interactionChanged(); refresh(); e->accept(); return;
    }
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
                m_manualPreview=m_engine->manualGeometry(m_dragRoots,m_dragDelta);
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
    if(m_imageResizeHandle>=0) {
        const int id=m_imageSelected; const double width=m_imageResizeWidth;
        m_target[id]=displayRect(id);
        m_imageResizeHandle=-1;
        m_engine->resizeImage(id,width);
        refresh(); emit viewChanged(); e->accept(); return;
    }
    if (!m_engine)
        return;
    if(m_creatingParent>=0) {
        const int parent=m_creatingParent;
        const bool dragged=m_creationDragged;
        const bool accepted=e->button()==Qt::LeftButton && boundingRect().contains(e->position()) &&
            (dragged || creationHandleRect().contains(e->position()));
        const QPointF destination=mapToWorld(e->position());
        cancelCreation();
        if(accepted) {
            m_animating=false; m_animationTimer.stop();
            m_engine->addChildFromPointer(parent,dragged ? std::optional<QPointF>(destination) : std::nullopt);
        }
        e->accept(); return;
    }
    if (m_dragging) {
        updateDrop(e->position());
        int parent = m_dropParent, before = m_before;
        const auto roots=m_dragRoots;
        m_deferredSelection=false;
        QPointF delta = m_dragDelta;
        // Adopt the rendered preview before notifying the document, avoiding
        // an animation from the pre-drag positions after a successful drop.
        if(!m_manualPreview.isEmpty()) { m_target=m_manualPreview; m_previous=m_target; }
        m_manualPreview.clear();
        m_dragging = false;
        m_dragDelta = {};
        m_dragIds.clear();
        m_dragRoots.clear();
        if (m_engine->manual() || parent >= 0) {
            if(!m_engine->moveBranches(roots,parent,before,delta)) documentChanged();
        }
    } else if (e->button() == Qt::LeftButton && m_pressedTask >= 0 &&
               taskHit(e->position()) == m_pressedTask) {
        m_engine->select(m_pressedTask);
        m_engine->toggleChecked();
    } else if(!m_panning && !m_extend && m_pressedId>=0 && hit(e->position())==m_pressedId &&
              m_engine->nodes().value(m_pressedId).kind=="date" && m_imageSelected!=m_pressedId) {
        const int id=m_pressedId; const auto n=m_engine->nodes().value(id);
        const QPointF local=mapToWorld(e->position())-contentRect(id,displayRect(id)).topLeft();
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
    if(m_deferredSelection && m_pressedId>=0) m_engine->select(m_pressedId);
    m_deferredSelection=false;
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
    const int imageId=hit(e->position());
    if(imageId>=0 && imageWorldRect(imageId).contains(mapToWorld(e->position()))) {
        emit imagePreviewRequested(imageId); e->accept(); return;
    }
    if(creationHandleRect().contains(e->position())) { e->accept(); return; }
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
    const auto handle=creationHandleRect();
    if(!handle.isEmpty() && handle.adjusted(-4,-4,4,4).contains(e->position())) {
        id=m_hovered; actionable=true;
    } else if(id<0 && m_target.contains(m_hovered)) {
        const QRectF nodeScreen(mapFromWorld(displayRect(m_hovered).topLeft()),displayRect(m_hovered).size()*m_zoom);
        if(nodeScreen.united(handle).contains(e->position())) id=m_hovered;
    }
    if(m_engine && id>=0 && m_engine->nodes().value(id).kind=="date") {
        const auto n=m_engine->nodes().value(id); const auto days=Calendar::days(n.calendar);
        const QPointF local=mapToWorld(e->position())-contentRect(id,displayRect(id)).topLeft();
        actionable=Calendar::previous().contains(local) || Calendar::next().contains(local);
        for(int i=0;i<days.size();++i) if(days[i].isValid() && Calendar::cell(i).contains(local)) {
            text=n.calendar.entries.value(days[i].toString(Qt::ISODate)); actionable=true; break;
        }
    }
    const int handleIndex=imageHandleHit(e->position());
    if(handleIndex>=0) {
        static const Qt::CursorShape cursors[]={Qt::SizeFDiagCursor,Qt::SizeVerCursor,Qt::SizeBDiagCursor,Qt::SizeHorCursor,Qt::SizeFDiagCursor,Qt::SizeVerCursor,Qt::SizeBDiagCursor,Qt::SizeHorCursor};
        setCursor(cursors[handleIndex]);
    } else if(actionable) setCursor(Qt::PointingHandCursor); else unsetCursor();
    m_dateHoverPosition=e->position(); m_dateHoverText=text; emit interactionChanged();
    if(id!=m_hovered || task!=m_hoveredTask) {m_hovered=id; m_hoveredTask=task; refresh();}
}
void MindCanvas::hoverLeaveEvent(QHoverEvent *e) {
    m_dateHoverText.clear(); m_hovered=-1; m_hoveredTask=-1; unsetCursor(); emit interactionChanged(); refresh(); e->accept();
}
void MindCanvas::wheelEvent(QWheelEvent *e) {
    if(m_imageResizeHandle>=0) { e->accept(); return; }
    const auto phase=e->phase();
    // Some Linux backends omit phases. Treat a pause in that stream as its end.
    if(phase==Qt::ScrollBegin || (phase==Qt::NoScrollPhase &&
        m_wheelClock.isValid() && m_wheelClock.elapsed()>250)) m_wheelPanning=false;
    const bool trackpad=e->pointingDevice()->type()==QInputDevice::DeviceType::TouchPad ||
        !e->pixelDelta().isNull() || phase!=Qt::NoScrollPhase;
    if(trackpad && !(e->modifiers() & Qt::ControlModifier)) m_wheelPanning=true;
    m_wheelClock.restart();
    if(m_wheelPanning) {
        // Keep both axes, even when an update supplies only angle deltas.
        const QPointF delta=e->pixelDelta().isNull()
            ? QPointF(e->angleDelta())/3.0 : QPointF(e->pixelDelta());
        if(!delta.isNull()) { stopFocusAnimation(); m_pan+=delta; refreshView(); }
    } else {
        double delta=e->angleDelta().y();
        if(delta==0) delta=e->pixelDelta().y();
        if(delta!=0) zoomAt(e->position(),std::exp(delta*.0015));
    }
    if(phase==Qt::ScrollEnd) m_wheelPanning=false;
    e->accept();
}
void MindCanvas::keyPressEvent(QKeyEvent *e) {
    if(m_imageResizeHandle>=0) {
        if(e->key()==Qt::Key_Escape) cancelImageResize();
        e->accept(); return;
    }
    if(m_creatingParent>=0) {
        if(e->key()==Qt::Key_Escape) cancelCreation();
        e->accept(); return;
    }
    if (!m_engine || editing()) {
        e->ignore();
        return;
    }
    bool cmd = e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier),
         shift = e->modifiers() & Qt::ShiftModifier;
    // Qt maps the macOS Command key to ControlModifier. Use the same
    // viewport step at every zoom level, including key auto-repeat.
    // Move the artwork opposite to the desired viewport travel; this keyboard
    // path is independent of wheel events and natural-scrolling preferences.
    if (cmd && !shift && !(e->modifiers() & Qt::AltModifier)) {
        QPointF delta;
        switch (e->key()) {
        case Qt::Key_Left: delta={40,0}; break;
        case Qt::Key_Right: delta={-40,0}; break;
        case Qt::Key_Up: delta={0,40}; break;
        case Qt::Key_Down: delta={0,-40}; break;
        default: break;
        }
        if (!delta.isNull()) {
            panBy(delta.x(),delta.y());
            e->accept();
            return;
        }
    }
    // Printable canvas input replaces a single selected title. Start the normal
    // editor first so its original-text baseline and commit/undo behavior remain intact.
    const bool alt = e->modifiers() & Qt::AltModifier;
    if (!cmd && !alt && !m_space && m_engine->selectedIds().size()==1 &&
        m_engine->selectedKind() != "date" && !e->text().isEmpty() &&
        e->text().front().isPrint() && !e->text().front().isSpace()) {
        beginEdit(m_engine->selectedId());
        if(editing()) { emit replaceEditingText(e->text().toHtmlEscaped()); e->accept(); return; }
    }
    bool handled = true;
    if (!cmd && alt && e->key()==Qt::Key_F) m_engine->toggleFold();
    else if (!cmd && alt && e->key()==Qt::Key_T) m_engine->toggleTask();
    else if (cmd && e->key() == Qt::Key_C && !shift) {
        if(m_imageSelected>=0) m_engine->copyImage(m_imageSelected); else m_engine->copyBranches();
    } else if(cmd && e->key()==Qt::Key_X && !shift && m_imageSelected>=0) m_engine->cutImage(m_imageSelected);
    else if (cmd && e->key() == Qt::Key_V && !shift) {
        if(m_engine->clipboardHasImage()) {
            if(m_engine->selectedIds().size()==1 && m_engine->pasteImage(m_engine->selectedId())) {
                m_imageSelected=m_engine->selectedId(); refresh(); ensureVisible(m_imageSelected);
            }
        } else if(m_engine->pasteBranches()) ensureVisible(m_engine->selectedId());
    } else if(cmd && !shift && e->key()==Qt::Key_L) {
        if(m_engine->selectedIds().size()==2) m_engine->connectSelection();
    } else if(cmd && shift && e->key()==Qt::Key_F) {
        if(focusActive()) exitFocus(); else focusBranch();
    } else if (cmd && e->key() == Qt::Key_Z) {
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
            if(e->modifiers()==Qt::NoModifier && m_imageSelected>=0 &&
               m_engine->hasImage(m_imageSelected) && m_engine->selectedIds().contains(m_imageSelected) &&
               !m_dragging && !m_panning && !m_marquee) {
                if(!e->isAutoRepeat()) emit imagePreviewRequested(m_imageSelected);
                break;
            }
            m_space = true;
            setCursor(Qt::OpenHandCursor);
            break;
        case Qt::Key_Escape:
            if(focusActive()) { exitFocus(); break; }
            m_engine->select(-1);
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            if(m_engine->selectedId()>0 || cmd || alt) { handled=false; break; }
            zoomIn();
            break;
        case Qt::Key_Minus:
            if(m_engine->selectedId()>0 || cmd || alt) { handled=false; break; }
            zoomOut();
            break;
        case Qt::Key_0:
            if(m_engine->selectedId()>0 || cmd || alt) { handled=false; break; }
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
    stopFocusAnimation();
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


void MindCanvas::focusSearchResult(int id, QString query) {
    if(focusActive() && !focusIncludes(id)) exitFocus();
    stopFocusAnimation();
    if(!m_engine || !m_engine->revealSearchNode(id)) return;
    m_animating=false; m_animationTimer.stop();
    m_searchResult=id; m_searchQuery=query;
    m_pan=QPointF(width()/2,height()/2)-m_engine->nodes().value(id).rect.center()*m_zoom;
    refresh(); emit searchResultFocused();
}
QRectF MindCanvas::searchResultRect() const {
    if(!m_engine || !m_engine->nodes().contains(m_searchResult)) return {};
    const auto rect=displayRect(m_searchResult);
    return {mapFromWorld(rect.topLeft()),rect.size()*m_zoom};
}

void MindCanvas::clearSearchHighlight() { m_searchResult=-1; m_searchQuery.clear(); refresh(); }

void MindCanvas::updateFocusIds() {
    m_focusIds.clear();
    if(!m_engine || !focusActive()) return;
    QVector<int> pending{m_focusRoot};
    while(!pending.isEmpty()) { const int id=pending.takeLast();m_focusIds.insert(id);pending+=m_engine->nodes().value(id).children; }
    for(int id=m_engine->nodes().value(m_focusRoot).parent;id>=0;id=m_engine->nodes().value(id).parent) m_focusIds.insert(id);
}
QVariantList MindCanvas::focusBreadcrumb() const {
    QVariantList result;
    if(!m_engine || !focusActive()) return result;
    for(int id=m_focusRoot;id>=0;id=m_engine->nodes().value(id).parent) {
        QTextDocument title;title.setHtml(m_engine->nodes().value(id).text);
        result.prepend(QVariantMap{{"id",id},{"text",title.toPlainText()}});
    }
    return result;
}
void MindCanvas::stopFocusAnimation() {
    m_focusAnimation.stop();
    m_focusReturning=false;
}
void MindCanvas::animateFocusView(double zoom,QPointF pan) {
    m_focusAnimation.stop();
    disconnect(&m_focusAnimation,&QVariantAnimation::valueChanged,this,nullptr);
    const QPointF center(width()/2,height()/2);
    const auto fromCenter=(center-m_pan)/m_zoom;
    const auto toCenter=(center-pan)/zoom;
    const auto fromZoom=m_zoom;
    connect(&m_focusAnimation,&QVariantAnimation::valueChanged,this,[this,fromCenter,toCenter,fromZoom,zoom](const QVariant &value) {
        const auto t=value.toDouble();
        // Logarithmic scale interpolation makes a long zoom feel evenly paced.
        m_zoom=t>=1 ? zoom : std::exp(std::log(fromZoom)+(std::log(zoom)-std::log(fromZoom))*t);
        m_pan=QPointF(width()/2,height()/2)-(fromCenter+(toCenter-fromCenter)*t)*m_zoom;
        refresh();
    });
    m_focusAnimation.setStartValue(0.); m_focusAnimation.setEndValue(1.);
    m_focusAnimation.start();
}
void MindCanvas::focusBranch(int id) {
    if(!m_engine || editing()) return;
    if(id<0) id=m_engine->selectedId();
    if(!m_engine->nodes().contains(id)) return;
    if(!focusActive() && !m_focusReturning) { m_beforeFocusPan=m_pan;m_beforeFocusZoom=m_zoom; }
    m_focusReturning=false;
    m_engine->select(id);
    m_focusRoot=id;updateFocusIds();m_hovered=-1;m_hoveredTask=-1;
    const auto rect=displayRect(id);
    const double fitted=std::min(width()*.65/std::max(1.,rect.width()),height()*.55/std::max(1.,rect.height()));
    const double zoom=std::clamp(std::max(m_zoom,std::min(m_zoom*2.2,fitted)),.00001,4.);
    animateFocusView(zoom,QPointF(width()/2,height()/2)-rect.center()*zoom);
    refresh();emit focusChanged();
}
void MindCanvas::exitFocus() {
    if(!focusActive()) return;
    m_focusRoot=-1;m_focusIds.clear();m_focusReturning=true;
    animateFocusView(m_beforeFocusZoom,m_beforeFocusPan);
    refresh();emit focusChanged();
}
