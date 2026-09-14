#pragma once
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace AppIdentity {
inline QSettings *windowSettings() { return new QSettings("Mindarchy", "Mindarchy"); }
inline QString sessionDirectory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath("Mindarchy/session");
}
}
