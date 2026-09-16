#pragma once
#include <Qt>

// Keys that keep their canvas action when a single node is selected, instead
// of replacing that node's title. Add further bare-key commands here.
namespace CanvasShortcuts {
enum class Action { None, ZoomIn, ZoomOut };
struct Command { Action action; int key; };
inline constexpr Command commands[] = {
    {Action::ZoomIn, Qt::Key_Plus},
    {Action::ZoomIn, Qt::Key_Equal},
    {Action::ZoomOut, Qt::Key_Minus},
};
inline Action action(int key, Qt::KeyboardModifiers mods) {
    mods &= ~(Qt::KeypadModifier | Qt::GroupSwitchModifier);
    if (mods != Qt::NoModifier) return Action::None;
    for (const auto &command : commands)
        if (command.key == key) return command.action;
    return Action::None;
}
}
