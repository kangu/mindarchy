#include "theme.h"

#include <array>
#include <QDir>
#include <QFontDatabase>
#include <QHash>
#include <QResource>
#include <QFileInfo>
#include <QStandardPaths>
#include <cmath>

static void initializeThemeFonts() { Q_INIT_RESOURCE(fonts); }

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
        // A restrained collection: native sans typography, quiet surfaces and clear ink.
        {"porcelain", "Porcelain", "#F7F8FA",
         {"#52759A", "#4C807B", "#86709D", "#997848", "#AA6865", "#638066"},
         "Cool white · slate ink", "#273444", "#293E55", "#FFFFFF"},
        {"omarchy", "Omarchy", "#1A1B26",
         {"#9ECE6A", "#7AA2F7", "#BB9AF7", "#E0AF68", "#F7768E", "#0DB9D7"},
         "Terminal night · precise mono", "#C0CAF5", "#9ECE6A", "#1A1B26", true},
        {"sky", "Sky", "#F2F7FB",
         {"#467CA6", "#4A8789", "#797AAD", "#9A7950", "#A96F83", "#628269"},
         "Air blue · deep navy", "#263C50", "#D6E7F3", "#233E57"},
        {"starlight", "Starlight", "#FAF7F1",
         {"#987344", "#668078", "#80729D", "#547A99", "#A36863", "#7F8151"},
         "Warm ivory · bronze", "#403A32", "#EAE0CE", "#483B2B"},
        {"sage", "Sage", "#F4F7F3",
         {"#557C65", "#4B7E88", "#94794E", "#8B7494", "#A56D61", "#687CA0"},
         "Soft mineral · forest ink", "#2C3D34", "#345744", "#F7FAF4"},
        {"blush", "Blush", "#FCF6F5",
         {"#A46679", "#6E7C9E", "#598278", "#9A7A50", "#8B70A0", "#A76F59"},
         "Rose porcelain · plum ink", "#46343F", "#F1DDE2", "#583B4A"},
        {"graphite", "Graphite", "#20242B",
         {"#9AB7D8", "#93BFB7", "#B7A6CC", "#C8B18D", "#D0A2A0", "#A8BCA0"},
         "Charcoal · soft silver", "#E6E9EF", "#CFD9E6", "#26313E", true},
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
bool omarchyInstallation(const QStringList &roots) {
    for(const auto &root:roots) {
        if(root.isEmpty()) continue;
        if(QFileInfo::exists(root+"/bin/omarchy-theme-set") ||
           QFileInfo::exists(root+"/current/theme/colors.toml")) return true;
    }
    return false;
}
QString defaultId() {
#ifdef Q_OS_LINUX
    const auto configuredState=qEnvironmentVariable("XDG_STATE_HOME");
    const auto state=configuredState.isEmpty() ? QDir::homePath()+"/.local/state" : configuredState;
    if(omarchyInstallation({qEnvironmentVariable("OMARCHY_PATH"),
            QStringLiteral("/usr/share/omarchy"),QDir::homePath()+"/.local/share/omarchy",
            state+"/omarchy",
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/omarchy"}))
        return QStringLiteral("omarchy");
#endif
    return DefaultId;
}

QString fontFamily(const QString &themeId) {
    static const QHash<QString,QString> directories{
        {"omarchy","jetbrainsmono"},{"porcelain","inter"},{"sky","manrope"},{"starlight","lora"},{"sage","sourcesans3"},
        {"blush","dmsans"},{"graphite","ibmplexsans"},{"beach-day","nunitosans"},
        {"holographic","spacegrotesk"},{"retro","archivo"},{"arcade","spacemono"},
        {"lab","ibmplexmono"},{"canopy","literata"},{"atlas","publicsans"},
        {"studio","sourceserif4"},{"nocturne","plusjakartasans"},{"paper","newsreader"},
        {"forest","lato"},{"midnight","outfit"}};
    static const QHash<QString,QString> families=[] {
        initializeThemeFonts(); QHash<QString,QString> loaded;
        const QDir root(":/assets/fonts");
        for(const auto &folder:root.entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
            const QDir directory(root.filePath(folder));
            for(const auto &file:directory.entryList({"*.ttf"},QDir::Files)) {
                const int id=QFontDatabase::addApplicationFont(directory.filePath(file));
                const auto names=QFontDatabase::applicationFontFamilies(id);
                if(!names.isEmpty()) loaded[folder]=names.first();
            }
        }
        return loaded;
    }();
    return families.value(directories.value(themeId,"inter"),QStringLiteral("Helvetica Neue"));
}

qreal contrastRatio(QColor a,QColor b) {
    auto luminance=[](QColor c) {
        auto linear=[](qreal v) { return v<=.04045 ? v/12.92 : std::pow((v+.055)/1.055,2.4); };
        return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());
    };
    const qreal x=luminance(a),y=luminance(b);
    return (std::max(x,y)+.05)/(std::min(x,y)+.05);
}
QColor contrastInk(QColor background) {
    return contrastRatio(background,Qt::white)>=contrastRatio(background,Qt::black) ? QColor(Qt::white) : QColor(Qt::black);
}

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
QVariantList catalog(QString defaultTheme) {
    QVariantList result;
    for (const auto &theme : allThemes()) {
        QVariantList palette;
        for (const QColor &color : theme.palette)
            palette.append(color);
        auto previewStyle=[](const NodeAppearance &a) {
            return QVariantMap{{"fill",a.fill},{"border",a.border},{"text",a.text},{"branch",a.branch},
                               {"shape",int(a.shape)},{"borderWidth",a.borderWidth},{"taskAccent",a.task.accent},{"taskRadius",a.task.cornerRadius},{"taskCheckWidth",a.task.checkWidth},{"taskProgressWidth",a.task.progressWidth}};
        };
        QVariantList previewBranches, previewLeaves;
        for(int i=0;i<3;++i) {
            previewBranches.append(previewStyle(appearance(theme.id,1,i)));
            previewLeaves.append(previewStyle(appearance(theme.id,2,i)));
        }
        result.append(QVariantMap{{"id", theme.id},
                                  {"name", theme.name}, {"description",theme.description}, {"fontFamily",fontFamily(theme.id)}, {"refined",theme.ink.isValid()}, {"isDefault",theme.id==defaultTheme},
                                  {"canvas", theme.canvas},
                                  {"palette", palette}, {"recipe",layoutRecipe(theme.id)},
                                  {"previewRoot",previewStyle(appearance(theme.id,0,0))},
                                  {"previewBranches",previewBranches},{"previewLeaves",previewLeaves}});
    }
    // Put the new collection first without changing stable theme IDs.
    QVariantList ordered;
    for(const auto &entry:result) if(entry.toMap()["refined"].toBool()) ordered.append(entry);
    for(const auto &entry:result) if(!entry.toMap()["refined"].toBool()) ordered.append(entry);
    // The platform default leads; Porcelain and Omarchy remain the first pair.
    if(defaultTheme=="omarchy") {
        for(qsizetype i=0;i<ordered.size();++i)
            if(ordered[i].toMap()["id"].toString()==defaultTheme) {
                ordered.prepend(ordered.takeAt(i)); break;
            }
    }
    return ordered;
}
static NodeAppearance baseAppearance(const QString &themeId, int depth, int branchIndex) {
    const MapTheme &theme = get(themeId);
    const QColor color = branchColor(theme, branchIndex);
    auto tint=[](QColor foreground,QColor background,qreal amount) {
        return QColor::fromRgbF(foreground.redF()*amount+background.redF()*(1-amount),
                                foreground.greenF()*amount+background.greenF()*(1-amount),
                                foreground.blueF()*amount+background.blueF()*(1-amount));
    };
    if(themeId=="omarchy") {
        if(depth==0) return {theme.rootFill,theme.rootFill,theme.rootText,color,NodeShape::Rounded,0,8,
                            Qt::SolidLine,Qt::SolidLine,1.5};
        if(depth==1) return {tint(color,theme.canvas,.10),color,theme.ink,color,
                            NodeShape::Rounded,1.5,5,Qt::SolidLine,Qt::SolidLine,1.5};
        return {transparent(),color,theme.ink,color,NodeShape::Underline,1.5,0,
                Qt::SolidLine,Qt::SolidLine,1.5};
    }
    if(theme.ink.isValid()) {
        if(depth==0) return {theme.rootFill,theme.rootFill,theme.rootText,color,NodeShape::Rounded,0,16,
                            Qt::SolidLine,Qt::SolidLine,1.5};
        if(depth==1) return {tint(color,theme.canvas,theme.dark ? .13 : .08),color,theme.ink,color,
                            NodeShape::Rounded,1.5,11,Qt::SolidLine,Qt::SolidLine,1.5};
        return {transparent(),color,theme.ink,color,NodeShape::Underline,1.5,0,
                Qt::SolidLine,Qt::SolidLine,1.5};
    }
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
TaskAppearance taskAppearance(const QString &themeId,QColor background) {
    struct Treatment { QString id; QColor light,dark; qreal radius,check,ring,track; };
    static const QVector<Treatment> treatments{
        {"porcelain","#147D64","#82DDB9",2.8,2.8,2.4,.18},
        {"sky","#2876A2","#91CFF1",3,2.7,2.3,.18},
        {"starlight","#8B692E","#E2C48B",2,2.6,2.2,.22},
        {"sage","#39754F","#A0D7AC",3,2.9,2.6,.20},
        {"blush","#A34F73","#E8AEC8",3,2.7,2.3,.18},
        {"omarchy","#426924","#9ECE6A",1.5,2.8,2.5,.22},
        {"graphite","#4F7094","#A7C7EC",2.5,2.9,2.6,.24},
    };
    TaskAppearance style;
    QColor light("#087F5B"),dark("#82E2B7");
    for(const auto &t:treatments) if(t.id==themeId) {
        light=t.light; dark=t.dark; style.cornerRadius=t.radius; style.checkWidth=t.check;
        style.progressWidth=t.ring; style.trackOpacity=t.track; break;
    }
    style.accent=contrastRatio(background,light)>=contrastRatio(background,dark) ? light : dark;
    return style;
}
NodeAppearance appearance(const QString &themeId,int depth,int branchIndex) {
    auto style=baseAppearance(themeId,depth,branchIndex);
    style.fontFamily=fontFamily(themeId);
    style.task=taskAppearance(themeId,style.fill.alpha() ? style.fill : get(themeId).canvas);
    return style;
}
} // namespace Themes
