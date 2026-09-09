#include "shelltheme.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

ShellTheme::ShellTheme(QString directory, QObject *parent) : QObject(parent), m_directory(directory) {
#ifdef Q_OS_LINUX
    if (m_directory.isEmpty())
        m_directory = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/omarchy/current";
#endif
    if (m_directory.isEmpty()) return;
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &ShellTheme::reload);
    // Recover watches after atomic directory replacements and late Omarchy setup.
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, &ShellTheme::reload);
    m_poll.start();
    reload();
}
void ShellTheme::reload() {
    const QString filePath = m_directory + "/theme/colors.toml";
    QStringList wanted{m_directory, m_directory + "/theme", filePath};
    for (const auto &path : wanted)
        if (QFile::exists(path) && !m_watcher.files().contains(path) && !m_watcher.directories().contains(path))
            m_watcher.addPath(path);
    QFile file(filePath);
    // Keep the last valid palette during the remove/rename gap of a switch.
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return;
    const QString text = QString::fromUtf8(file.readAll());
    QHash<QString,QColor> palette;
    const QRegularExpression entry(R"re(^\s*([a-zA-Z_0-9]+)\s*=\s*["'](#[0-9a-fA-F]{6})["']\s*(?:#.*)?$)re",
                                  QRegularExpression::MultilineOption);
    auto matches = entry.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        palette.insert(match.captured(1), QColor(match.captured(2)));
    }
    if (!palette.contains("background") || !palette.contains("foreground") || !palette.contains("accent")) return;
    const QColor bg=palette["background"], fg=palette["foreground"], accent=palette["accent"];
    auto mix=[](QColor a,QColor b,double t) {
        return QColor::fromRgbF(a.redF()*(1-t)+b.redF()*t,a.greenF()*(1-t)+b.greenF()*t,a.blueF()*(1-t)+b.blueF()*t);
    };
    QVariantMap colors;
    auto assign=[&](QStringList originals,QColor replacement) {
        for (const auto &original:originals) colors.insert(original,replacement);
    };
    assign({"#111920","#111b23","#122029","#172129","#17232c","#19242d","#19252e"},bg);
    assign({"#e0e9ee","#d5e1e9","#c5d3da","#a5b5bf","#c3cbd0"},fg);
    assign({"#81939f","#94a9b7","#718896","#647783","#627782"},mix(bg,fg,.65));
    assign({"#70d8c4"},accent);
    assign({"#1e2c36","#22313b","#22323d","#23323c","#253540"},mix(bg,fg,.08));
    assign({"#2a3d48","#334653"},mix(bg,fg,.14));
    assign({"#35505a"},mix(bg,fg,.22));
    assign({"#29463f","#317d73"},mix(bg,accent,.28));
    assign({"#32434d","#2a3943","#34434c","#3a4b57","#40545f"},mix(bg,fg,.25));
    assign({"#ffffff"},fg);
    assign({"#f08b83","#efaa96","#ef8585"},palette.value("color1",QColor("#ef8585")));
    assign({"#563832"},mix(bg,palette.value("color1",QColor("#ef8585")),.2));
    assign({"#ffd3c7"},fg);
    if (colors != m_colors) { m_colors=colors; emit changed(); }
}
