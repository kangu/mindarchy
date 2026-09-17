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

bool macWindowIsZoomed(QWindow *window) {
    if (!window || !window->handle()) return false;
    return reinterpret_cast<NSView *>(window->winId()).window.isZoomed;
}

void presentMacWindow(QWindow *window) {
    if (!window) return;
    window->create();
    NSWindow *native = reinterpret_cast<NSView *>(window->winId()).window;
    const BOOL zoomed = native.zoomed;
    const NSRect frame = native.frame;
    window->raise();
    window->requestActivate();
    [native makeKeyAndOrderFront:nil];
    // Qt raise/activate can toggle AppKit zoom back to the restore-down
    // rectangle. Put a double-click-zoomed window back if that happened.
    if (zoomed) {
        if (!native.zoomed) [native zoom:nil];
        if (!NSEqualRects(native.frame, frame)) [native setFrame:frame display:YES];
    }
}
