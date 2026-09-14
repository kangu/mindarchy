#include "tabshortcuts.h"
#include <QGuiApplication>
#include <objc/runtime.h>
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

namespace {
NSString *tabKey(const QString &action) {
    const auto *command=TabShortcuts::find(action);
    return command ? QString(QChar(command->key==Qt::Key_Tab ? '\t' : command->key)).toLower().toNSString() : @"";
}
NSEventModifierFlags tabModifiers(const QString &action) {
    const auto *command=TabShortcuts::find(action);
    if(!command) return 0;
    const auto modifiers=TabShortcuts::modifiers(*command,true);
    NSEventModifierFlags flags=0;
    if(modifiers.testFlag(Qt::ControlModifier)) flags|=NSEventModifierFlagCommand;
    if(modifiers.testFlag(Qt::MetaModifier)) flags|=NSEventModifierFlagControl;
    if(modifiers.testFlag(Qt::ShiftModifier)) flags|=NSEventModifierFlagShift;
    return flags;
}
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
    // Updating the active document title makes AppKit lay out its title bar
    // again even though the shared window never resizes or changes focus.
    QObject::connect(window, &QWindow::windowTitleChanged, owner, align);
    // Observe AppKit directly: Qt size signals arrive before native titlebar
    // layout, and queued corrections let the default position reach the screen.
    // A synchronous did-resize observer runs after layout, before drawing.
    NSMutableArray *observers = [[NSMutableArray alloc] init];
    NSWindow *native = reinterpret_cast<NSView *>(window->winId()).window;
    NSWindow.allowsAutomaticWindowTabbing=NO;
    native.tabbingIdentifier=@"MindarchyDocuments";
    native.tabbingMode=NSWindowTabbingModeDisallowed;
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
- (void)mergeWindows:(id)sender;
@end
@implementation OMMWindowMenuTarget
- (void)tabCommand:(QString)action {
    if(NSApp.currentEvent.type==NSEventTypeKeyDown && NSApp.currentEvent.isARepeat && action!="next" && action!="previous") return;
    for(auto *window:QGuiApplication::allWindows()) {
        if(window->property("macDocumentWindowId").isNull()) continue;
        if(reinterpret_cast<NSView *>(window->winId()).window!=NSApp.keyWindow) continue;
        QObject *engine=window->property("controller").value<QObject *>();
        if(engine) QMetaObject::invokeMethod(engine,"tabActionRequested",Q_ARG(QString,action),Q_ARG(qint64,0));
        return;
    }
}
- (void)mergeWindows:(id)sender { [self tabCommand:QStringLiteral("merge")]; }
- (void)nextTab:(id)sender { [self tabCommand:QStringLiteral("next")]; }
- (void)previousTab:(id)sender { [self tabCommand:QStringLiteral("previous")]; }
- (void)detachTab:(id)sender { [self tabCommand:QStringLiteral("detach")]; }
- (void)activatePid:(qint64)pid {
    activateWindow(pid);
    bool local=false;
    for(const auto &entry:listWindows()) if(entry.toMap().contains("windowId")) { local=true; break; }
    NSRunningApplication *application = local ? NSRunningApplication.currentApplication : [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if (@available(macOS 14.0, *)) [application activateFromApplication:NSRunningApplication.currentApplication options:0];
    else [application activateWithOptions:0];
}
- (void)selectWindow:(NSMenuItem *)item { [self activatePid:[item.representedObject longLongValue]]; }
- (void)cycle:(int)direction {
    const auto windows = listWindows();
    if (windows.isEmpty()) return;
    int current = 0;
    for (int i=0; i<windows.size(); ++i)
        if (windows[i].toMap().contains("windowId") ? windows[i].toMap().value("current").toBool() : windows[i].toMap().value("pid").toLongLong() == QCoreApplication::applicationPid()) current=i;
    [self activatePid:windows[(current+direction+windows.size())%windows.size()].toMap().value("pid").toLongLong()];
}
- (void)nextWindow:(id)sender { [self cycle:1]; }
- (void)previousWindow:(id)sender { [self cycle:-1]; }
- (void)bringAll:(id)sender {
    for (const auto &window : listWindows()) [self activatePid:window.toMap().value("pid").toLongLong()];
    [self activatePid:QCoreApplication::applicationPid()];
}
- (void)centerWindow:(id)sender { [NSApp.keyWindow center]; }
- (BOOL)validateMenuItem:(NSMenuItem *)item {
    if(item.action==@selector(mergeWindows:)) {
        for(auto *window:QGuiApplication::allWindows()) if(window->isActive()) return window->property("canMergeWindows").toBool();
        return NO;
    }
    if(item.action==@selector(nextWindow:) || item.action==@selector(previousWindow:)) return listWindows().size()>1;
    return YES;
}
- (void)menuNeedsUpdate:(NSMenu *)menu {
    for (NSMenuItem *item in [[menu.itemArray copy] autorelease]) if (item.tag == 9101) [menu removeItem:item];
    const auto windows = listWindows();
    for (const auto &window : windows) {
        const auto map = window.toMap();
        // AppKit supplies the current process's window list itself.
        if (map.contains("windowId") || map.value("pid").toLongLong() == QCoreApplication::applicationPid()) continue;
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
    NSMenuItem *prevTab=add(@"Show Previous Tab", @selector(previousTab:), tabKey("previous"), target);
    prevTab.keyEquivalentModifierMask=tabModifiers("previous");
    NSMenuItem *nextTab=add(@"Show Next Tab", @selector(nextTab:), tabKey("next"), target);
    nextTab.keyEquivalentModifierMask=tabModifiers("next");
    add(@"Move Tab to New Window", @selector(detachTab:), @"", target);
    add(@"Merge All Windows", @selector(mergeWindows:), @"", target);
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

@interface OMMShortcutsWindow : NSWindow
@end
@implementation OMMShortcutsWindow
- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if ((event.modifierFlags & NSEventModifierFlagCommand) &&
        [event.charactersIgnoringModifiers isEqualToString:@"w"]) {
        [self performClose:nil]; return YES;
    }
    return [super performKeyEquivalent:event];
}
@end

@interface OMMHelpTarget : NSObject <NSTableViewDataSource, NSTableViewDelegate, NSSearchFieldDelegate> {
    NSWindow *shortcutsWindow;
    NSArray *rows;
    NSMutableArray *filteredRows;
    NSTableView *shortcutsTable;
}
- (void)showShortcuts:(id)sender;
@end
@implementation OMMHelpTarget
- (NSString *)searchableText:(NSString *)text {
    NSString *result=text.lowercaseString;
    for (NSArray *pair in @[@[@"⌘",@" command "], @[@"cmd",@"command"], @[@"⇧",@" shift "],
                           @[@"⌃",@" control "], @[@"ctrl",@"control"], @[@"⌫",@" backspace "],
                           @[@"↑",@" up arrow "], @[@"↓",@" down arrow "],
                           @[@"←",@" left arrow "], @[@"→",@" right arrow "], @[@"−",@"-"]])
        result=[result stringByReplacingOccurrencesOfString:pair[0] withString:pair[1]];
    return result;
}
- (void)controlTextDidChange:(NSNotification *)notification {
    NSString *query=[notification.object stringValue];
    NSString *normalized=[self searchableText:query];
    if (normalized.length>1) normalized=[normalized stringByReplacingOccurrencesOfString:@"+" withString:@" "];
    NSArray *terms=[normalized componentsSeparatedByCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
    [filteredRows removeAllObjects];
    for(NSArray *row in rows) {
        NSString *haystack=[self searchableText:[row componentsJoinedByString:@" "]];
        BOOL matches=YES;
        for(NSString *term in terms)
            if(term.length && [haystack rangeOfString:term options:NSCaseInsensitiveSearch|NSDiacriticInsensitiveSearch].location==NSNotFound) { matches=NO; break; }
        if(matches) [filteredRows addObject:row];
    }
    [shortcutsTable reloadData];
    if(filteredRows.count) [shortcutsTable scrollRowToVisible:0];
}
- (NSInteger)numberOfRowsInTableView:(NSTableView *)table { return filteredRows.count; }
- (NSView *)tableView:(NSTableView *)table viewForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
    NSInteger index=[column.identifier integerValue];
    NSTableCellView *cell=[[[NSTableCellView alloc] init] autorelease];
    NSTextField *label=[NSTextField labelWithString:filteredRows[row][index]];
    label.font=index==2 ? [NSFont monospacedSystemFontOfSize:13 weight:NSFontWeightMedium] : [NSFont systemFontOfSize:13];
    label.textColor=index==0 ? NSColor.secondaryLabelColor : NSColor.labelColor;
    label.lineBreakMode=NSLineBreakByTruncatingTail;
    label.translatesAutoresizingMaskIntoConstraints=NO;
    [cell addSubview:label]; cell.textField=label;
    [NSLayoutConstraint activateConstraints:@[
        [label.centerYAnchor constraintEqualToAnchor:cell.centerYAnchor],
        [label.leadingAnchor constraintEqualToAnchor:cell.leadingAnchor constant:2],
        [label.trailingAnchor constraintEqualToAnchor:cell.trailingAnchor constant:-2]
    ]];
    return cell;
}
- (void)showShortcuts:(id)sender {
    if (!shortcutsWindow) {
        rows=[@[
            @[@"Document", @"Open document", @"⌘ O"],
            @[@"Document", @"Save", @"⌘ S"],
            @[@"Document", @"Quit application", @"⌘ Q"],
            @[@"Document", @"Undo", @"⌘ Z"],
            @[@"Document", @"Redo", @"⇧ ⌘ Z"],
            @[@"Canvas", @"Copy selected branches", @"⌘ C"],
            @[@"Images", @"Copy / cut selected image; paste onto selected node", @"⌘ C / ⌘ X / ⌘ V"],
            @[@"Canvas", @"Paste branches or outline as children", @"⌘ V"],
            @[@"Canvas", @"Connect two selected nodes", @"⌘ L"],
            @[@"Canvas", @"Toggle branch Focus mode", @"⇧ ⌘ F"],
            @[@"Canvas", @"Exit Focus and restore viewport", @"Esc"],
            @[@"Canvas", @"Navigate nodes", @"↑ ↓ ← →"],
            @[@"Canvas", @"Extend selection", @"⇧ + arrows"],
            @[@"Canvas", @"Move viewport in arrow direction", @"⌘ + arrows"],
            @[@"Canvas", @"Pan with mouse", @"Space + drag"],
            @[@"Canvas", @"Create child", @"Tab"],
            @[@"Canvas", @"Create sibling (child of root)", @"Return"],
            @[@"Canvas", @"Edit selected node", @"⌘ Return / F2"],
            @[@"Canvas", @"Delete selected branch", @"Delete / ⌫"],
            @[@"Images", @"Remove selected image, keeping its node", @"Delete / ⌫"],
            @[@"Canvas", @"Replace selected title", @"Type text"],
            @[@"Canvas", @"Fold or expand branch", @"⌥ F"],
            @[@"Canvas", @"Toggle Task node type", @"⌥ T"],
            @[@"Canvas (no selection)", @"Zoom in", @"+ / ="],
            @[@"Canvas (no selection)", @"Zoom out", @"−"],
            @[@"Canvas (no selection)", @"Fit map", @"0"],
            @[@"Search", @"Open mind map search", @"⌘ F"],
            @[@"Search", @"Next result after automatic first match", @"Return"],
            @[@"Search", @"Close search", @"Esc"],
            @[@"Canvas", @"Clear selection / cancel child drag", @"Esc"],
            @[@"Selected image", @"Open image preview", @"Space"],
            @[@"Image preview", @"Close preview", @"Space / Esc"],
            @[@"Image resizing", @"Cancel resize", @"Esc"],
            @[@"Node editing", @"Commit text", @"Return / Esc"],
            @[@"Node editing", @"Commit and create child", @"Tab"],
            @[@"Node editing", @"Insert line break", @"⇧ Return"],
            @[@"Node editing", @"Bold", @"⌘ B"],
            @[@"Node editing", @"Italic", @"⌘ I"],
            @[@"Node editing", @"Underline", @"⌘ U"],
            @[@"Text fields", @"Select all", @"⌘ A"],
            @[@"Text fields", @"Cut / copy / paste", @"⌘ X / C / V"],
            @[@"Date entry", @"Save entry", @"⌘ Return"],
            @[@"Date entry", @"Cancel entry", @"Esc"],
            @[@"Window", @"Minimize", @"⌘ M"],
            @[@"Window", @"Toggle full screen", @"⌃ ⌘ F"],
            @[@"Window", @"Next window", @"⌘ `"],
            @[@"Window", @"Previous window", @"⇧ ⌘ `"]
        ] retain];
        NSMutableArray *allRows=[NSMutableArray array];
        for(const auto &entry:TabShortcuts::help()) {
            const auto row=entry.toMap();
            [allRows addObject:@[@"Document tabs",row.value("label").toString().toNSString(),row.value("shortcut").toString().toNSString()]];
        }
        [allRows addObjectsFromArray:rows]; [rows release]; rows=[allRows copy];
        filteredRows=[rows mutableCopy];
        shortcutsWindow=[[OMMShortcutsWindow alloc] initWithContentRect:NSMakeRect(0,0,740,600)
            styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        shortcutsWindow.title=@"Keyboard Shortcuts";
        shortcutsWindow.identifier=@"mindarchy-keyboard-shortcuts";
        shortcutsWindow.releasedWhenClosed=NO;
        shortcutsWindow.contentMinSize=NSMakeSize(680,360);
        NSView *content=shortcutsWindow.contentView;
        NSTextField *heading=[NSTextField labelWithString:@"Keyboard Shortcuts"];
        heading.font=[NSFont systemFontOfSize:23 weight:NSFontWeightSemibold];
        NSTextField *subtitle=[NSTextField labelWithString:@"Canvas shortcuts work while the mind map is focused. Text fields keep their editing shortcuts."];
        subtitle.font=[NSFont systemFontOfSize:12]; subtitle.textColor=NSColor.secondaryLabelColor;
        NSSearchField *search=[[[NSSearchField alloc] init] autorelease];
        search.placeholderString=@"Search actions or keys";
        search.accessibilityLabel=@"Search keyboard shortcuts";
        search.identifier=@"shortcut-search";
        search.delegate=self;
        search.sendsSearchStringImmediately=YES;
        NSScrollView *scroll=[[[NSScrollView alloc] init] autorelease];
        scroll.hasVerticalScroller=YES; scroll.autohidesScrollers=YES;
        NSTableView *table=[[[NSTableView alloc] init] autorelease];
        shortcutsTable=table;
        NSArray *titles=@[@"Context", @"Action", @"Shortcut"];
        for (NSInteger i=0;i<3;++i) {
            NSTableColumn *column=[[[NSTableColumn alloc] initWithIdentifier:[@(i) stringValue]] autorelease];
            column.title=titles[i]; column.width=i==0 ? 115 : i==1 ? 345 : 210;
            column.minWidth=i==0 ? 100 : i==1 ? 280 : 180;
            [table addTableColumn:column];
        }
        table.usesAlternatingRowBackgroundColors=YES; table.rowHeight=32;
        table.selectionHighlightStyle=NSTableViewSelectionHighlightStyleNone;
        table.dataSource=self; table.delegate=self;
        scroll.documentView=table;
        for(NSView *child in @[heading,subtitle,search,scroll]) { child.translatesAutoresizingMaskIntoConstraints=NO; [content addSubview:child]; }
        [NSLayoutConstraint activateConstraints:@[
            [heading.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:24],
            [heading.topAnchor constraintEqualToAnchor:content.topAnchor constant:22],
            [subtitle.leadingAnchor constraintEqualToAnchor:heading.leadingAnchor],
            [subtitle.topAnchor constraintEqualToAnchor:heading.bottomAnchor constant:8],
            [search.leadingAnchor constraintEqualToAnchor:heading.leadingAnchor],
            [search.topAnchor constraintEqualToAnchor:subtitle.bottomAnchor constant:14],
            [search.widthAnchor constraintEqualToConstant:280],
            [search.heightAnchor constraintEqualToConstant:26],
            [scroll.topAnchor constraintEqualToAnchor:search.bottomAnchor constant:14],
            [scroll.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:16],
            [scroll.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-16],
            [scroll.bottomAnchor constraintEqualToAnchor:content.bottomAnchor constant:-16]
        ]];
        [shortcutsWindow center];
    }
    [shortcutsWindow makeKeyAndOrderFront:nil];
}
- (void)dealloc {
    [shortcutsWindow close]; [shortcutsWindow release]; [filteredRows release]; [rows release]; [super dealloc];
}
@end

void installMacHelpMenu(QWindow *owner) {
    NSMenu *menu=[[[NSMenu alloc] initWithTitle:@"Help"] autorelease];
    NSMenuItem *root=[[[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""] autorelease];
    OMMHelpTarget *target=[[OMMHelpTarget alloc] init];
    NSMenuItem *item=[[[NSMenuItem alloc] initWithTitle:@"Keyboard Shortcuts…" action:@selector(showShortcuts:) keyEquivalent:@""] autorelease];
    item.target=target; [menu addItem:item]; root.submenu=menu;
    [NSApp.mainMenu addItem:root]; NSApp.helpMenu=menu;
    QObject::connect(owner,&QObject::destroyed,[target,root,menu] {
        if(NSApp.helpMenu==menu) NSApp.helpMenu=nil;
        [NSApp.mainMenu removeItem:root]; [target release];
    });
}

@interface OMMFileMenuTarget : NSObject <NSMenuDelegate> {
@public
    std::function<void(QString,QString)> command;
    std::function<QVariantList()> recentFiles;
}
- (void)invoke:(NSMenuItem *)item;
@end
@implementation OMMFileMenuTarget
- (void)invoke:(NSMenuItem *)item {
    NSDictionary *value=item.representedObject;
    if(NSApp.currentEvent.type==NSEventTypeKeyDown && NSApp.currentEvent.isARepeat && ([@[@"new",@"newtab",@"close"] containsObject:value[@"action"]])) return;
    command(QString::fromNSString(value[@"action"]),QString::fromNSString(value[@"path"] ?: @""));
}
- (void)menuNeedsUpdate:(NSMenu *)menu {
    [menu removeAllItems];
    const auto files=recentFiles();
    if(files.isEmpty()) {
        NSMenuItem *empty=[[[NSMenuItem alloc] initWithTitle:@"No Recent Files" action:nil keyEquivalent:@""] autorelease];
        empty.enabled=NO; [menu addItem:empty];
    }
    for(const auto &value:files) {
        const auto entry=value.toMap(); const auto path=entry["path"].toString();
        const auto title=entry["name"].toString()+" — "+QFileInfo(path).absolutePath();
        NSMenuItem *item=[[[NSMenuItem alloc] initWithTitle:title.toNSString() action:@selector(invoke:) keyEquivalent:@""] autorelease];
        item.target=self; item.representedObject=@{@"action":@"recent",@"path":path.toNSString()};
        item.toolTip=path.toNSString(); item.enabled=entry["available"].toBool(); [menu addItem:item];
    }
    [menu addItem:NSMenuItem.separatorItem];
    NSMenuItem *clear=[[[NSMenuItem alloc] initWithTitle:@"Clear Menu" action:@selector(invoke:) keyEquivalent:@""] autorelease];
    clear.target=self; clear.representedObject=@{@"action":@"clear"}; clear.enabled=!files.isEmpty(); [menu addItem:clear];
}
@end

void installMacFileMenu(QWindow *owner,std::function<void(QString,QString)> command,std::function<QVariantList()> recent) {
    OMMFileMenuTarget *target=[[OMMFileMenuTarget alloc] init]; target->command=std::move(command); target->recentFiles=std::move(recent);
    NSMenu *menu=[[[NSMenu alloc] initWithTitle:@"File"] autorelease];
    NSMenuItem *root=[[[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""] autorelease]; root.submenu=menu;
    auto add=[&](NSString *title,NSString *key,NSString *action) {
        NSMenuItem *item=[[[NSMenuItem alloc] initWithTitle:title action:@selector(invoke:) keyEquivalent:key] autorelease];
        const QString command=[action isEqualToString:@"newtab"] ? QStringLiteral("new") : [action isEqualToString:@"new"] ? QStringLiteral("window") : QString::fromNSString(action);
        if(TabShortcuts::find(command)) { item.keyEquivalent=tabKey(command); item.keyEquivalentModifierMask=tabModifiers(command); }
        item.target=target; item.representedObject=@{@"action":action}; [menu addItem:item];
    };
    add(@"New",@"n",@"new"); add(@"New Tab",@"t",@"newtab"); add(@"Open…",@"o",@"open");
    NSMenuItem *recentRoot=[[[NSMenuItem alloc] initWithTitle:@"Open Recent" action:nil keyEquivalent:@""] autorelease];
    NSMenu *recentMenu=[[[NSMenu alloc] initWithTitle:@"Open Recent"] autorelease];
    recentMenu.autoenablesItems=NO; recentMenu.delegate=target; recentRoot.submenu=recentMenu; [menu addItem:recentRoot];
    add(@"Save",@"s",@"save"); [menu addItem:NSMenuItem.separatorItem]; add(@"Close Tab",@"w",@"close");
    [NSApp.mainMenu insertItem:root atIndex:MIN(1,NSApp.mainMenu.numberOfItems)];
    QObject::connect(owner,&QObject::destroyed,[root,target] { [NSApp.mainMenu removeItem:root]; [target release]; });
}

// Qt owns the application delegate. Keep AppKit's standard Dock document list
// (fed by NSApp.windowsMenu) and extend only reopening after the final window.
static std::function<void()> reopenDocuments;
static BOOL mindarchyReopen(id,SEL,NSApplication *,BOOL) {
    if(reopenDocuments) reopenDocuments();
    return YES;
}
void installMacReopenHandler(QWindow *owner,std::function<void()> reopen) {
    reopenDocuments=std::move(reopen);
    Class cls=object_getClass(NSApp.delegate);
    SEL selector=@selector(applicationShouldHandleReopen:hasVisibleWindows:);
    IMP previous=class_getInstanceMethod(cls,selector)?class_getMethodImplementation(cls,selector):nullptr;
    class_replaceMethod(cls,selector,(IMP)mindarchyReopen,"B@:@B");
    QObject::connect(owner,&QObject::destroyed,[cls,selector,previous] {
        if(previous) class_replaceMethod(cls,selector,previous,"B@:@B");
        reopenDocuments={};
    });
}
