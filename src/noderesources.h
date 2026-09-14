#pragma once
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QVariantList>

// Runtime file targets are absolute; document targets are relative to the .omm.
namespace NodeResources {
inline bool valid(const QVariantMap &r) {
    const auto kind=r.value("kind").toString(), target=r.value("target").toString();
    if (r.value("name").toString().size()>512 || target.isEmpty() || target.size()>8192 || target.contains(QChar::Null)) return false;
    if (kind=="file") return true;
    const QUrl url(target,QUrl::StrictMode);
    return kind=="url" && url.isValid() && !url.host().isEmpty() &&
        (url.scheme()=="https" || url.scheme()=="http") && url.userInfo().isEmpty();
}
inline QJsonArray encode(const QVariantList &items, const QString &document) {
    QJsonArray result;
    for (auto value:items) {
        auto r=value.toMap();
        if (r["kind"]=="file" && !document.isEmpty())
            r["target"]=QFileInfo(document).absoluteDir().relativeFilePath(r["target"].toString());
        result.append(QJsonObject::fromVariantMap(r));
    }
    return result;
}
inline bool decode(const QJsonValue &value, const QString &document, QVariantList &result) {
    if(value.isUndefined()) return true; // Legacy documents.
    if(!value.isArray() || value.toArray().size()>100) return false;
    for(auto item:value.toArray()) {
        if(!item.isObject()) return false;
        const auto object=item.toObject();
        if(!object["kind"].isString() || !object["target"].isString() || !object["name"].isString()) return false;
        auto r=object.toVariantMap();
        if(!valid(r)) return false;
        if(r["kind"]=="file") {
            auto target=r["target"].toString();
            if(QDir::isRelativePath(target)) {
                if(document.isEmpty()) return false;
                target=QFileInfo(document).absoluteDir().absoluteFilePath(target);
            }
            r["target"]=QDir::cleanPath(target);
        }
        result.append(r);
    }
    return true;
}
}
