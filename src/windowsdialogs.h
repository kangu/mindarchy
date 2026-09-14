#pragma once
class Engine;
class QObject;
class QQuickWindow;
// Standard Windows dialogs, owned by the document window.
void installWindowsDialogs(Engine *document, QQuickWindow *window, QObject *workspace = nullptr);
