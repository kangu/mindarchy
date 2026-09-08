#include <QGuiApplication>
#include <QWindow>
#include <cmath>
#include <functional>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#import <AppKit/AppKit.h>

void installMacToolbar(QWindow *window);
void showMacCloseConfirmation(QWindow *, const QString &, std::function<void(int)>);

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
    for (int choice : {0, 1, 2}) {
        int result = -1;
        showMacCloseConfirmation(&window, "Native dialog test", [&](int value) { result = value; });
        app.processEvents();
        NSWindow *sheet = native.attachedSheet;
        if (!sheet) { fprintf(stderr, "Native close sheet was not attached\n"); return 3; }
        const NSModalResponse response = choice == 1 ? NSAlertFirstButtonReturn
            : choice == 2 ? NSAlertSecondButtonReturn : NSAlertThirdButtonReturn;
        [native endSheet:sheet returnCode:response];
        QElapsedTimer timer; timer.start();
        while (result < 0 && timer.elapsed() < 3000) app.processEvents();
        if (result != choice || !window.isVisible()) return 4;
    }
    fprintf(stdout, "Native close sheet: Save, Don't Save, and Cancel callbacks verified.\n");
    QTemporaryDir directory;
    id<NSOpenSavePanelDelegate> delegate = [[NSClassFromString(@"OMMSavePanelDelegate") alloc] init];
    if (!delegate) return 5;
    for (const QString &name : {QString("existing.omm"), QString("uppercase.OMM"), QString("unrelated.json")}) {
        QFile file(directory.filePath(name));
        if (!file.open(QIODevice::WriteOnly)) return 6;
        file.write("{}"); file.close();
        NSURL *url = [NSURL fileURLWithPath:file.fileName().toNSString()];
        bool enabled = [delegate panel:native shouldEnableURL:url];
        if (enabled != name.endsWith("omm", Qt::CaseInsensitive)) return 7;
    }
    if (![delegate panel:native shouldEnableURL:[NSURL fileURLWithPath:directory.path().toNSString()]]) return 8;
    [(NSObject *)delegate release];
    fprintf(stdout, "Native save filter enables .omm/.OMM and folders, excludes unrelated files.\n");
    return 0;
}
