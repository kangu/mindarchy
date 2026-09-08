#include <QWindow>
#include <QVariantList>
#include <QCoreApplication>
#include <QFileInfo>
#include <functional>
#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface OMMFolderMenuTarget : NSObject
- (void)openFolder:(NSMenuItem *)item;
@end
@implementation OMMFolderMenuTarget
- (void)openFolder:(NSMenuItem *)item {
    [NSWorkspace.sharedWorkspace openURL:item.representedObject];
}
@end

NSMenu *createMacParentFolderMenu(const QString &file) {
    static OMMFolderMenuTarget *target = [[OMMFolderMenuTarget alloc] init];
    NSMenu *menu = [[[NSMenu alloc] initWithTitle:@"Document location"] autorelease];
    NSString *path = QFileInfo(file).absolutePath().toNSString();
    while (path.length) {
        NSString *name = [NSFileManager.defaultManager displayNameAtPath:path];
        NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:name action:@selector(openFolder:) keyEquivalent:@""] autorelease];
        item.target = target;
        item.representedObject = [NSURL fileURLWithPath:path isDirectory:YES];
        NSImage *icon = [[[NSWorkspace.sharedWorkspace iconForFile:path] copy] autorelease];
        icon.size = NSMakeSize(16, 16); item.image = icon;
        [menu addItem:item];
        NSString *parent = [path stringByDeletingLastPathComponent];
        if ([parent isEqualToString:path]) break;
        path = parent;
    }
    return menu;
}

void showMacParentFolderMenu(QWindow *window, const QString &file, double x, double y) {
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSPoint position = NSMakePoint(x, view.isFlipped ? y : NSHeight(view.bounds)-y);
    [createMacParentFolderMenu(file) popUpMenuPositioningItem:nil atLocation:position inView:view];
}

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

@interface OMMWindowMenuTarget : NSObject <NSMenuDelegate> {
@public
    std::function<QVariantList()> listWindows;
    std::function<void(qint64)> activateWindow;
}
- (void)selectWindow:(NSMenuItem *)item;
- (void)nextWindow:(id)sender;
- (void)previousWindow:(id)sender;
- (void)bringAll:(id)sender;
- (void)centerWindow:(id)sender;
@end
@implementation OMMWindowMenuTarget
- (void)activatePid:(qint64)pid {
    activateWindow(pid);
    NSRunningApplication *application = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if (@available(macOS 14.0, *)) [application activateFromApplication:NSRunningApplication.currentApplication options:0];
    else [application activateWithOptions:0];
}
- (void)selectWindow:(NSMenuItem *)item { [self activatePid:[item.representedObject longLongValue]]; }
- (void)cycle:(int)direction {
    const auto windows = listWindows();
    if (windows.isEmpty()) return;
    int current = 0;
    for (int i=0; i<windows.size(); ++i)
        if (windows[i].toMap().value("pid").toLongLong() == QCoreApplication::applicationPid()) current=i;
    [self activatePid:windows[(current+direction+windows.size())%windows.size()].toMap().value("pid").toLongLong()];
}
- (void)nextWindow:(id)sender { [self cycle:1]; }
- (void)previousWindow:(id)sender { [self cycle:-1]; }
- (void)bringAll:(id)sender {
    for (const auto &window : listWindows()) [self activatePid:window.toMap().value("pid").toLongLong()];
    [self activatePid:QCoreApplication::applicationPid()];
}
- (void)centerWindow:(id)sender { [NSApp.keyWindow center]; }
- (void)menuNeedsUpdate:(NSMenu *)menu {
    for (NSMenuItem *item in [[menu.itemArray copy] autorelease]) if (item.tag == 9101) [menu removeItem:item];
    const auto windows = listWindows();
    for (const auto &window : windows) {
        const auto map = window.toMap();
        // AppKit supplies the current process's window list itself.
        if (map.value("pid").toLongLong() == QCoreApplication::applicationPid()) continue;
        NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:map.value("title").toString().toNSString()
            action:@selector(selectWindow:) keyEquivalent:@""] autorelease];
        item.target=self; item.tag=9101; item.representedObject=@(map.value("pid").toLongLong());
        [menu addItem:item];
    }
}
@end

void installMacWindowMenu(QWindow *window, std::function<QVariantList()> list,
                          std::function<void(qint64)> activate) {
    NSMenu *main = NSApp.mainMenu;
    if (!main) { main = [[[NSMenu alloc] initWithTitle:@""] autorelease]; NSApp.mainMenu=main; }
    OMMWindowMenuTarget *target = [[OMMWindowMenuTarget alloc] init];
    target->listWindows=std::move(list); target->activateWindow=std::move(activate);
    // Qt already registers a hidden Window menu for Dock window listings.
    // Reuse that native menu instead of competing with its registration.
    NSMenu *menu = NSApp.windowsMenu;
    NSMenuItem *root = nil;
    for (NSMenuItem *item in main.itemArray) if (item.submenu == menu) { root=item; break; }
    if (!menu) menu = [[[NSMenu alloc] initWithTitle:@"Window"] autorelease];
    [menu removeAllItems];
    menu.delegate=target;
    auto add = [&](NSString *title, SEL action, NSString *key, id receiver) {
        NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key] autorelease];
        item.target=receiver; [menu addItem:item]; return item;
    };
    add(@"Minimize", @selector(performMiniaturize:), @"m", nil);
    add(@"Zoom", @selector(performZoom:), @"", nil);
    add(@"Center", @selector(centerWindow:), @"", target);
    NSMenuItem *fullscreen=add(@"Enter Full Screen", @selector(toggleFullScreen:), @"f", nil);
    fullscreen.keyEquivalentModifierMask=NSEventModifierFlagControl|NSEventModifierFlagCommand;
    [menu addItem:NSMenuItem.separatorItem];
    add(@"Next Window", @selector(nextWindow:), @"`", target);
    NSMenuItem *previous=add(@"Previous Window", @selector(previousWindow:), @"`", target);
    previous.keyEquivalentModifierMask=NSEventModifierFlagCommand|NSEventModifierFlagShift;
    [menu addItem:NSMenuItem.separatorItem];
    add(@"Bring All to Front", @selector(bringAll:), @"", target);
    if (!root) {
        root = [[[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""] autorelease];
        root.submenu=menu;
        NSInteger index=main.numberOfItems;
        for (NSInteger i=0; i<main.numberOfItems; ++i) if ([[main itemAtIndex:i].title isEqualToString:@"Help"]) { index=i; break; }
        [main insertItem:root atIndex:index];
    }
    root.hidden=NO; root.title=@"Window"; menu.title=@"Window";
    if (NSApp.windowsMenu != menu) NSApp.windowsMenu=menu;
    for (NSWindow *native in NSApp.windows) {
        native.excludedFromWindowsMenu = !native.excludedFromWindowsMenu;
        native.excludedFromWindowsMenu = !native.excludedFromWindowsMenu;
    }
    QObject::connect(window, &QObject::destroyed, [target, menu, root] {
        menu.delegate=nil;
        if (NSApp.windowsMenu == menu) NSApp.windowsMenu=nil;
        [NSApp.mainMenu removeItem:root];
        [target release];
    });
}
