#include <QGuiApplication>
#include <QWindow>
#include <cmath>
#import <AppKit/AppKit.h>

void installMacToolbar(QWindow *window);

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QWindow window;
    window.setFlags(Qt::Window | Qt::ExpandedClientAreaHint | Qt::NoTitleBarBackgroundHint);
    window.resize(900, 650);
    window.show();
    app.processEvents();
    installMacToolbar(&window);
    app.processEvents();
    NSView *view = reinterpret_cast<NSView *>(window.winId());
    NSWindow *native = view.window;
    for (int step = 0; step < 120; ++step) {
        NSRect frame = native.frame;
        frame.size = NSMakeSize(800 + (step % 30) * 8, 600 + (step % 20) * 6);
        [native setFrame:frame display:YES];
        // No event-loop turn or delayed repair is allowed before checking.
        int index = 0;
        for (NSNumber *kind in @[@(NSWindowCloseButton), @(NSWindowMiniaturizeButton), @(NSWindowZoomButton)]) {
            NSButton *button = [native standardWindowButton:(NSWindowButton)kind.integerValue];
            NSRect rect = [view convertRect:button.bounds fromView:button];
            double top = view.isFlipped ? NSMidY(rect) : NSHeight(view.bounds) - NSMidY(rect);
            if (std::abs(NSMinX(rect) - (18 + 20 * index)) > 1 || std::abs(top - 30) > 1) {
                fprintf(stderr, "Resize %d button %d jumped: x=%.1f center-from-top=%.1f\n",
                        step, index, NSMinX(rect), top);
                return 1;
            }
            ++index;
        }
        if (native.titleVisibility != NSWindowTitleHidden) return 2;
        app.processEvents();
    }
    fprintf(stdout, "120 native resizes: all three controls remained aligned, title hidden.\n");
    return 0;
}
