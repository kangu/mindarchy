#include <QWindow>
#include <QTimer>
#import <AppKit/AppKit.h>

// Retain AppKit's real buttons and their native menus/fullscreen behavior.
void installMacToolbar(QWindow *window) {
    auto *owner = new QObject(window);
    auto align = [window] {
        NSView *view = reinterpret_cast<NSView *>(window->winId());
        NSWindow *native = view.window;
        native.titleVisibility = NSWindowTitleHidden;
        native.titlebarAppearsTransparent = YES;
        if (native.styleMask & NSWindowStyleMaskFullScreen) return;
        NSButton *close = [native standardWindowButton:NSWindowCloseButton];
        NSView *titlebar = close.superview;
        NSRect frame = titlebar.frame;
        frame.origin.y += frame.size.height - 60;
        frame.size.height = 60;
        titlebar.frame = frame;
        int index = 0;
        for (NSNumber *kind in @[@(NSWindowCloseButton), @(NSWindowMiniaturizeButton), @(NSWindowZoomButton)]) {
            NSButton *button = [native standardWindowButton:(NSWindowButton)kind.integerValue];
            NSRect buttonFrame = button.frame;
            buttonFrame.origin.x = 18 + 20 * index++;
            buttonFrame.origin.y = (60 - buttonFrame.size.height) / 2;
            button.frame = buttonFrame;
        }
    };
    auto schedule = [owner, align] {
        QTimer::singleShot(0, owner, align);
        // Native zoom/fullscreen animations can lay out the titlebar after Qt.
        QTimer::singleShot(250, owner, align);
    };
    QObject::connect(window, &QWindow::widthChanged, owner, schedule);
    QObject::connect(window, &QWindow::heightChanged, owner, schedule);
    QObject::connect(window, &QWindow::visibilityChanged, owner, schedule);
    // AppKit finishes its own titlebar layout after fullscreen transitions.
    id observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:NSWindowDidExitFullScreenNotification
        object:reinterpret_cast<NSView *>(window->winId()).window queue:nil
        usingBlock:^(NSNotification *) { schedule(); }];
    QObject::connect(owner, &QObject::destroyed, [observer] {
        [[NSNotificationCenter defaultCenter] removeObserver:observer];
    });
    schedule();
}
