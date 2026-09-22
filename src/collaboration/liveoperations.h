#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
namespace LiveOperations {
QJsonObject normalize(const QByteArray &document);
QByteArray project(const QJsonObject &canonical);
QJsonArray diff(const QJsonObject &before, const QJsonObject &after);
bool apply(QJsonObject &canonical, const QJsonArray &operations, QString *error = nullptr);
} // namespace LiveOperations
