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
        {"canopy", "Canopy", "#f5f4eb",
         {"#47705a", "#6c7f4e", "#927344", "#497d7b", "#806785", "#a45f4d"}},
        {"atlas", "Atlas", "#f5f7fb",
         {"#4167a6", "#247f89", "#8264a0", "#bd7544", "#4f8063", "#ad5974"}},
        {"studio", "Studio", "#faf6ef",
         {"#b3634c", "#477a78", "#827043", "#74668c", "#567249", "#526b8d"}},
        {"nocturne", "Nocturne", "#111a2b",
         {"#8ab8ef", "#6bc8c0", "#b39be8", "#e8b976", "#91c792", "#e69aad"}},
        // Match the website's appearance examples, including its dark-mode accents.
        {"paper", "Paper", "#fbfaf6", {"#3e6b50", "#9c492d", "#375d80"}},
        {"forest", "Forest", "#203b32", {"#cbe2b5", "#f5b399", "#b7d8fb"}},
        {"midnight", "Midnight", "#253443", {"#c4d8f0", "#f5b399", "#b7d8fb"}},
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
    return get(QStringLiteral("lab"));
}
bool contains(const QString &id) {
    for (const auto &theme : allThemes())
        if (theme.id == id)
            return true;
    return false;
}
QVariantMap layoutRecipe(const QString &id) {
    if(id=="canopy") return {{"layout","Horizontal"},{"spacing","Wide"},{"branchStyle","Rounded"},{"name","Open brainstorm"},{"purpose","Explore ideas with breathing room"}};
    if(id=="atlas") return {{"layout","Vertical"},{"spacing","Wide"},{"branchStyle","Angular"},{"name","Hierarchy"},{"purpose","Show teams, systems and dependencies"}};
    if(id=="studio") return {{"layout","Compact"},{"spacing","Standard"},{"branchStyle","Angular"},{"name","Reading outline"},{"purpose","Scan notes and plans in order"}};
    if(id=="nocturne") return {{"layout","Horizontal"},{"spacing","Standard"},{"branchStyle","Rounded"},{"name","Focused planning"},{"purpose","Plan with clear branches on a dark canvas"}};
    return {};
}
QVariantList catalog() {
    QVariantList result;
    for (const auto &theme : allThemes()) {
        QVariantList palette;
        for (const QColor &color : theme.palette)
            palette.append(color);
        auto previewStyle=[](const NodeAppearance &a) {
            return QVariantMap{{"fill",a.fill},{"border",a.border},{"text",a.text},{"branch",a.branch},
                               {"shape",int(a.shape)},{"borderWidth",a.borderWidth}};
        };
        QVariantList previewBranches, previewLeaves;
        for(int i=0;i<3;++i) {
            previewBranches.append(previewStyle(appearance(theme.id,1,i)));
            previewLeaves.append(previewStyle(appearance(theme.id,2,i)));
        }
        result.append(QVariantMap{{"id", theme.id},
                                  {"name", theme.name},
                                  {"canvas", theme.canvas},
                                  {"palette", palette}, {"recipe",layoutRecipe(theme.id)},
                                  {"previewRoot",previewStyle(appearance(theme.id,0,0))},
                                  {"previewBranches",previewBranches},{"previewLeaves",previewLeaves}});
    }
    return result;
}
NodeAppearance appearance(const QString &themeId, int depth, int branchIndex) {
    const MapTheme &theme = get(themeId);
    const QColor color = branchColor(theme, branchIndex);
    auto tint=[](QColor foreground,QColor background,qreal amount) {
        return QColor::fromRgbF(foreground.redF()*amount+background.redF()*(1-amount),
                                foreground.greenF()*amount+background.greenF()*(1-amount),
                                foreground.blueF()*amount+background.blueF()*(1-amount));
    };
    if (theme.id == "paper" || theme.id == "forest" || theme.id == "midnight") {
        // The website keeps the same quiet outlined style at every depth.
        return {theme.canvas, color, color, color, NodeShape::Rounded, 1.5, 11,
                Qt::SolidLine, Qt::SolidLine, 1.5};
    }
    if(theme.id=="canopy") {
        if(depth==0) return {"#29483a","#29483a","#fffdf1",color,NodeShape::Pill,0,24};
        if(depth==1) return {tint(color,theme.canvas,.15),color,"#26362e",color,NodeShape::Rounded,1.5,14};
        return {transparent(),color,"#26362e",color,NodeShape::Underline,1.5,0};
    }
    if(theme.id=="atlas") {
        if(depth==0) return {"#243654","#243654","#ffffff",color,NodeShape::Rectangle,0,0};
        return {depth==1 ? tint(color,theme.canvas,.16) : QColor("#ffffff"),color,"#243249",color,NodeShape::Rectangle,1.5,0};
    }
    if(theme.id=="studio") {
        if(depth==0) return {"#ede2d3","#876e55","#352d27",color,NodeShape::Rounded,1,8};
        if(depth==1) return {tint(color,theme.canvas,.12),color,"#352d27",color,NodeShape::Rounded,1,6};
        return {transparent(),color,"#352d27",color,NodeShape::Embedded,0,0};
    }
    if(theme.id=="nocturne") {
        if(depth==0) return {"#c1d8ff","#c1d8ff","#111d33",color,NodeShape::Pill,0,24};
        if(depth==1) return {tint(color,theme.canvas,.17),color,"#edf2f7",color,NodeShape::Rounded,1.5,10};
        return {transparent(),color,"#edf2f7",color,NodeShape::Underline,1.5,0};
    }
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
