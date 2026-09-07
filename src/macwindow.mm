#include <QWindow>
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
    // Observe AppKit directly: Qt size signals arrive before native titlebar
    // layout, and queued corrections let the default position reach the screen.
    // A synchronous did-resize observer runs after layout, before drawing.
    NSMutableArray *observers = [[NSMutableArray alloc] init];
    NSWindow *native = reinterpret_cast<NSView *>(window->winId()).window;
    for (NSNotificationName name in @[NSWindowDidResizeNotification,
                                     NSWindowDidExitFullScreenNotification,
                                     NSWindowDidBecomeKeyNotification]) {
        id observer = [[NSNotificationCenter defaultCenter]
            addObserverForName:name object:native queue:nil
            usingBlock:^(NSNotification *) { align(); }];
        [observers addObject:observer];
    }
    QObject::connect(owner, &QObject::destroyed, [observers] {
        for (id observer in observers)
            [[NSNotificationCenter defaultCenter] removeObserver:observer];
        [observers release];
    });
    align();
}
