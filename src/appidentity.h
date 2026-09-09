#pragma once
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace AppIdentity {
// Stable storage identifiers from before the Mindarchy rename. Keeping these
// also lets old and new binaries coordinate their open-window session safely.
inline QSettings *windowSettings() { return new QSettings("MindmapBlue", "Mindmap Lab"); }
inline QString sessionDirectory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath("MindmapBlue/Mindmap Lab/session");
}
}
