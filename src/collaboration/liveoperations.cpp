#include "liveoperations.h"
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <algorithm>
namespace LiveOperations {
QJsonObject normalize(const QByteArray &bytes) {
    auto doc = QJsonDocument::fromJson(bytes).object();
    const auto records = doc.take("nodes").toArray();
    const auto links = doc.take("connections").toArray();
    QMap<int, QString> ids;
    QMap<int, int> orders;
    for (auto v : records) {
        auto n = v.toObject();
        int id = n["id"].toInt();
        ids[id] = n["syncId"].toString(QString("legacy:%1").arg(id));
        int i = 0;
        for (auto c : n["children"].toArray())
            orders[c.toInt()] = i++;
    }
    QJsonObject nodes, edges;
    for (auto v : records) {
        auto n = v.toObject();
        int id = n.take("id").toInt(), parent = n["parent"].toInt(-1);
        n.remove("syncId");
        n.remove("children");
        n["parent"] = parent < 0 ? QString() : ids[parent];
        n["order"] = orders.value(id);
        nodes[ids[id]] = n;
    }
    for (auto v : links) {
        auto e = v.toArray();
        if (e.size() != 2)
            continue;
        auto a = ids[e[0].toInt()], b = ids[e[1].toInt()];
        if (a > b)
            std::swap(a, b);
        auto row = edges[a].toObject();
        row[b] = true;
        edges[a] = row;
    }
    return {{"props", doc}, {"nodes", nodes}, {"edges", edges}};
}
QByteArray project(const QJsonObject &c) {
    auto doc = c["props"].toObject(), nodes = c["nodes"].toObject();
    QStringList keys = nodes.keys();
    QString root;
    for (auto k : keys)
        if (nodes[k].toObject()["parent"].toString().isEmpty())
            root = k;
    if (root.isEmpty())
        return {};
    keys.removeAll(root);
    keys.prepend(root);
    QMap<QString, int> ids;
    int i = 1;
    for (auto k : keys)
        ids[k] = i++;
    QJsonArray records, edges;
    for (auto k : keys) {
        auto n = nodes[k].toObject();
        QStringList kids;
        for (auto child : keys)
            if (nodes[child].toObject()["parent"].toString() == k)
                kids.append(child);
        std::sort(kids.begin(), kids.end(), [&](const QString &a, const QString &b) {
            double x = nodes[a].toObject()["order"].toDouble(), y = nodes[b].toObject()["order"].toDouble();
            return x == y ? a < b : x < y;
        });
        QJsonArray children;
        for (auto child : kids)
            children.append(ids[child]);
        auto parent = n["parent"].toString();
        n.remove("order");
        n["id"] = ids[k];
        n["syncId"] = k;
        n["parent"] = parent.isEmpty() ? -1 : ids[parent];
        n["children"] = children;
        records.append(n);
    }
    const auto links = c["edges"].toObject();
    for (auto a : links.keys())
        for (auto b : links[a].toObject().keys())
            if (ids.contains(a) && ids.contains(b))
                edges.append(QJsonArray{ids[a], ids[b]});
    doc["nodes"] = records;
    doc["connections"] = edges;
    return QJsonDocument(doc).toJson(QJsonDocument::Compact);
}
static void walk(const QJsonValue &a, const QJsonValue &b, QJsonArray path, QJsonArray &out) {
    if (a == b)
        return;
    if (a.isObject() && b.isObject()) {
        auto x = a.toObject(), y = b.toObject();
        QStringList keys = x.keys();
        for (auto k : y.keys())
            if (!keys.contains(k))
                keys.append(k);
        keys.sort();
        for (auto k : keys) {
            auto p = path;
            p.append(k);
            walk(x.value(k), y.value(k), p, out);
        }
        return;
    }
    QJsonObject op{{"op", b.isUndefined() ? "remove" : "set"}, {"path", path}};
    if (!b.isUndefined())
        op["value"] = b;
    out.append(op);
}
QJsonArray diff(const QJsonObject &a, const QJsonObject &b) {
    QJsonArray result;
    walk(a["props"], b["props"], QJsonArray{"props"}, result);
    walk(a["nodes"], b["nodes"], QJsonArray{"nodes"}, result);
    const auto x = a["edges"].toObject(), y = b["edges"].toObject();
    QStringList from = x.keys();
    for (auto k : y.keys())
        if (!from.contains(k))
            from.append(k);
    from.sort();
    for (auto f : from) {
        const auto left = x[f].toObject(), right = y[f].toObject();
        QStringList to = left.keys();
        for (auto k : right.keys())
            if (!to.contains(k))
                to.append(k);
        to.sort();
        for (auto t : to)
            walk(left.value(t), right.value(t), QJsonArray{"edges", f, t}, result);
    }
    return result;
}
static void put(QJsonObject &o, const QJsonArray &p, int i, const QJsonValue &value, bool remove) {
    auto key = p[i].toString();
    if (i == p.size() - 1) {
        if (remove)
            o.remove(key);
        else
            o[key] = value;
        return;
    }
    auto child = o[key].toObject();
    put(child, p, i + 1, value, remove);
    o[key] = child;
}
bool apply(QJsonObject &canonical, const QJsonArray &operations, QString *error) {
    auto c = canonical;
    auto fail = [&](const char *s) {
        if (error)
            *error = QString::fromLatin1(s);
        return false;
    };
    if (operations.size() > 1000)
        return fail("operation limit");
    for (auto v : operations) {
        auto op = v.toObject();
        auto p = op["path"].toArray();
        bool remove = op["op"] == "remove";
        if (p.size() < 2 || p.size() > 32 || (!remove && op["op"] != "set"))
            return fail("invalid operation");
        auto area = p[0].toString();
        if (area != "nodes" && area != "props" && area != "edges")
            return fail("invalid path");
        for (auto part : p)
            if (!part.isString() || part.toString().isEmpty())
                return fail("invalid path");
        if (area == "nodes") {
            auto nodes = c["nodes"].toObject();
            auto uid = p[1].toString();
            if (p.size() == 2 && !remove && nodes.contains(uid)) {
                if (nodes[uid] != op["value"])
                    return fail("identity collision");
                continue;
            }
            if (p.size() > 2 && !nodes.contains(uid))
                continue;
            if (remove && p.size() == 2) {
                if (!nodes.contains(uid))
                    continue;
                if (nodes[uid].toObject()["parent"].toString().isEmpty())
                    return fail("root removal");
                QSet<QString> gone{uid};
                bool changed = true;
                while (changed) {
                    changed = false;
                    for (auto k : nodes.keys())
                        if (!gone.contains(k) && gone.contains(nodes[k].toObject()["parent"].toString())) {
                            gone.insert(k);
                            changed = true;
                        }
                }
                for (auto k : gone)
                    nodes.remove(k);
                c["nodes"] = nodes;
                auto edges = c["edges"].toObject();
                for (auto a : edges.keys()) {
                    if (gone.contains(a)) {
                        edges.remove(a);
                        continue;
                    }
                    auto row = edges[a].toObject();
                    for (auto b : gone)
                        row.remove(b);
                    if (row.isEmpty())
                        edges.remove(a);
                    else
                        edges[a] = row;
                }
                c["edges"] = edges;
                continue;
            }
        }
        if (area == "edges") {
            if (p.size() != 3)
                return fail("invalid edge");
            if (!remove && op["value"] != true)
                return fail("invalid edge value");
            const auto nodes = c["nodes"].toObject();
            if (!nodes.contains(p[1].toString()) || !nodes.contains(p[2].toString()))
                continue;
        }
        put(c, p, 0, op["value"], remove);
        if (area == "edges") {
            auto edges = c["edges"].toObject();
            if (edges[p[1].toString()].toObject().isEmpty())
                edges.remove(p[1].toString());
            c["edges"] = edges;
        }
    }
    auto nodes = c["nodes"].toObject();
    int roots = 0;
    for (auto uid : nodes.keys()) {
        auto n = nodes[uid].toObject();
        if (!n["parent"].isString() || !n["order"].isDouble())
            return fail("invalid node");
        QString parent = n["parent"].toString();
        if (parent.isEmpty())
            ++roots;
        QSet<QString> seen{uid};
        while (!parent.isEmpty()) {
            if (seen.contains(parent) || !nodes.contains(parent))
                return fail("invalid parent");
            seen.insert(parent);
            parent = nodes[parent].toObject()["parent"].toString();
        }
    }
    if (roots != 1 || nodes.size() > 10000)
        return fail("invalid root or node limit");
    for (auto uid : canonical["nodes"].toObject().keys())
        if (canonical["nodes"].toObject()[uid].toObject()["parent"].toString().isEmpty() &&
            nodes[uid].toObject()["parent"].toString() != QString())
            return fail("root move");
    if (project(c).size() > 1024 * 1024)
        return fail("document limit");
    canonical = c;
    return true;
}
} // namespace LiveOperations
