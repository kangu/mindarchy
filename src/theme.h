#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

enum class NodeShape { Rounded, Rectangle, Pill, Underline, Hexagon, Scalloped, Embedded, Octagon };

struct TaskAppearance {
    QColor accent;
    qreal completedFrameOpacity = .28;
    qreal checkWidth = 2.8;
    qreal cornerRadius = 2;
    qreal trackOpacity = .22;
    qreal progressWidth = 2.5;
};

struct NodeAppearance {
    QColor fill, border, text, branch;
    NodeShape shape = NodeShape::Rounded;
    qreal borderWidth = 0;
    qreal radius = 10;
    Qt::PenStyle borderStyle = Qt::SolidLine, branchStroke = Qt::SolidLine;
    qreal branchWidth = 2;
    TaskAppearance task;
    QString fontFamily;
    int fontSize = 15;
};

struct MapTheme {
    QString id, name;
    QColor canvas;
    QVector<QColor> palette;
    QString description;
    QColor ink, rootFill, rootText;
    bool dark = false;
};

namespace Themes {
inline const QString DefaultId = QStringLiteral("porcelain");
bool omarchyInstallation(const QStringList &roots);
QString defaultId();
const MapTheme &get(const QString &id);
bool contains(const QString &id);
qreal contrastRatio(QColor a, QColor b);
QColor contrastInk(QColor background);
TaskAppearance taskAppearance(const QString &themeId,QColor background);
QString fontFamily(const QString &themeId);
QVariantList catalog(QString defaultTheme = defaultId());
QVariantMap layoutRecipe(const QString &id);
NodeAppearance appearance(const QString &themeId, int depth, int branchIndex);
} // namespace Themes
