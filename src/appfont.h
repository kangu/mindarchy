#pragma once
#include <QString>

// Windows does not resolve the CSS generic family name consistently in QFont.
// Use its built-in UI font for both measurement and painting.
inline QString mindarchyTextFamily() {
#ifdef Q_OS_WIN
    return QStringLiteral("Segoe UI");
#else
    return QStringLiteral("sans-serif");
#endif
}
