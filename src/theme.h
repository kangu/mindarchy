#pragma once

#include <QColor>
#include <QString>
#include <QVariantList>
#include <QVector>

enum class NodeShape { Rounded, Rectangle, Pill, Underline, Hexagon, Scalloped, Embedded, Octagon };

struct NodeAppearance {
    QColor fill, border, text, branch;
    NodeShape shape = NodeShape::Rounded;
    qreal borderWidth = 0;
    qreal radius = 10;
    Qt::PenStyle borderStyle = Qt::SolidLine, branchStroke = Qt::SolidLine;
    qreal branchWidth = 1.5;
};

struct MapTheme {
    QString id, name;
    QColor canvas;
    QVector<QColor> palette;
};

namespace Themes {
const MapTheme &get(const QString &id);
bool contains(const QString &id);
QVariantList catalog();
QVariantMap layoutRecipe(const QString &id);
NodeAppearance appearance(const QString &themeId, int depth, int branchIndex);
} // namespace Themes
