#include <QWindow>
#import <AppKit/AppKit.h>

void prepareMacMaximizedWindow(QWindow *window) {
    window->create();
    NSWindow *native = reinterpret_cast<NSView *>(window->winId()).window;
    // Zoom the hidden native window: AppKit retains its restore-down frame,
    // while no intermediate normal window can be presented on screen.
    if (!native.zoomed) [native zoom:nil];
    window->setWindowState(Qt::WindowMaximized);
}
