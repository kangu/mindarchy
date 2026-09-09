#include "engine.h"
#include "searchmatch.h"
#include <QTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QTextCursor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int MaxNodes = 10000, MaxText = 16384, MaxDepth = 512;
QString localPath(const QString &path) {
    return path.startsWith("file:") ? QUrl(path).toLocalFile() : path;
}
bool validLayout(const QString &s) {
    return s == "Horizontal" || s == "Vertical" || s == "Compact";
}
bool validSpacing(const QString &s) { return s == "Narrow" || s == "Standard" || s == "Wide"; }
bool validBranch(const QString &s) { return s == "Rounded" || s == "Angular"; }
bool validNodeStyle(const QVariantMap &s) {
    for (auto it=s.begin(); it!=s.end(); ++it) {
        const QString k=it.key(); const QVariant v=it.value();
        if (QStringList{"fill","border","branch","textColor"}.contains(k)) {
            if (v.metaType().id()!=QMetaType::QString || !QColor(v.toString()).isValid()) return false;
        } else {
            bool ok=false; double n=v.toDouble(&ok);
            if (!ok || !std::isfinite(n) || v.metaType().id()==QMetaType::Bool || v.metaType().id()==QMetaType::QString) return false;
            if (k=="shape") { if (n<0 || n>7 || std::floor(n)!=n) return false; }
            else if (k=="borderStyle" || k=="branchStroke") { if (n<1 || n>3 || std::floor(n)!=n) return false; }
            else if (k=="width") { if (n!=0 && (n<70 || n>1200)) return false; }
            else if (k=="borderWidth" || k=="branchWidth") { if(n<0 || n>20) return false; }
            else return false;
        }
    }
    return true;
}
} // namespace
Engine::Engine(QObject *parent, InitialContent content) : QObject(parent) {
    if(content==InitialContent::Blank) {
        m_themeId = "beach-day";
        MapNode root; root.id=1; root.text="Central idea";
        m_nodes.insert(1,root); m_nextId=2;
        rebuild(); m_savedBytes = documentBytes(); return;
    }
    const QStringList labels = {"Mindarchy",     "Layout engine",       "Interaction",
                                "Document",        "Measured text",       "Horizontal / vertical",
                                "Compact outline", "Keyboard navigation", "Drag to reparent",
                                "Fold a branch",   "Local JSON files",    "Undo / redo",
                                "Tasks and notes", "Try 1,000 nodes",     "Inspect layout time"};
    for (int i = 0; i < labels.size(); ++i) {
        MapNode n;
        n.id = i + 1;
        n.parent = i == 0 ? -1 : (i <= 3 ? 1 : (i <= 6 ? 2 : (i <= 9 ? 3 : 4)));
        n.text = labels[i];
        if (n.id == 12) {
            n.task = true;
            n.checked = true;
        }
        if (n.id == 13) {
            n.task = true;
            n.notes = "Use the inspector to keep supporting details with an idea.";
        }
        m_nodes.insert(n.id, n);
        if (n.parent != -1)
            m_nodes[n.parent].children.append(n.id);
    }
    m_nextId = labels.size() + 1;
    rebuild();
    m_savedBytes = documentBytes();
}
Engine::State Engine::state() const {
    return {m_nodes,  m_connections, m_layout, m_spacing, m_branchStyle, m_themeId,
            m_manual, m_selected,    m_nextId, m_selection};
}
void Engine::restore(const State &s) {
    ++m_documentRevision;
    m_nodes = s.nodes;
    m_connections = s.connections;
    m_layout = s.layout;
    m_spacing = s.spacing;
    m_branchStyle = s.branchStyle;
    m_themeId = s.themeId;
    m_manual = s.manual;
    m_selected = s.selected;
    m_nextId = s.nextId;
    m_selection = s.selection;
    m_error.clear();
    rebuild();
}
void Engine::checkpoint() {
    ++m_documentRevision;
    m_undo.append(state());
    m_redo.clear();
    m_error.clear();
    // Bounded copy-on-write snapshots: at most 40 commands and 200k retained node records.
    int retained = 0;
    for (const auto &s : m_undo)
        retained += s.nodes.size();
    while (m_undo.size() > 1 && (m_undo.size() > 40 || retained > 200000)) {
        retained -= m_undo.first().nodes.size();
        m_undo.removeFirst();
    }
}
bool Engine::fail(const QString &text) {
    m_error = text;
    emit changed();
    return false;
}
bool Engine::isDescendant(int node, int ancestor) const {
    if (!m_nodes.contains(node) || !m_nodes.contains(ancestor))
        return false;
    int p = m_nodes[node].parent;
    for (int i = 0; p != -1 && i <= m_nodes.size(); ++i) {
        if (p == ancestor)
            return true;
        p = m_nodes.value(p).parent;
    }
    return false;
}
QVariantList Engine::selection() const {
    QVariantList result;
    for (int id : m_visible)
        if (m_selection.contains(id))
            result.append(id);
    return result;
}
QVariantList Engine::outline() const {
    QVariantList result;
    result.reserve(m_visible.size());
    for (int id : m_visible) {
        const auto &n = m_nodes[id];
        result.append(QVariantMap{{"id", id},
                                  {"text", n.kind=="date" ? QString("Date · ")+Calendar::title(n.calendar) : m_textCache.value(id).plainText},
                                  {"depth", n.depth},
                                  {"folded", n.folded},
                                  {"hasChildren", !n.children.isEmpty()},
                                  {"task", n.task},
                                  {"checked", n.checked},
                                  {"selected", m_selection.contains(id)}});
    }
    return result;
}
bool Engine::measureText(const QString &text, bool task, TextMeasure &result, double fixedWidth) {
    QTextDocument doc;
    QFont font("sans-serif", 11);
    font.setPixelSize(15);
    doc.setDefaultFont(font);
    doc.setDocumentMargin(0);
    doc.setDefaultStyleSheet("body,p {color:#e9eff4; margin:0;} a {color:#8be4cf;}");
    doc.setHtml(text);
    // Some font backends clamp extreme requested sizes to small fallback glyphs.
    // Reject the parsed declaration as well as the resulting layout dimensions.
    for (auto block = doc.begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QFont runFont = it.fragment().charFormat().font();
            if (runFont.pixelSize() > 4096 || runFont.pointSizeF() > 3072)
                return false;
        }
    }
    doc.setTextWidth(-1);
    const double idealWidth = doc.idealWidth();
    if (!std::isfinite(idealWidth))
        return false;
    const double width = fixedWidth > 0 ? std::max(20., fixedWidth - 30 - (task ? 20 : 0)) : std::min(std::ceil(idealWidth), 260.0);
    doc.setTextWidth(width);
    const QSizeF measured = doc.size();
    const double height = std::max(22.0, measured.height()) + 20;
    if (!std::isfinite(measured.width()) || !std::isfinite(measured.height()) ||
        measured.height() < 0 || measured.width() < 0 || height > 4096)
        return false;
    result = {text, doc.toPlainText(), task, QSizeF(width + 30 + (task ? 20 : 0), height), fixedWidth};
    return true;
}
void Engine::rebuild() {
    QElapsedTimer timer;
    timer.start();
    // Include folded descendants; task completion is document state, not visibility.
    QVector<int> taskOrder{1};
    for(int i=0;i<taskOrder.size();++i)
        for(int child:m_nodes.value(taskOrder[i]).children) taskOrder.append(child);
    for(auto it=taskOrder.crbegin();it!=taskOrder.crend();++it) {
        auto &node=m_nodes[*it];
        node.taskChildren=0; node.completedTaskChildren=0;
        if(node.kind!="text" || !node.meeting.isEmpty()) continue;
        for(int child:node.children) if(m_nodes[child].task) {
            ++node.taskChildren;
            if(m_nodes[child].checked) ++node.completedTaskChildren;
        }
        if(node.taskChildren>0) {
            node.task=true;
            node.checked=node.completedTaskChildren==node.taskChildren;
        }
    }
    m_visible.clear();
    m_branchIndices.clear();
    const auto rootChildren = m_nodes.value(1).children;
    for (int i = 0; i < rootChildren.size(); ++i)
        m_branchIndices.insert(rootChildren[i], i);
    QVector<int> pending{1};
    while (!pending.isEmpty()) {
        int id = pending.takeLast();
        auto &n = m_nodes[id];
        n.depth = n.parent < 0 ? 0 : m_nodes[n.parent].depth + 1;
        if (n.depth > 1)
            m_branchIndices.insert(id, m_branchIndices.value(n.parent));
        m_visible.append(id);
        if (!n.folded)
            for (auto it = n.children.crbegin(); it != n.children.crend(); ++it)
                pending.append(*it);
    }
    QSet<int> visible(m_visible.begin(), m_visible.end());
    for (auto it = m_selection.begin(); it != m_selection.end();) {
        if (!visible.contains(*it))
            it = m_selection.erase(it);
        else
            ++it;
    }
    while (!visible.contains(m_selected) && m_nodes.contains(m_selected))
        m_selected = m_nodes[m_selected].parent;
    if (m_selected != -1 && !visible.contains(m_selected))
        m_selected = 1;
    if (m_selected != -1)
        m_selection.insert(m_selected);
    const double gap =
        m_layout == "Compact" ? 32 : (m_spacing == "Narrow" ? 18 : (m_spacing == "Wide" ? 52 : 32));
    bool vertical = m_layout == "Vertical";
    QHash<int, double> span;
    QVector<double> depthSize(MaxDepth + 1, 0), depthPos(MaxDepth + 1, 0);
    for (auto it = m_textCache.begin(); it != m_textCache.end();) {
        if (!m_nodes.contains(it.key()))
            it = m_textCache.erase(it);
        else
            ++it;
    }
    for (int id : m_visible) {
        auto &n = m_nodes[id];
        QSizeF size;
        auto cached = m_textCache.constFind(id);
        if (cached != m_textCache.cend() && cached->text == n.text && cached->task == n.task && cached->width == n.style.value("width").toDouble())
            size = cached->size;
        else {
            TextMeasure measurement;
            if (!measureText(n.text, n.task, measurement, n.style.value("width").toDouble())) {
                // All title entry points validate before mutation; retain a safe rectangle
                // if an unsupported font backend nevertheless produces invalid geometry.
                measurement = {n.text, QString(), n.task, QSizeF(102 + (n.task ? 20 : 0), 42)};
            }
            size = measurement.size;
            m_textCache.insert(id, measurement);
        }
        if(n.kind=="date") size=Calendar::size(n.calendar);
        n.rect = QRectF(QPointF(), size);
        depthSize[n.depth] = std::max(depthSize[n.depth], vertical ? size.height() : size.width());
    }
    for (int i = 1; i < depthPos.size(); ++i)
        depthPos[i] = depthPos[i - 1] + depthSize[i - 1] + gap * 2;
    if (m_layout == "Compact") {
        double y = 0;
        for (int id : m_visible) {
            auto &n = m_nodes[id];
            n.rect.moveTopLeft(QPointF(n.depth * (gap + 24), y));
            y += n.rect.height() + gap * .5;
        }
    } else {
        for (auto it = m_visible.crbegin(); it != m_visible.crend(); ++it) {
            const auto &n = m_nodes[*it];
            double children = 0;
            if (!n.folded)
                for (int child : n.children)
                    children += span[child] + gap;
            if (children)
                children -= gap;
            span[*it] = std::max(vertical ? n.rect.width() : n.rect.height(), children);
        }
        QHash<int, double> start;
        start[1] = 0;
        for (int id : m_visible) {
            auto &n = m_nodes[id];
            double cross =
                start[id] + (span[id] - (vertical ? n.rect.width() : n.rect.height())) * .5;
            // Each subtree owns a disjoint cross-axis band (span/start), so
            // shortening its primary-axis gap cannot overlap other branches.
            // Preserve the old base coordinates for persisted manual offsets.
            double along = depthPos[n.depth];
            if (!m_manual) {
                const auto parent = m_nodes.value(n.parent).rect;
                along = n.parent < 0 ? 0 : (vertical ? parent.bottom() : parent.right()) + gap * 2;
            }
            n.rect.moveTopLeft(vertical ? QPointF(cross, along)
                                        : QPointF(along, cross));
            double cursor = start[id];
            if (!n.folded)
                for (int child : n.children) {
                    start[child] = cursor;
                    cursor += span[child] + gap;
                }
        }
    }
    m_layoutRects.clear();
    for(int id:m_visible) m_layoutRects.insert(id,m_nodes[id].rect);
    const auto mirrored = m_manual && m_layout == "Horizontal" ? manualGeometry() : QHash<int,QRectF>();
    QHash<int, QPointF> accumulatedOffsets;
    m_bounds = QRectF();
    for (int id : m_visible) {
        auto &n = m_nodes[id];
        if (!mirrored.isEmpty()) {
            n.rect = mirrored.value(id);
        } else if (m_manual) {
            QPointF offset = n.manualOffset + accumulatedOffsets.value(n.parent);
            accumulatedOffsets.insert(id, offset);
            n.rect.translate(offset);
        }
        m_bounds = m_bounds.isNull() ? n.rect : m_bounds.united(n.rect);
    }
    m_layoutMs = timer.nsecsElapsed() / 1000000.0;
    emit outlineChanged();
    emit changed();
}
NodeAppearance Engine::appearance(int id) const {
    const auto it = m_nodes.constFind(id);
    if (it == m_nodes.cend())
        return Themes::appearance(m_themeId, 0, 0);
    auto a = Themes::appearance(m_themeId, it->depth, m_branchIndices.value(id));
    const auto &s = it->style;
    if (s.contains("shape")) a.shape = NodeShape(s["shape"].toInt());
    if (s.contains("fill")) a.fill = QColor(s["fill"].toString());
    if (s.contains("border")) a.border = QColor(s["border"].toString());
    if (s.contains("textColor")) a.text = QColor(s["textColor"].toString());
    if (s.contains("branch")) a.branch = QColor(s["branch"].toString());
    if (s.contains("borderWidth")) a.borderWidth = s["borderWidth"].toDouble();
    if (s.contains("branchWidth")) a.branchWidth = s["branchWidth"].toDouble();
    if (s.contains("borderStyle")) a.borderStyle = Qt::PenStyle(s["borderStyle"].toInt());
    if (s.contains("branchStroke")) a.branchStroke = Qt::PenStyle(s["branchStroke"].toInt());
    if(it->kind=="date" && !s.contains("shape") && (a.shape==NodeShape::Underline || a.shape==NodeShape::Embedded)) {
        a.shape=NodeShape::Rounded; a.border=a.branch; a.borderWidth=1; a.fill=canvasColor();
    }
    // Shape overrides and the Date fallback can inherit a zero radius from
    // an underline/rectangle theme. Rounded must still have curved corners.
    if (a.shape==NodeShape::Rounded && a.radius<=0)
        a.radius=NodeAppearance{}.radius;
    return a;
}
void Engine::setLayout(QString value) {
    if (!validLayout(value) || m_layout == value)
        return;
    checkpoint();
    m_layout = value;
    if (m_layout == "Compact")
        m_manual = false;
    rebuild();
}
void Engine::setSpacing(QString value) {
    if (m_manual || m_layout == "Compact" || !validSpacing(value) || m_spacing == value)
        return;
    checkpoint();
    m_spacing = value;
    rebuild();
}
void Engine::setBranchStyle(QString value) {
    if (!validBranch(value) || m_branchStyle == value)
        return;
    checkpoint();
    m_branchStyle = value;
    emit changed();
}
void Engine::setManual(bool value) {
    if (m_layout == "Compact" || m_manual == value)
        return;
    checkpoint();
    m_manual = value;
    rebuild();
}
void Engine::setThemeId(QString value) {
    if (value == m_themeId)
        return;
    if (!Themes::contains(value)) {
        fail("Unknown theme ID: " + value);
        return;
    }
    checkpoint();
    m_themeId = std::move(value);
    emit changed();
}
void Engine::select(int id, bool extend) {
    if (id == -1) {
        m_selection.clear();
        m_selected = -1;
        emit changed();
        return;
    }
    if (!m_visible.contains(id))
        return;
    if (!extend)
        m_selection.clear();
    m_selection.insert(id);
    m_selected = id;
    emit changed();
}
void Engine::add(int parent, int after, QString kind, QString dateView, std::optional<QPointF> position, bool emptyText) {
    if (position && (!std::isfinite(position->x()) || !std::isfinite(position->y()) ||
        std::abs(position->x()) > 1e5 || std::abs(position->y()) > 1e5)) return;
    if (!m_nodes.contains(parent))
        return;
    if (m_nodes.size() >= MaxNodes) {
        fail("The prototype supports at most 10,000 nodes.");
        return;
    }
    int depth = 0, p = parent;
    while (p != -1) {
        ++depth;
        p = m_nodes[p].parent;
    }
    if (depth > MaxDepth) {
        fail("Maximum tree depth reached.");
        return;
    }
    checkpoint();
    MapNode n;
    n.id = m_nextId++;
    n.parent = parent;
    n.kind=kind;
    n.task=kind=="text" && m_nodes.value(parent).task;
    n.text = kind=="date" ? "Date" : emptyText || !m_nodes.value(parent).meetingSection.isEmpty() ? "" : "New idea";
    if(kind=="date") { n.calendar.view=dateView; n.calendar.anchor=QDate::currentDate(); }
    m_nodes.insert(n.id, n);
    auto &children = m_nodes[parent].children;
    int index = children.indexOf(after);
    if (index >= 0)
        children.insert(index + 1, n.id);
    else
        children.append(n.id);
    m_nodes[parent].folded = false;
    m_selected = n.id;
    m_selection = {n.id};
    rebuild();
    if (position && m_manual) {
        QPointF delta = *position - m_nodes[n.id].rect.center();
        if (m_layout=="Horizontal" && parent>1 &&
            m_nodes[parent].rect.center().x()<m_nodes[1].rect.center().x()) delta.setX(-delta.x());
        m_nodes[n.id].manualOffset += delta;
        rebuild();
    }
    if(kind=="text") emit editRequested(n.id);
}
void Engine::addChild() { add(m_selected); }
void Engine::addChildFromPointer(int parent, std::optional<QPointF> position) {
    add(parent, -1, "text", "week", position, true);
}
void Engine::addSibling() {
    if (!m_nodes.contains(m_selected))
        return;
    int parent = m_nodes.value(m_selected).parent;
    add(parent < 0 ? 1 : parent, m_selected);
}
void Engine::removeSelected() {
    QSet<int> removing = m_selection;
    removing.remove(1);
    if (removing.isEmpty())
        return;
    checkpoint();
    int fallback = m_nodes.value(m_selected).parent;
    QVector<int> pending(removing.begin(), removing.end());
    while (!pending.isEmpty()) {
        int id = pending.takeLast();
        for (int child : m_nodes[id].children)
            if (!removing.contains(child)) {
                removing.insert(child);
                pending.append(child);
            }
    }
    while (removing.contains(fallback))
        fallback = m_nodes[fallback].parent;
    for (int id : removing) {
        int p = m_nodes[id].parent;
        if (!removing.contains(p))
            m_nodes[p].children.removeAll(id);
    }
    for (int id : removing)
        m_nodes.remove(id);
    m_connections.removeIf([&](const auto &edge) {
        return removing.contains(edge.first) || removing.contains(edge.second);
    });
    m_selected = m_nodes.contains(fallback) ? fallback : 1;
    m_selection = {m_selected};
    rebuild();
}
void Engine::toggleFold() {
    if (m_nodes.value(m_selected).children.isEmpty())
        return;
    checkpoint();
    m_nodes[m_selected].folded = !m_nodes[m_selected].folded;
    rebuild();
}
void Engine::toggleTask() {
    if (!m_nodes.contains(m_selected) || m_nodes.value(m_selected).kind!="text")
        return;
    setNodeKind(m_selected,m_nodes.value(m_selected).task ? "text" : "task");
}
void Engine::toggleChecked() {
    if (!m_nodes.contains(m_selected) || m_nodes.value(m_selected).kind!="text" ||
        m_nodes.value(m_selected).taskChildren>0)
        return;
    checkpoint();
    auto &n = m_nodes[m_selected];
    n.task = true;
    n.checked = !n.checked;
    rebuild();
}
QSizeF Engine::previewTextSize(int id, const QString &text) const {
    if (!m_nodes.contains(id) || text.size() > MaxText) return {};
    TextMeasure measurement;
    if (!measureText(text, m_nodes.value(id).task, measurement, m_nodes.value(id).style.value("width").toDouble())) return {};
    return measurement.size;
}
bool Engine::setText(int id, QString text) {
    if (!m_nodes.contains(id) || m_nodes.value(id).kind!="text")
        return false;
    if (m_nodes[id].text == text)
        return true;
    if (text.size() > MaxText) {
        fail("Node text exceeds 16,384 characters.");
        return false;
    }
    TextMeasure measurement;
    if (!measureText(text, m_nodes[id].task, measurement, m_nodes[id].style.value("width").toDouble()))
        return fail("Node text is too tall (maximum measured height is 4,096 pixels).");
    checkpoint();
    m_textCache.insert(id, measurement);
    m_nodes[id].text = text;
    rebuild();
    return true;
}
void Engine::setNotes(QString text) {
    if (!m_nodes.contains(m_selected))
        return;
    if (m_nodes[m_selected].notes == text)
        return;
    if (text.size() > MaxText) {
        fail("Notes exceed 16,384 characters.");
        return;
    }
    checkpoint();
    m_nodes[m_selected].notes = text;
    emit changed();
}
void Engine::navigate(QString direction, bool extend) {
    if (!m_nodes.contains(m_selected)) {
        select(1, extend);
        return;
    }
    direction = direction.toLower();
    const auto current = m_nodes.value(m_selected);
    int target = -1;
    QString forward = m_layout == "Vertical" ? "down" : "right",
            backward = m_layout == "Vertical" ? "up" : "left";
    if (direction == backward)
        target = current.parent;
    else if (direction == forward && !current.children.isEmpty()) {
        if (current.folded)
            return;
        target = current.children.first();
    } else if (m_layout == "Compact" && (direction == "up" || direction == "down")) {
        int i = m_visible.indexOf(m_selected) + (direction == "up" ? -1 : 1);
        if (i >= 0 && i < m_visible.size())
            target = m_visible[i];
    } else if (direction == "up" || direction == "down" || direction == "left" ||
               direction == "right") {
        QPointF origin = current.rect.center();
        double best = std::numeric_limits<double>::max();
        for (int id : m_visible) {
            if (id == m_selected)
                continue;
            QPointF d = m_nodes[id].rect.center() - origin;
            double along = direction == "right"  ? d.x()
                           : direction == "left" ? -d.x()
                           : direction == "down" ? d.y()
                                                 : -d.y();
            double across =
                (direction == "left" || direction == "right") ? std::abs(d.y()) : std::abs(d.x());
            if (along > 1 && across < along * 2 + 20) {
                double score = along + across * 3;
                if (score < best) {
                    best = score;
                    target = id;
                }
            }
        }
    }
    if (target >= 0)
        select(target, extend);
}
void Engine::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append(state());
    State s = m_undo.takeLast();
    restore(s);
}
void Engine::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append(state());
    State s = m_redo.takeLast();
    restore(s);
}
void Engine::loadFixture(int count) {
    if (count < 1 || count > MaxNodes) {
        fail("Fixture size must be between 1 and 10,000.");
        return;
    }
    checkpoint();
    m_nodes.clear();
    m_connections.clear();
    m_textCache.clear();
    for (int id = 1; id <= count; ++id) {
        MapNode n;
        n.id = id;
        n.parent = id == 1 ? -1 : (id - 2) / 4 + 1;
        n.text = id == 1 ? "Mindarchy" : QString("Idea %1").arg(id);
        m_nodes.insert(id, n);
        if (n.parent != -1)
            m_nodes[n.parent].children.append(id);
    }
    m_nextId = count + 1;
    m_selected = 1;
    m_selection = {1};
    rebuild();
}
void Engine::moveNode(int id, int parent, int beforeId) {
    if (!m_nodes.contains(id) || !m_nodes.contains(parent) || id == 1 || id == parent ||
        isDescendant(parent, id)) {
        fail("A node cannot be moved into itself or a descendant.");
        return;
    }
    if (beforeId == id)
        return;
    if (beforeId != -1 && !m_nodes[parent].children.contains(beforeId)) {
        fail("Reorder target must belong to the destination parent.");
        return;
    }
    int depth = 0, p = parent;
    while (p != -1) {
        ++depth;
        p = m_nodes[p].parent;
    }
    QVector<QPair<int, int>> pending{{id, depth}};
    while (!pending.isEmpty()) {
        auto entry = pending.takeLast();
        if (entry.second > MaxDepth) {
            fail("Move would exceed maximum tree depth.");
            return;
        }
        for (int c : m_nodes[entry.first].children)
            pending.append({c, entry.second + 1});
    }
    checkpoint();
    m_nodes[m_nodes[id].parent].children.removeAll(id);
    auto &children = m_nodes[parent].children;
    int index = children.indexOf(beforeId);
    if (index < 0)
        children.append(id);
    else
        children.insert(index, id);
    m_nodes[id].parent = parent;
    m_nodes[parent].folded = false;
    rebuild();
}
void Engine::moveManual(int id, double dx, double dy) {
    // Offsets belong to the parent's local growth direction. Pointer movement
    // stays in world coordinates, including inside an already mirrored branch.
    if(m_layout=="Horizontal" && m_nodes.contains(id)) {
        const int parent=m_nodes.value(id).parent;
        if(parent>1 && m_nodes.value(parent).rect.center().x()<m_nodes.value(1).rect.center().x()) dx=-dx;
    }
    if (!m_manual || !m_nodes.contains(id) || !std::isfinite(dx) || !std::isfinite(dy) ||
        std::abs(dx) > 1e6 || std::abs(dy) > 1e6)
        return;
    QPointF next = m_nodes[id].manualOffset + QPointF(dx, dy);
    if (std::abs(next.x()) > 1e6 || std::abs(next.y()) > 1e6)
        return;
    checkpoint();
    m_nodes[id].manualOffset = next;
    rebuild();
}
QByteArray Engine::documentBytes() const {
    QJsonArray nodes;
    QList<int> ids = m_nodes.keys();
    std::sort(ids.begin(), ids.end());
    for (int id : ids) {
        const auto &n = m_nodes[id];
        QJsonArray children;
        for (int child : n.children)
            children.append(child);
        QJsonObject calendar;
        if(n.calendar.anchor.isValid()) {
            QJsonObject entries; for(auto it=n.calendar.entries.begin();it!=n.calendar.entries.end();++it) entries[it.key()]=it.value();
            calendar={{"view",n.calendar.view},{"anchor",n.calendar.anchor.toString(Qt::ISODate)},{"entries",entries}};
        }
        nodes.append(QJsonObject{{"kind",n.kind},{"calendar",calendar},{"id", id},
                                 {"meeting",QJsonObject::fromVariantMap(n.meeting)},
                                 {"meetingSection",n.meetingSection},
                                 {"parent", n.parent},
                                 {"children", children},
                                 {"text", n.text},
                                 {"notes", n.notes},
                                 {"style", QJsonObject::fromVariantMap(n.style)},
                                 {"folded", n.folded},
                                 {"task", n.task},
                                 {"checked", n.checked},
                                 {"x", n.manualOffset.x()},
                                 {"y", n.manualOffset.y()}});
    }
    QJsonArray connections;
    for (const auto &edge : m_connections)
        connections.append(QJsonArray{edge.first, edge.second});
    QJsonObject obj{
        {"connections", connections}, {"format", "mindmap-lab"}, {"version", 1},
        {"layout", m_layout},         {"spacing", m_spacing},    {"branchStyle", m_branchStyle},
        {"themeId", m_themeId},       {"manual", m_manual},      {"nodes", nodes}};
    return QJsonDocument(obj).toJson();
}
bool Engine::hasUnsavedChanges() const {
    return edited();
}
QString Engine::documentName() const {
    return m_documentPath.isEmpty() ? QStringLiteral("New mindmap") : QFileInfo(m_documentPath).completeBaseName();
}
bool Engine::edited() const {
    // Selection/hover notifications do not serialize the map again.
    if (m_checkedRevision != m_documentRevision) {
        m_edited = documentBytes() != m_savedBytes;
        m_checkedRevision = m_documentRevision;
    }
    return m_edited;
}
bool Engine::save(QString path) {
    QSaveFile file(localPath(path));
    if (!file.open(QIODevice::WriteOnly))
        return fail(file.errorString());
    QByteArray bytes = documentBytes();
    if (file.write(bytes) != bytes.size() || !file.commit())
        return fail(file.errorString());
    m_savedBytes = bytes;
    m_checkedRevision = ~quint64(0);
    m_documentPath = localPath(path);
    m_error.clear();
    emit changed();
    emit documentSaved();
    return true;
}
bool Engine::saveRecovery(const QString &path, const QVariantMap &ui) {
    const QJsonObject snapshot{{"format","mindarchy-recovery"},{"version",1},
        {"originalPath",m_documentPath},{"document",QJsonDocument::fromJson(documentBytes()).object()},
        {"baseline",QString::fromLatin1(m_savedBytes.toBase64())},{"ui",QJsonObject::fromVariantMap(ui)}};
    const auto bytes=QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    // Avoid needless writes while idle. QSaveFile preserves the previous good
    // snapshot if opening, writing, or atomic replacement fails.
    QFile previous(path);
    if(previous.open(QIODevice::ReadOnly) && previous.size()==bytes.size() && previous.readAll()==bytes) return true;
    previous.close();
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)) return fail("Could not save recovery: "+file.errorString());
    file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    if(file.write(bytes)!=bytes.size() || !file.commit()) return fail("Could not save recovery: "+file.errorString());
    return true;
}
bool Engine::openRecovery(const QString &path, QVariantMap *ui) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly) || file.size()>64*1024*1024) return fail("Cannot read recovery snapshot.");
    const auto envelope=QJsonDocument::fromJson(file.readAll()).object();
    if(envelope["format"]!="mindarchy-recovery" || envelope["version"].toInt()!=1 ||
       !envelope["document"].isObject() || !envelope["originalPath"].isString() || !envelope["baseline"].isString())
        return fail("Invalid recovery snapshot.");
    const auto baseline=QByteArray::fromBase64(envelope["baseline"].toString().toLatin1());
    if(!QJsonDocument::fromJson(baseline).isObject()) return fail("Invalid recovery baseline.");
    if(!loadDocumentBytes(QJsonDocument(envelope["document"].toObject()).toJson(),envelope["originalPath"].toString())) return false;
    m_savedBytes=baseline; m_checkedRevision=~quint64(0);
    if(ui) *ui=envelope["ui"].toObject().toVariantMap();
    emit changed();
    return true;
}
bool Engine::open(QString path) {
    QFile file(localPath(path));
    if (!file.open(QIODevice::ReadOnly))
        return fail(file.errorString());
    if (file.size() > 20 * 1024 * 1024)
        return fail("Document exceeds the 20 MB prototype limit.");
    return loadDocumentBytes(file.readAll(), localPath(path));
}
bool Engine::loadDocumentBytes(const QByteArray &bytes, const QString &path) {
    QJsonParseError parse;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject())
        return fail("Invalid JSON document.");
    QJsonObject obj = doc.object();
    if (obj["format"] != "mindmap-lab" || obj["version"].toInt(-1) != 1 || !obj["nodes"].isArray())
        return fail("Unsupported document format or version.");
    QString layout = obj["layout"].toString(), spacing = obj["spacing"].toString(),
            branch = obj["branchStyle"].toString();
    const QString themeId = obj.contains("themeId") ? obj["themeId"].toString() : QString("lab");
    if (!validLayout(layout) || !validSpacing(spacing) || !validBranch(branch) ||
        !obj["manual"].isBool() || !Themes::contains(themeId))
        return fail("Invalid document settings.");
    if (layout == "Compact" && obj["manual"].toBool())
        return fail("Compact layout does not support manual positioning.");
    QJsonArray array = obj["nodes"].toArray();
    if (array.isEmpty() || array.size() > MaxNodes)
        return fail("Document must contain 1 to 10,000 nodes.");
    QHash<int, MapNode> candidate;
    QHash<int, TextMeasure> candidateMeasurements;
    int nextId = 1;
    auto integer = [](const QJsonValue &v, int min, int max) {
        double d = v.toDouble(std::numeric_limits<double>::quiet_NaN());
        return v.isDouble() && std::isfinite(d) && d >= min && d <= max && std::floor(d) == d;
    };
    for (const auto &value : array) {
        if (!value.isObject())
            return fail("Invalid node record.");
        QJsonObject o = value.toObject();
        if (!integer(o["id"], 1, 1000000000) || !integer(o["parent"], -1, 1000000000) ||
            !o["children"].isArray() || !o["text"].isString() || !o["notes"].isString() ||
            !o["folded"].isBool() || !o["task"].isBool() || !o["checked"].isBool() ||
            !o["x"].isDouble() || !o["y"].isDouble())
            return fail("Invalid node field types.");
        MapNode n;
        n.id = o["id"].toInt();
        n.parent = o["parent"].toInt();
        n.kind=o.contains("kind") ? o["kind"].toString() : QString("text");
        if(n.kind!="text" && n.kind!="date") return fail("Unsupported node kind.");
        if(n.kind=="date" || !o["calendar"].toObject().isEmpty()) {
            if(!o["calendar"].isObject()) return fail("Missing calendar data.");
            const auto c=o["calendar"].toObject();
            n.calendar.view=c["view"].toString(); n.calendar.anchor=QDate::fromString(c["anchor"].toString(),Qt::ISODate);
            if((n.calendar.view!="week" && n.calendar.view!="month") || !n.calendar.anchor.isValid() ||
                n.calendar.anchor.year()<1 || n.calendar.anchor.year()>9999 || !c["entries"].isObject()) return fail("Invalid calendar settings.");
            const auto entries=c["entries"].toObject();
            if(entries.size()>3660) return fail("Calendar entry limit exceeded.");
            for(auto it=entries.begin();it!=entries.end();++it) {
                const auto day=QDate::fromString(it.key(),Qt::ISODate);
                if(!day.isValid() || day.year()<1 || day.year()>9999 || day.toString(Qt::ISODate)!=it.key() || !it.value().isString() ||
                   it.value().toString().trimmed().isEmpty() || it.value().toString().size()>4096) return fail("Invalid calendar entry.");
                n.calendar.entries.insert(it.key(),it.value().toString());
            }
        }
        if(o.contains("meeting") && !o["meeting"].isObject()) return fail("Invalid meeting metadata.");
        n.meeting=o["meeting"].toObject().toVariantMap();
        if(!n.meeting.isEmpty()) {
            const auto date=n.meeting.value("date").toString();
            const auto time=n.meeting.value("time").toString();
            if(n.kind!="text" || !QDate::fromString(date,Qt::ISODate).isValid() ||
               (!time.isEmpty() && !QTime::fromString(time,"HH:mm").isValid()) ||
               n.meeting.value("attendees").toString().size()>4096)
                return fail("Invalid meeting details.");
        }
        n.meetingSection=o["meetingSection"].toString();
        if(!QStringList{"","agenda","notes","decisions","actions"}.contains(n.meetingSection))
            return fail("Invalid meeting section.");
        n.text = o["text"].toString();
        n.notes = o["notes"].toString();
        if (o.contains("style") && !o["style"].isObject()) return fail("Invalid node style.");
        n.style = o["style"].toObject().toVariantMap();
        if (!validNodeStyle(n.style)) return fail("Invalid node style.");
        n.folded = o["folded"].toBool();
        n.task = o["task"].toBool();
        n.checked = o["checked"].toBool();
        n.manualOffset = {o["x"].toDouble(), o["y"].toDouble()};
        if (candidate.contains(n.id) || n.text.size() > MaxText || n.notes.size() > MaxText ||
            (n.checked && !n.task) || (n.kind=="date" && (n.task || n.checked)) || std::abs(n.manualOffset.x()) > 1e6 ||
            std::abs(n.manualOffset.y()) > 1e6)
            return fail("Duplicate ID or invalid node content.");
        QSet<int> unique;
        for (const auto &c : o["children"].toArray()) {
            if (!integer(c, 1, 1000000000) || unique.contains(c.toInt()))
                return fail("Invalid or duplicate child ID.");
            unique.insert(c.toInt());
            n.children.append(c.toInt());
        }
        TextMeasure measurement;
        if (!measureText(n.text, n.task, measurement, n.style.value("width").toDouble()))
            return fail("Node text is too tall (maximum measured height is 4,096 pixels).");
        candidateMeasurements.insert(n.id, measurement);
        candidate.insert(n.id, n);
        nextId = std::max(nextId, n.id + 1);
    }
    if (!candidate.contains(1) || candidate[1].parent != -1)
        return fail("Root node 1 is missing or invalid.");
    QSet<int> visited;
    QVector<QPair<int, int>> pending{{1, 0}};
    while (!pending.isEmpty()) {
        auto entry = pending.takeLast();
        int id = entry.first;
        if (visited.contains(id) || !candidate.contains(id) || entry.second > MaxDepth)
            return fail("Document contains a cycle, missing node, or excessive depth.");
        visited.insert(id);
        for (int c : candidate[id].children) {
            if (!candidate.contains(c) || candidate[c].parent != id)
                return fail("Parent and child links disagree.");
            pending.append({c, entry.second + 1});
        }
    }
    if (visited.size() != candidate.size())
        return fail("Document contains disconnected nodes.");
    QVector<QPair<int, int>> connections;
    QSet<QString> edgeKeys;
    if (obj.contains("connections") && !obj["connections"].isArray())
        return fail("Invalid relationships.");
    QJsonArray edges = obj["connections"].toArray();
    if (edges.size() > MaxNodes)
        return fail("Too many relationships.");
    for (const auto &value : edges) {
        QJsonArray edge = value.toArray();
        if (!value.isArray() || edge.size() != 2 || !integer(edge[0], 1, 1000000000) ||
            !integer(edge[1], 1, 1000000000))
            return fail("Invalid relationship endpoints.");
        int a = edge[0].toInt(), b = edge[1].toInt();
        if (a > b)
            std::swap(a, b);
        QString key = QString::number(a) + ":" + QString::number(b);
        if (a == b || !candidate.contains(a) || !candidate.contains(b) || edgeKeys.contains(key))
            return fail("Missing or duplicate relationship endpoint.");
        edgeKeys.insert(key);
        connections.append({a, b});
    }
    checkpoint();
    m_textCache = std::move(candidateMeasurements);
    m_connections = connections;
    m_nodes = std::move(candidate);
    m_nextId = nextId;
    m_layout = layout;
    m_spacing = spacing;
    m_branchStyle = branch;
    m_themeId = themeId;
    m_manual = obj["manual"].toBool();
    m_selected = 1;
    m_selection = {1};
    rebuild();
    m_savedBytes = documentBytes();
    m_documentPath = localPath(path);
    m_checkedRevision = ~quint64(0);
    emit changed();
    return true;
}

void Engine::connectSelection() {
    if (m_selection.size() != 2) {
        fail("Select exactly two nodes to add a relationship.");
        return;
    }
    QList<int> ids = m_selection.values();
    std::sort(ids.begin(), ids.end());
    QPair<int, int> edge{ids[0], ids[1]};
    if (m_connections.contains(edge))
        return;
    if (m_connections.size() >= MaxNodes) {
        fail("Maximum relationship count reached.");
        return;
    }
    checkpoint();
    m_connections.append(edge);
    emit changed();
}

void Engine::selectMany(QVariantList ids, bool extend) {
    if (!extend) {
        m_selection.clear();
        m_selected = -1;
    }
    QSet<int> visible(m_visible.begin(), m_visible.end());
    for (const auto &value : ids) {
        bool ok = false;
        int id = value.toInt(&ok);
        if (ok && visible.contains(id)) {
            m_selection.insert(id);
            m_selected = id;
        }
    }
    emit changed();
}

QStringList Engine::fontFamilies() const { return QFontDatabase::families(); }
QVariantMap Engine::selectedStyle() const {
    auto values = [this](int id) {
        const auto n=m_nodes.value(id); const auto a=appearance(id);
        QTextDocument doc; QFont font("sans-serif"); font.setPixelSize(15);
        doc.setDefaultFont(font); doc.setHtml(n.text);
        QTextCursor cursor(&doc); cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        const auto f=cursor.charFormat().font().resolve(font);
        const auto alignment=cursor.blockFormat().alignment();
        return QVariantMap{{"shape",int(a.shape)}, {"fill",a.fill.name(QColor::HexArgb)},
            {"border",a.border.name(QColor::HexArgb)}, {"textColor",a.text.name(QColor::HexArgb)},
            {"branch",a.branch.name(QColor::HexArgb)}, {"borderWidth",a.borderWidth},
            {"branchWidth",a.branchWidth}, {"themeBranchWidth",!n.style.contains("branchWidth")}, {"borderStyle",int(a.borderStyle)},
            {"branchStroke",int(a.branchStroke)}, {"width",n.style.value("width",0)},
            {"fontFamily",f.family()}, {"fontSize",f.pixelSize()>0 ? double(f.pixelSize()) : f.pointSizeF()*96./72.},
            {"bold",f.bold()}, {"italic",f.italic()}, {"underline",f.underline()}, {"strike",f.strikeOut()},
            {"alignment",(alignment & Qt::AlignHCenter) ? 1 : (alignment & Qt::AlignRight) ? 2 : (alignment & Qt::AlignJustify) ? 3 : 0}};
    };
    QVariantMap result=values(m_selected); QStringList mixed;
    for (int id:m_selection) {
        const auto other=values(id);
        for(auto it=result.begin();it!=result.end();++it)
            if (it.value()!=other.value(it.key()) && !mixed.contains(it.key())) mixed.append(it.key());
    }
    result.insert("actualWidth",m_nodes.value(m_selected).rect.width());
    result.insert("mixed",mixed); result.insert("count",m_selection.size());
    return result;
}
bool Engine::applyNodeStyle(QVariantMap patch) {
    if (m_selection.isEmpty() || patch.isEmpty()) return true;
    const QStringList typography{"fontFamily","fontSize","bold","italic","underline","strike","alignment"};
    QVariantMap visual=patch;
    for(const auto &key:typography) visual.remove(key);
    if(!validNodeStyle(visual)) return fail("Invalid node style value.");
    for(auto it=patch.begin();it!=patch.end();++it) {
        const auto k=it.key(); const auto v=it.value();
        if(k=="fontFamily" && (v.metaType().id()!=QMetaType::QString || v.toString().isEmpty() || v.toString().size()>200)) return fail("Invalid font family.");
        if(k=="fontSize" && (!v.canConvert<double>() || !std::isfinite(v.toDouble()) || v.toDouble()<8 || v.toDouble()>144)) return fail("Font size must be 8–144 pixels.");
        if(k=="alignment" && (v.toInt()<0 || v.toInt()>3 || v.toDouble()!=v.toInt())) return fail("Invalid text alignment.");
        if(QStringList{"bold","italic","underline","strike"}.contains(k) && v.metaType().id()!=QMetaType::Bool) return fail("Invalid font option.");
    }
    auto next=m_nodes;
    for(int id:m_selection) {
        auto &n=next[id];
        for(auto it=visual.begin();it!=visual.end();++it) n.style.insert(it.key(),it.value());
        QTextDocument doc; QFont base("sans-serif"); base.setPixelSize(15); doc.setDefaultFont(base);
        doc.setDocumentMargin(0); doc.setHtml(n.text);
        if(patch.contains("fontSize")) {
            // Imported point sizes take precedence over pixel sizes during HTML
            // serialization. Remove that competing property on each text run.
            QVector<QPair<QTextCursor,QTextCharFormat>> runs;
            for(auto block=doc.begin();block.isValid();block=block.next())
                for(auto it=block.begin();!it.atEnd();++it) {
                    const auto fragment=it.fragment(); auto f=fragment.charFormat();
                    f.clearProperty(QTextFormat::FontPointSize);
                    f.clearProperty(QTextFormat::FontSizeAdjustment);
                    f.setProperty(QTextFormat::FontPixelSize,patch["fontSize"].toInt());
                    QTextCursor run(&doc); run.setPosition(fragment.position());
                    run.setPosition(fragment.position()+fragment.length(),QTextCursor::KeepAnchor);
                    runs.append({run,f});
                }
            for(auto &run:runs) run.first.setCharFormat(run.second);
        }
        QTextCursor cursor(&doc); cursor.select(QTextCursor::Document); QTextCharFormat format;
        if(patch.contains("fontFamily")) format.setFontFamilies({patch["fontFamily"].toString()});
        if(patch.contains("fontSize")) format.setProperty(QTextFormat::FontPixelSize,patch["fontSize"].toInt());
        if(patch.contains("bold")) format.setFontWeight(patch["bold"].toBool()?QFont::Bold:QFont::Normal);
        if(patch.contains("italic")) format.setFontItalic(patch["italic"].toBool());
        if(patch.contains("underline")) format.setFontUnderline(patch["underline"].toBool());
        if(patch.contains("strike")) format.setFontStrikeOut(patch["strike"].toBool());
        // Explicit text colors replace pre-existing rich-text run colors too.
        if(patch.contains("textColor")) format.setForeground(QColor(patch["textColor"].toString()));
        cursor.mergeCharFormat(format);
        if(patch.contains("alignment")) {
            QTextBlockFormat block; const Qt::Alignment alignments[]={Qt::AlignLeft,Qt::AlignHCenter,Qt::AlignRight,Qt::AlignJustify};
            block.setAlignment(alignments[patch["alignment"].toInt()]); cursor.mergeBlockFormat(block);
        }
        bool hasTypography=patch.contains("textColor"); for(const auto &key:typography) hasTypography |= patch.contains(key);
        if(hasTypography) n.text=doc.toHtml();
        TextMeasure measurement;
        if(n.text.size()>MaxText || !measureText(n.text,n.task,measurement,n.style.value("width").toDouble()))
            return fail("This style makes the title too large.");
    }
    bool different=false; for(int id:m_selection) different |= next[id].style!=m_nodes[id].style || next[id].text!=m_nodes[id].text;
    if(!different) return true;
    checkpoint(); m_nodes=next; rebuild(); return true;
}
void Engine::resetNodeStyle() {
    if(m_selection.isEmpty()) return;
    auto next=m_nodes;
    for(int id:m_selection) {
        auto &n=next[id]; n.style.clear();
        QTextDocument doc; doc.setHtml(n.text);
        n.text=doc.toPlainText().toHtmlEscaped().replace("\n","<br>");
        TextMeasure measurement;
        if(n.text.size()>MaxText || !measureText(n.text,n.task,measurement)) {
            fail("Reset would exceed the title size limit."); return;
        }
    }
    checkpoint(); m_nodes=next; rebuild();
}

void Engine::resetBranchWidth() {
    bool changed=false; for(int id:m_selection) changed |= m_nodes[id].style.contains("branchWidth");
    if(!changed) return;
    checkpoint(); for(int id:m_selection) m_nodes[id].style.remove("branchWidth"); rebuild();
}

QHash<int,QRectF> Engine::manualGeometry(int movingId, QPointF delta) const {
    QHash<int,QRectF> result;
    result.reserve(m_visible.size());
    const int movingParent=m_nodes.value(movingId).parent;
    if(movingId>=0 && movingParent>1 &&
       m_nodes.value(movingParent).rect.center().x()<m_nodes.value(1).rect.center().x())
        delta.setX(-delta.x());
    if(movingId>=0) {
        const auto next=m_nodes.value(movingId).manualOffset+delta;
        if(!std::isfinite(delta.x()) || !std::isfinite(delta.y()) ||
           std::abs(delta.x())>1e6 || std::abs(delta.y())>1e6 ||
           std::abs(next.x())>1e6 || std::abs(next.y())>1e6) return manualGeometry();
    }
    for(int id:m_visible) {
        const auto &n=m_nodes[id];
        QRectF rect=m_layoutRects.value(id);
        QPointF offset=n.manualOffset+(id==movingId ? delta : QPointF());
        if(n.parent<0) rect.translate(offset);
        else {
            const auto parent=result.value(n.parent);
            const qreal direction=n.parent!=1 && parent.center().x()<result.value(1).center().x() ? -1. : 1.;
            QPointF relative=rect.center()-m_layoutRects.value(n.parent).center()+offset;
            relative.setX(relative.x()*direction);
            rect.moveCenter(parent.center()+relative);
        }
        result.insert(id,rect);
    }
    return result;
}

bool Engine::applyThemeRecipe(QString id) {
    const auto recipe=Themes::layoutRecipe(id);
    if(recipe.isEmpty()) return fail("This theme has no layout recipe.");
    const QString layout=recipe["layout"].toString(), spacing=recipe["spacing"].toString(),
                  branch=recipe["branchStyle"].toString();
    if(!Themes::contains(id) || !validLayout(layout) || !validSpacing(spacing) || !validBranch(branch))
        return fail("Invalid theme layout recipe.");
    if(m_themeId==id && m_layout==layout && m_spacing==spacing && m_branchStyle==branch && !m_manual)
        return true;
    checkpoint();
    m_themeId=id; m_layout=layout; m_spacing=spacing; m_branchStyle=branch; m_manual=false;
    rebuild();
    return true;
}

QVariantMap Engine::selectedCalendar() const {
    const auto n=m_nodes.value(m_selected); QVariantList days;
    if(n.kind=="date") for(const auto &date:Calendar::days(n.calendar)) if(date.isValid())
        days.append(QVariantMap{{"date",date.toString(Qt::ISODate)},
            {"label",date.toString("ddd d MMM yyyy")+(n.calendar.entries.contains(date.toString(Qt::ISODate)) ? " · assigned" : "")}});
    return {{"view",n.calendar.view},{"anchor",n.calendar.anchor.toString(Qt::ISODate)},
            {"title",n.kind=="date" ? Calendar::title(n.calendar) : QString()}, {"days",days}};
}
bool Engine::setNodeKind(int id, QString kind) {
    if(!m_nodes.contains(id) || (kind!="text" && kind!="task" && kind!="date")) return fail("Choose Text, Task or Date.");
    const QString storageKind=kind=="task" ? "text" : kind;
    const bool task=kind=="task";
    if(m_nodes.value(id).kind==storageKind && m_nodes.value(id).task==task) {
        bool needsCascade=false;
        if(task) {
            QVector<int> pending=m_nodes[id].children;
            while(!pending.isEmpty()) {
                const auto &child=m_nodes[pending.takeLast()];
                if(!child.task) { needsCascade=true; break; }
                pending+=child.children;
            }
        }
        if(!needsCascade) return true;
    }
    checkpoint();
    const bool cascade=task || (kind=="text" && m_nodes[id].task);
    if(cascade) {
        QVector<int> pending=m_nodes[id].children;
        while(!pending.isEmpty()) {
            const int child=pending.takeLast(); auto &descendant=m_nodes[child];
            pending+=descendant.children;
            if(task) descendant.kind="text";
            descendant.task=task;
            if(!task) descendant.checked=false;
        }
    }
    auto &node=m_nodes[id]; node.kind=storageKind; node.task=task;
    if(kind!="text") node.meeting.clear();
    if(!task) node.checked=false;
    if(kind=="date" && !node.calendar.anchor.isValid()) node.calendar.anchor=QDate::currentDate();
    rebuild(); return true;
}
void Engine::addDateNode(QString view) {
    if(view!="week" && view!="month") { fail("Choose week or month."); return; }
    add(m_nodes.contains(m_selected) ? m_selected : 1,-1,"date",view);
}
bool Engine::configureDateNode(int id, QString view, QString anchor) {
    const auto date=QDate::fromString(anchor,Qt::ISODate);
    if(!m_nodes.contains(id) || m_nodes[id].kind!="date" || (view!="week" && view!="month") ||
       !date.isValid() || date.toString(Qt::ISODate)!=anchor || date.year()<1 || date.year()>9999)
        return fail("Choose a valid calendar date (YYYY-MM-DD) and week or month.");
    auto &c=m_nodes[id].calendar;
    if(c.view==view && c.anchor==date) return true;
    checkpoint(); m_nodes[id].calendar.view=view; m_nodes[id].calendar.anchor=date; rebuild(); return true;
}
bool Engine::shiftDateNode(int id, int direction) {
    if(!m_nodes.contains(id) || m_nodes[id].kind!="date" || (direction!=-1 && direction!=1)) return false;
    const auto c=m_nodes[id].calendar;
    const auto date=c.view=="month" ? Calendar::start(c).addMonths(direction) : c.anchor.addDays(direction*7);
    return configureDateNode(id,c.view,date.toString(Qt::ISODate));
}
QString Engine::dateEntry(int id,QString date) const { return m_nodes.value(id).calendar.entries.value(date); }
bool Engine::setDateEntry(int id,QString date,QString text) {
    const auto day=QDate::fromString(date,Qt::ISODate);
    if(!m_nodes.contains(id) || m_nodes[id].kind!="date" || !day.isValid() || day.toString(Qt::ISODate)!=date ||
       day.year()<1 || day.year()>9999 || text.size()>4096) return fail("Invalid date or entry (maximum 4,096 characters).");
    if(text.trimmed().isEmpty()) text.clear();
    const auto previous=m_nodes[id].calendar.entries.value(date);
    if(previous==text) return true;
    if(!text.isEmpty() && !m_nodes[id].calendar.entries.contains(date) && m_nodes[id].calendar.entries.size()>=3660)
        return fail("A Date node supports up to 3,660 entries.");
    checkpoint();
    if(text.isEmpty()) m_nodes[id].calendar.entries.remove(date);
    else m_nodes[id].calendar.entries.insert(date,text);
    rebuild(); return true;
}

QString Engine::selectedEntryPrompt() const {
    const auto role=m_nodes.value(m_nodes.value(m_selected).parent).meetingSection;
    if(role=="agenda") return "Topic to discuss…";
    if(role=="notes") return "Capture a note…";
    if(role=="decisions") return "What was decided?";
    if(role=="actions") return "What needs to be done?";
    return {};
}
bool Engine::addMeetingTemplate() {
    const int parent=m_selected;
    if(!m_nodes.contains(parent) || m_selection.size()!=1) return fail("Select one parent node first.");
    if(m_nodes.size()+9>MaxNodes) return fail("Not enough room for the meeting template.");
    int depth=0;
    for(int p=parent;m_nodes[p].parent>=0;p=m_nodes[p].parent) ++depth;
    if(depth+3>MaxDepth) return fail("The meeting template would exceed maximum tree depth.");
    checkpoint();
    MapNode meeting; meeting.id=m_nextId++; meeting.parent=parent; meeting.text="Meeting Notes";
    meeting.meeting={{"date",QDate::currentDate().toString(Qt::ISODate)},{"time",""},{"attendees",""}};
    int note=0;
    for(const QString role : {QString("agenda"),QString("notes"),QString("decisions"),QString("actions")}) {
        MapNode section; section.id=m_nextId++; section.parent=meeting.id; section.meetingSection=role;
        section.text=role.left(1).toUpper()+role.mid(1); section.task=role=="actions";
        if(role=="decisions") section.style.insert("textColor","#3da995");
        MapNode entry; entry.id=m_nextId++; entry.parent=section.id; entry.task=section.task;
        section.children.append(entry.id);
        if(role=="notes") note=entry.id;
        meeting.children.append(section.id);
        m_nodes.insert(section.id,section); m_nodes.insert(entry.id,entry);
    }
    m_nodes.insert(meeting.id,meeting); m_nodes[parent].children.append(meeting.id); m_nodes[parent].folded=false;
    m_selected=note; m_selection={note}; rebuild(); emit editRequested(note); return true;
}
bool Engine::updateMeeting(QString date,QString time,QString attendees) {
    if(!m_nodes.contains(m_selected) || m_nodes[m_selected].meeting.isEmpty()) return false;
    if(!QDate::fromString(date,Qt::ISODate).isValid() || date!=QDate::fromString(date,Qt::ISODate).toString(Qt::ISODate) ||
       (!time.isEmpty() && (!QTime::fromString(time,"HH:mm").isValid() || time!=QTime::fromString(time,"HH:mm").toString("HH:mm"))) || attendees.size()>4096)
        return fail("Use YYYY-MM-DD, optional HH:mm, and up to 4,096 characters for attendees.");
    const QVariantMap details{{"date",date},{"time",time},{"attendees",attendees}};
    if(m_nodes[m_selected].meeting==details) return true;
    checkpoint(); m_nodes[m_selected].meeting=details; rebuild(); return true;
}


QVariantList Engine::nodeTemplates() const {
    return {QVariantMap{{"id","weekly-tasks"},{"name","Weekly task list"},
        {"description","Monday–Friday, with an empty task for each day."}},
        QVariantMap{{"id","meeting-notes"},{"name","Meeting Notes"},
        {"description","Agenda, Notes, Decisions, and Actions, each with an empty entry."}}};
}
QVariantMap Engine::templateCalendar(QString anchor, int monthOffset) const {
    QDate date=anchor.isEmpty() ? QDate::currentDate() : QDate::fromString(anchor,Qt::ISODate);
    if(!date.isValid() || monthOffset < -1 || monthOffset > 1) return {};
    const QDate month=QDate(date.year(),date.month(),1).addMonths(monthOffset);
    if(month.year()<2 || month.year()>9998) return {};
    const QDate first=month.addDays(1-month.dayOfWeek());
    QVariantList weeks;
    for(int row=0;row<6;++row) {
        const auto monday=first.addDays(row*7);
        if(monday>month.addMonths(1).addDays(-1)) break;
        int year; const int week=monday.weekNumber(&year);
        QVariantList days;
        for(int day=0;day<7;++day) {
            const auto d=monday.addDays(day);
            days.append(QVariantMap{{"day",d.day()},{"inMonth",d.month()==month.month()}});
        }
        weeks.append(QVariantMap{{"number",week},{"year",year},{"monday",monday.toString(Qt::ISODate)},
            {"label",QString("Week %1 · %2 – %3").arg(week).arg(monday.toString("d MMM yyyy"),monday.addDays(4).toString("d MMM yyyy"))},
            {"days",days}});
    }
    const auto today=QDate::currentDate();
    return {{"anchor",month.toString(Qt::ISODate)},{"title",month.toString("MMMM yyyy")},{"weeks",weeks},
        {"currentMonday",today.addDays(1-today.dayOfWeek()).toString(Qt::ISODate)}};
}
bool Engine::addNodeTemplate(QString templateId, QString weekDate) {
    if(templateId=="meeting-notes") return addMeetingTemplate();
    if(templateId!="weekly-tasks") return fail("Choose a supported node template.");
    if(!m_nodes.contains(m_selected) || m_selection.size()!=1) return fail("Select one parent node first.");
    const auto date=QDate::fromString(weekDate,Qt::ISODate);
    if(!date.isValid() || date.year()<2 || date.year()>9998) return fail("Choose a valid calendar week.");
    if(m_nodes.size()+11>MaxNodes) return fail("Not enough room for the weekly task list.");
    int depth=0;
    for(int p=m_selected;m_nodes[p].parent>=0;p=m_nodes[p].parent) ++depth;
    if(depth+3>MaxDepth) return fail("The weekly task list would exceed maximum tree depth.");
    const auto monday=date.addDays(1-date.dayOfWeek());
    checkpoint();
    const int parent=m_selected;
    MapNode week; week.id=m_nextId++; week.parent=parent; week.task=true;
    week.text=QString("Week %1 · %2 – %3").arg(monday.weekNumber())
        .arg(monday.toString("d MMM yyyy"),monday.addDays(4).toString("d MMM yyyy"));
    int firstTask=0;
    for(int i=0;i<5;++i) {
        MapNode day; day.id=m_nextId++; day.parent=week.id; day.task=true;
        day.text=monday.addDays(i).toString("dddd · d MMM");
        MapNode task; task.id=m_nextId++; task.parent=day.id; task.task=true;
        day.children.append(task.id); week.children.append(day.id);
        m_nodes.insert(day.id,day); m_nodes.insert(task.id,task);
        if(i==0) firstTask=task.id;
    }
    m_nodes.insert(week.id,week); m_nodes[parent].children.append(week.id); m_nodes[parent].folded=false;
    m_selected=firstTask; m_selection={firstTask}; rebuild(); emit editRequested(firstTask); return true;
}


QVariantList Engine::searchNodes(QString query) const {
    if(query.trimmed().isEmpty()) return {};
    QVector<QPair<int,int>> matches;
    QVector<int> pending{1};
    while(!pending.isEmpty()) {
        const int id=pending.takeLast(); const auto &node=m_nodes[id];
        for(auto it=node.children.crbegin();it!=node.children.crend();++it) pending.append(*it);
        QString text=m_textCache.value(id).plainText+" "+node.notes;
        if(node.kind=="date") text+=" "+Calendar::title(node.calendar)+" "+node.calendar.entries.values().join(' ');
        const auto match=Search::match(text,query);
        if(match.found) matches.append({match.score,id});
    }
    std::stable_sort(matches.begin(),matches.end(),[](auto a,auto b){return a.first<b.first;});
    QVariantList result; for(auto match:matches) result.append(match.second); return result;
}
bool Engine::revealSearchNode(int id) {
    if(!m_nodes.contains(id)) return false;
    QVector<int> folded;
    for(int p=m_nodes[id].parent;p>=0;p=m_nodes[p].parent) if(m_nodes[p].folded) folded.append(p);
    if(!folded.isEmpty()) {
        checkpoint(); for(int p:folded) m_nodes[p].folded=false; rebuild();
    }
    select(id); return true;
}
