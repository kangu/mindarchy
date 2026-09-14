#pragma once
#include <QString>
#include <QVariantList>
#include <Qt>

// Qt maps ControlModifier to Command on macOS; MetaModifier is physical Control.
namespace TabShortcuts {
#ifdef Q_OS_MACOS
inline constexpr bool nativeMac = true;
#else
inline constexpr bool nativeMac = false;
#endif
struct Command { const char *action; const char *label; int key; bool cycle; bool backwards; };
inline constexpr Command commands[] = {
    {"new", "New tab", Qt::Key_T, false, false},
    {"window", "New window", Qt::Key_N, false, false},
    {"close", "Close tab", Qt::Key_W, false, false},
    {"next", "Next tab", Qt::Key_Tab, true, false},
    {"previous", "Previous tab", Qt::Key_Tab, true, true}
};
inline const Command *find(const QString &action) {
    for(const auto &command:commands) if(action==QLatin1String(command.action)) return &command;
    return nullptr;
}
inline Qt::KeyboardModifiers modifiers(const Command &command, bool mac = nativeMac) {
    Qt::KeyboardModifiers result = command.cycle && mac ? Qt::MetaModifier : Qt::ControlModifier;
    if(command.backwards) result |= Qt::ShiftModifier;
    return result;
}
inline QString action(int key, Qt::KeyboardModifiers mods, bool mac = nativeMac) {
    if(key == Qt::Key_Backtab) { key = Qt::Key_Tab; mods |= Qt::ShiftModifier; }
    for(const auto &command:commands) if(key==command.key && mods==modifiers(command,mac)) return QString::fromLatin1(command.action);
    return {};
}
inline QVariantList help(bool mac = nativeMac) {
    QVariantList result;
    for(const auto &command:commands) {
        const QString prefix=command.cycle ? (mac?QStringLiteral("⌃ "):QStringLiteral("Ctrl+")) : (mac?QStringLiteral("⌘ "):QStringLiteral("Ctrl+"));
        const QString shift=command.backwards ? (mac?QStringLiteral("⇧ "):QStringLiteral("Shift+")) : QString();
        const QString key=command.key==Qt::Key_Tab ? QStringLiteral("Tab") : QString(QChar(command.key));
        result.append(QVariantMap{{"action",command.action},{"label",command.label},{"shortcut",prefix+shift+key}});
    }
    return result;
}
}
