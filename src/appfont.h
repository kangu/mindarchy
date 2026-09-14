#pragma once
#include <QString>
#include <QFontDatabase>

// Use the host's UI sans serif; keep measurement, canvas, export and editor identical.
inline QString mindarchyTextFamily() {
#ifdef Q_OS_WIN
    return QStringLiteral("Segoe UI");
#elif defined(Q_OS_MACOS)
    // Explicit family also resolves in offscreen Quick Look and export processes.
    return QStringLiteral("Helvetica Neue");
#else
    const auto families=QFontDatabase::families();
    for(const auto &name : {QStringLiteral("Inter"),QStringLiteral("Noto Sans"),QStringLiteral("DejaVu Sans")})
        if(families.contains(name)) return name;
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
#endif
}
