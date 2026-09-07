#include "theme.h"

#include <array>

namespace {
const QVector<MapTheme> &allThemes() {
    static const QVector<MapTheme> themes{
        {"beach-day", "Beach Day", "#faf9f6",
         {"#98a5cc", "#eaa36d", "#f3cb8d", "#90c8ad", "#61a8aa", "#d8ba83"}},
        {"holographic", "Holographic", "#f5f0ff",
         {"#aac1f5", "#c0eef1", "#d69bef", "#ece7b7", "#ccc7f3", "#f0bde0"}},
        {"retro", "Retro", "#f0ede5",
         {"#af355f", "#517659", "#6267a3", "#b2905a", "#8c5676", "#3f7187"}},
        {"arcade", "Arcade", "#2a1c38",
         {"#b86cc7", "#e36b73", "#ea9965", "#ecd06a", "#98ca63", "#64c6ec"}},
        {"lab", "Lab", "#111920",
         {"#3da995", "#7e8dde", "#ca9b56", "#bf7ba6", "#efb86f", "#92dcc7"}},
    };
    return themes;
}
QColor transparent() { return QColor(0, 0, 0, 0); }
QColor branchColor(const MapTheme &theme, int branchIndex) {
    const int size = theme.palette.size();
    return theme.palette[size ? ((branchIndex % size) + size) % size : 0];
}
} // namespace

namespace Themes {
const MapTheme &get(const QString &id) {
    for (const auto &theme : allThemes())
        if (theme.id == id)
            return theme;
    return allThemes().last();
}
bool contains(const QString &id) {
    for (const auto &theme : allThemes())
        if (theme.id == id)
            return true;
    return false;
}
QVariantList catalog() {
    QVariantList result;
    for (const auto &theme : allThemes()) {
        QVariantList palette;
        for (const QColor &color : theme.palette)
            palette.append(color);
        result.append(QVariantMap{{"id", theme.id},
                                  {"name", theme.name},
                                  {"canvas", theme.canvas},
                                  {"palette", palette}});
    }
    return result;
}
NodeAppearance appearance(const QString &themeId, int depth, int branchIndex) {
    const MapTheme &theme = get(themeId);
    const QColor color = branchColor(theme, branchIndex);
    if (theme.id == "beach-day") {
        if (depth == 0)
            return {"#f3ddb4", "#473c32", "#302a25", color, NodeShape::Rounded, 1.5, 14};
        if (depth == 1)
            return {"#ffffff", color, "#302a25", color, NodeShape::Rounded, 2, 12};
        return {transparent(), color, "#302a25", color, NodeShape::Underline, 2, 0};
    }
    if (theme.id == "holographic") {
        if (depth == 0)
            return {"#f3cdf3", "#19151b", "#19151b", "#19151b", NodeShape::Rectangle, 2, 0};
        const NodeShape shape = depth == 1   ? NodeShape::Rectangle
                                : depth == 2 ? NodeShape::Pill
                                             : NodeShape::Hexagon;
        return {color, "#19151b", "#19151b", "#19151b", shape, 2, shape == NodeShape::Pill ? 24.0 : 0.0};
    }
    if (theme.id == "retro") {
        if (depth == 0)
            return {"#32345f", "#32345f", "#fff2d0", color, NodeShape::Hexagon, 2, 0};
        if (depth == 1)
            return {color, color.darker(140), branchIndex % theme.palette.size() == 3 ? QColor("#282332") : QColor("#fff2d0"), color, NodeShape::Rectangle, 2, 0};
        return {theme.canvas, color, "#302d3d", color, NodeShape::Rounded, 2, 10};
    }
    if (theme.id == "arcade") {
        if (depth == 0)
            return {"#81364e", "#d87b8d", "#fff2d0", color, NodeShape::Scalloped, 2, 12};
        if (depth == 1)
            return {color.lighter(135), color.darker(125), "#2a1c38", color, NodeShape::Pill, 2, 24};
        if (depth == 2)
            return {color.lighter(135), color.darker(125), "#2a1c38", color, NodeShape::Rounded, 2, 10};
        return {transparent(), color, "#fff2d0", color, NodeShape::Underline, 2, 0};
    }
    return {color.darker(depth == 0 ? 300 : 260), color, "#e9eff4", color,
            NodeShape::Rounded, 2, 10};
}
} // namespace Themes
