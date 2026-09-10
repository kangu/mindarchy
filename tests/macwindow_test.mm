#include <QGuiApplication>
#include <QWindow>
#include <cmath>
#include <functional>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QVariantList>
#import <AppKit/AppKit.h>

void installMacFileMenu(QWindow *,std::function<void(QString,QString)>,std::function<QVariantList()>);
void installMacToolbar(QWindow *window);
void installMacHelpMenu(QWindow *);
void showMacCloseConfirmation(QWindow *, const QString &, std::function<void(int)>);
NSMenu *createMacParentFolderMenu(const QString &);
void installMacWindowMenu(QWindow *, std::function<QVariantList()>, std::function<void(qint64)>);

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
    NSMenu *folders = createMacParentFolderMenu(directory.filePath("existing.omm"));
    QString expected = directory.path();
    for (NSMenuItem *item in folders.itemArray) {
        NSURL *url = item.representedObject;
        if (QString::fromNSString(url.path) != expected || !item.image || !item.target ||
            item.action != @selector(openFolder:)) return 9;
        expected = QFileInfo(expected).absolutePath();
    }
    if (QString::fromNSString([(NSURL *)folders.itemArray.lastObject.representedObject path]) != "/") return 10;
    fprintf(stdout, "Native parent-folder menu contains every ancestor through the volume root.\n");
    qint64 activated = 0;
    installMacWindowMenu(&window, [] {
        return QVariantList{QVariantMap{{"pid", QCoreApplication::applicationPid()}, {"title", "Current map"}},
            QVariantMap{{"pid", qint64(999999)}, {"title", "Second map"}}};
    }, [&](qint64 pid) { activated=pid; });
    NSMenu *windowMenu=NSApp.windowsMenu;
    if (!windowMenu || ![NSApp.mainMenu itemWithTitle:@"Window"]) return 11;
    [windowMenu.delegate menuNeedsUpdate:windowMenu];
    NSMenuItem *next=[windowMenu itemWithTitle:@"Next Window"];
    NSMenuItem *previous=[windowMenu itemWithTitle:@"Previous Window"];
    if (!next || !previous || ![next.keyEquivalent isEqualToString:@"`"] ||
        previous.keyEquivalentModifierMask != (NSEventModifierFlagCommand|NSEventModifierFlagShift)) return 12;
    [NSApp sendAction:next.action to:next.target from:next];
    if (activated != 999999) return 13;
    if (![windowMenu itemWithTitle:@"Second map"] || ![windowMenu itemWithTitle:@"Minimize"]) return 14;
    fprintf(stdout, "Window menu: native registration, cross-process listing and cycle shortcut verified.\n");
    QString fileCommand,chosenPath;
    QVariantList recentFiles{QVariantMap{{"name","Example"},{"path","/tmp/example.omm"},{"available",true}},
        QVariantMap{{"name","Missing"},{"path","/tmp/missing.omm"},{"available",false}}};
    installMacFileMenu(&window,[&](QString command,QString path) { fileCommand=command; chosenPath=path; if(command=="clear") recentFiles.clear(); },[&] {return recentFiles;});
    NSMenu *fileMenu=[NSApp.mainMenu itemWithTitle:@"File"].submenu;
    if(!fileMenu || fileMenu.numberOfItems!=6) return 24;
    for(NSString *title in @[@"New",@"Open…",@"Save",@"Close Window"]) {
        NSMenuItem *item=[fileMenu itemWithTitle:title]; if(!item || !item.keyEquivalent.length) return 25;
        fileCommand.clear(); [NSApp sendAction:item.action to:item.target from:item]; if(fileCommand.isEmpty()) return 26;
    }
    NSMenu *recentMenu=[fileMenu itemWithTitle:@"Open Recent"].submenu;
    [recentMenu.delegate menuNeedsUpdate:recentMenu];
    if(recentMenu.numberOfItems!=4 || !recentMenu.itemArray[0].enabled || recentMenu.itemArray[1].enabled) return 27;
    NSMenuItem *recentItem=recentMenu.itemArray[0]; [NSApp sendAction:recentItem.action to:recentItem.target from:recentItem];
    if(fileCommand!="recent" || chosenPath!="/tmp/example.omm") return 28;
    NSMenuItem *clear=[recentMenu itemWithTitle:@"Clear Menu"]; [NSApp sendAction:clear.action to:clear.target from:clear];
    [recentMenu.delegate menuNeedsUpdate:recentMenu];
    if(![recentMenu itemWithTitle:@"No Recent Files"] || [recentMenu itemWithTitle:@"Clear Menu"].enabled) return 29;
    fprintf(stdout,"File menu: standard commands, recent-file dispatch, unavailable items, and Clear Menu verified.\n");
    installMacHelpMenu(&window);
    NSMenu *help=NSApp.helpMenu;
    NSMenuItem *shortcuts=[help itemWithTitle:@"Keyboard Shortcuts…"];
    if(!shortcuts || ![NSApp.mainMenu itemWithTitle:@"Help"]) return 15;
    [NSApp sendAction:shortcuts.action to:shortcuts.target from:shortcuts];
    app.processEvents();
    NSWindow *reference=nil;
    for(NSWindow *candidate in NSApp.windows)
        if([candidate.identifier isEqualToString:@"mindarchy-keyboard-shortcuts"]) reference=candidate;
    if(!reference || !reference.visible || reference==native) return 16;
    NSTableView *table=nil;
    for(NSView *child in reference.contentView.subviews)
        if([child isKindOfClass:NSScrollView.class]) table=(NSTableView *)[(NSScrollView *)child documentView];
    if(!table || table.numberOfRows<30 || table.numberOfColumns!=3) return 17;
    [reference.contentView layoutSubtreeIfNeeded];
    for(NSInteger column=0;column<3;++column) {
        NSTableCellView *cell=[table viewAtColumn:column row:0 makeIfNecessary:YES];
        [cell layoutSubtreeIfNeeded];
        if(!cell.textField || std::abs(NSMidY(cell.textField.frame)-NSMidY(cell.bounds))>1) return 21;
    }
    NSSearchField *search=nil;
    for(NSView *child in reference.contentView.subviews)
        if([child isKindOfClass:NSSearchField.class]) search=(NSSearchField *)child;
    if(!search) return 22;
    const NSInteger totalRows=table.numberOfRows;
    for(NSString *query in @[@"zoom",@"Cmd+Return",@"⌘ Return",@"unlikely-no-match",@""]) {
        search.stringValue=query;
        [search.delegate controlTextDidChange:[NSNotification notificationWithName:NSControlTextDidChangeNotification object:search]];
        const NSInteger expected=[query isEqualToString:@"zoom"] ? 2 :
            [query isEqualToString:@"unlikely-no-match"] ? 0 : query.length ? 2 : totalRows;
        if(table.numberOfRows!=expected) { fprintf(stderr,"Search result mismatch for %s: %ld\n",query.UTF8String,(long)table.numberOfRows); return 23; }
    }
    fprintf(stdout,"Shortcut table: vertical centering, description/key filtering, aliases, empty result and clearing verified.\n");

    app.processEvents();
    if(const auto path=qEnvironmentVariable("MINDARCHY_SHORTCUTS_SCREENSHOT"); !path.isEmpty()) {
        [reference.contentView layoutSubtreeIfNeeded];
        NSBitmapImageRep *bitmap=[reference.contentView bitmapImageRepForCachingDisplayInRect:reference.contentView.bounds];
        [reference.contentView cacheDisplayInRect:reference.contentView.bounds toBitmapImageRep:bitmap];
        [[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:path.toNSString() atomically:YES];
    }
    [NSApp sendAction:shortcuts.action to:shortcuts.target from:shortcuts];
    NSInteger count=0;
    for(NSWindow *candidate in NSApp.windows)
        if([candidate.identifier isEqualToString:@"mindarchy-keyboard-shortcuts"]) ++count;
    if(count!=1) return 18;
    NSEvent *closeKey=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
        modifierFlags:NSEventModifierFlagCommand timestamp:0 windowNumber:reference.windowNumber
        context:nil characters:@"w" charactersIgnoringModifiers:@"w" isARepeat:NO keyCode:13];
    if(![reference performKeyEquivalent:closeKey] || reference.visible || !native.visible) return 19;
    [NSApp sendAction:shortcuts.action to:shortcuts.target from:shortcuts];
    if(!reference.visible) return 20;
    [reference close];
    fprintf(stdout,"Help menu: separate shortcuts table opens, reuses its window, closes with Cmd-W and reopens.\n");
    return 0;
}
