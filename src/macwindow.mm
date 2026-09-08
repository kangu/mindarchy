#include <QWindow>
#include <functional>
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface OMMSavePanelDelegate : NSObject <NSOpenSavePanelDelegate>
@end
@implementation OMMSavePanelDelegate
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url {
    NSNumber *directory = nil;
    [url getResourceValue:&directory forKey:NSURLIsDirectoryKey error:nil];
    return directory.boolValue || [url.pathExtension caseInsensitiveCompare:@"omm"] == NSOrderedSame;
}
@end

void showMacSavePanel(QWindow *window, const QString &name, std::function<void(QString)> completion) {
    NSSavePanel *panel = [NSSavePanel savePanel];
    OMMSavePanelDelegate *delegate = [[OMMSavePanelDelegate alloc] init];
    panel.delegate = delegate;
    UTType *type = [UTType typeWithFilenameExtension:@"omm" conformingToType:UTTypeJSON];
    panel.allowedContentTypes = type ? @[type] : @[];
    panel.allowsOtherFileTypes = NO;
    panel.canCreateDirectories = YES;
    panel.extensionHidden = NO;
    panel.nameFieldStringValue = (name + ".omm").toNSString();
    panel.title = @"Save mindmap";
    [panel beginSheetModalForWindow:reinterpret_cast<NSView *>(window->winId()).window
        completionHandler:^(NSModalResponse response) {
            const QString path = response == NSModalResponseOK ? QString::fromNSString(panel.URL.path) : QString();
            [panel orderOut:nil];
            panel.delegate = nil;
            [delegate release];
            completion(path);
        }];
}

// 0 = cancel, 1 = save, 2 = discard. AppKit owns the sheet interaction.
void showMacCloseConfirmation(QWindow *window, const QString &name,
                              std::function<void(int)> completion) {
    NSWindow *native = reinterpret_cast<NSView *>(window->winId()).window;
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleInformational;
    alert.icon = NSApp.applicationIconImage;
    alert.messageText = [NSString stringWithFormat:@"Do you want to save the changes made to “%@”?", name.toNSString()];
    alert.informativeText = @"Your changes will be lost if you don’t save them.";
    [alert addButtonWithTitle:@"Save"];
    NSButton *discard = [alert addButtonWithTitle:@"Don’t Save"];
    discard.hasDestructiveAction = YES;
    discard.keyEquivalent = @"d";
    discard.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    NSButton *cancel = [alert addButtonWithTitle:@"Cancel"];
    cancel.keyEquivalent = @"\033";
    [alert beginSheetModalForWindow:native completionHandler:^(NSModalResponse response) {
        completion(response == NSAlertFirstButtonReturn ? 1 : response == NSAlertSecondButtonReturn ? 2 : 0);
    }];
    [alert release];
}

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
