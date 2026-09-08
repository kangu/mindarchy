#include <QWindow>
#import <AppKit/AppKit.h>

// Exercise AppKit's actual zoom action as well as QWindow::showMaximized.
void nativeZoomForTest(QWindow *window) {
    [reinterpret_cast<NSView *>(window->winId()).window zoom:nil];
}

bool nativeZoomedForTest(QWindow *window) {
    return reinterpret_cast<NSView *>(window->winId()).window.isZoomed;
}
