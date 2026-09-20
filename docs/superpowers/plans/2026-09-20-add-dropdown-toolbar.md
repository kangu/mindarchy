# Add Dropdown Toolbar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Condense the toolbar's three Add actions (Add child, Add node template, Add sibling) into an "Add ▾" dropdown placed directly after the File dropdown.

**Architecture:** All changes are QML-level in `qml/DocumentWorkspace.qml`: a new `ToolbarButton` + `Menu` in the left `documentActions` group, cloned from the File menu's structure; the `NodeTemplateDialog` instance moves into the new button; the center `editingActions` group keeps Undo, Redo, separator, Fold/Expand only. Tests in `tests/ui_test.cpp` get a new TDD test case plus three touch-up edits, updated *before* the QML.

**Tech Stack:** Qt 6 / QML (QtQuick.Controls), CMake, Qt Test (`ui_test.cpp`), `scripts/dev.py` runner.

## Global Constraints

- Where the spec lives: `/Users/radu/Projects/mindmap-blue/qt-prototype/docs/specs/2026-09-20-add-dropdown-toolbar-design.md`
- Working directory for all commands: `/Users/radu/Projects/mindmap-blue/qt-prototype`
- Build/test commands: `python3 scripts/dev.py build` (app), `python3 scripts/dev.py check` (fast tests), `python3 scripts/dev.py ui` (offscreen UI suite). Never qmake per-suite dirs.
- Targeted offscreen UI run: `QT_QPA_PLATFORM=offscreen ./build-macos/ui_test <testFunctionName>` (binary at `build-macos/ui_test` on macOS).
- Do not run installer packaging/deployment; per `agents.md`, do not run the full UI suite after every change — run targeted cases, then one broad `dev.py ui` at the end (this is a broad QML change, so it warrants it).
- No new controller methods; no changes to `qml/DesktopMenus.qml` or native menus.
- Trigger guards preserved verbatim from today: `if (!window.commitEditor("")) return`, `canvas.forceActiveFocus()` for child/sibling, single-selection gate for the template item.
- Navbar text: "Add  ▾" styled exactly like "File  ▾" (`implicitWidth: 92`, `implicitHeight/base` per `window.width < 800`, `font.pixelSize: 13`, `Accessible.name: "Add menu"`).
- Menu styling must replicate `fileActionsMenu`: `width: 240; padding: 6; y: addButton.height + 8; x: 0; radius 10; background #172129; border #34434c; onOpened: currentIndex = 0; firstItem.forceActiveFocus()`.
- New menu item objectNames (used by tests and automation): `addChildButton`, `nodeTemplatesButton`, `addSiblingButton`; new button objectName `addMenuButton`; new menu objectName `addActionsMenu`.
- Each task: implement → test → commit. Per `agents.md`, after each task write a progress summary in `docs/progress/` named `<iso-datetime-normalized>-<title>.md` and restart the running macOS app only after relevant checks pass.

---

### Task 1: Update `tests/ui_test.cpp` to expect the Add dropdown (failing tests)

**Files:**
- Modify: `tests/ui_test.cpp` (~line 358–361, ~line 400–405, ~line 518–521, ~line 1005–1068)

**Interfaces:**
- Produces: objectName contract consumed by QML in Task 2 — `addMenuButton` (toolbar), `addActionsMenu` (Menu), items `addChildButton`, `nodeTemplatesButton`, `addSiblingButton`.

- [ ] **Step 1: Add a new test function after `fileMenuKeyboardAndExport()` (ends at line 1026)**

```cpp
    void addMenuCondensesActions() {
        document->select(2); document->select(1);
        auto *left=window->findChild<QQuickItem *>("documentActions"); QVERIFY(left);
        auto *addButton=window->findChild<QQuickItem *>("addMenuButton"); QVERIFY(addButton);
        // The Add button lives in the left group, next to File, inside the toolbar.
        const auto addScene=addButton->mapToScene(QPointF(addButton->width()/2,addButton->height()/2));
        QVERIFY(addScene.x() < window->width()/2);
        auto *addMenu=window->findChild<QObject *>("addActionsMenu"); QVERIFY(addMenu);
        QVERIFY(!addMenu->property("opened").toBool());
        addButton->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Return);
        QTRY_VERIFY(addMenu->property("opened").toBool());
        QCOMPARE(addMenu->property("count").toInt(),3);
        auto *childItem=window->findChild<QQuickItem *>("addChildButton"); QVERIFY(childItem);
        QTRY_VERIFY(childItem->hasActiveFocus());
        QTest::keyClick(window,Qt::Key_Down); // move to the template item
        auto *templateItem=window->findChild<QQuickItem *>("nodeTemplatesButton"); QVERIFY(templateItem);
        QTest::keyClick(window,Qt::Key_Down); // move to the sibling item
        auto *siblingItem=window->findChild<QQuickItem *>("addSiblingButton"); QVERIFY(siblingItem);
        QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!addMenu->property("visible").toBool());
        // Triggering the child item adds a child to the selection, same as the old toolbar button.
        const int before=document->nodeCount();
        addButton->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Return);
        QTRY_VERIFY(addMenu->property("opened").toBool());
        auto *childItem2=window->findChild<QQuickItem *>("addChildButton"); QVERIFY(childItem2);
        QTRY_VERIFY(childItem2->hasActiveFocus());
        QVERIFY(QMetaObject::invokeMethod(childItem2,"clicked"));
        QTest::qWait(50);
        QCOMPARE(document->nodeCount(),before+1);
        window->findChild<QQuickItem *>("centerWorkspace");
        canvas->forceActiveFocus(); document->undo();
        QCOMPARE(document->nodeCount(),before);
        // Disabled without selection: clear the selection makes the template item disabled.
        document->selectMany({});
        addButton->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Return);
        QTRY_VERIFY(addMenu->property("opened").toBool());
        auto *templateItem2=window->findChild<QQuickItem *>("nodeTemplatesButton"); QVERIFY(templateItem2);
        QVERIFY(!templateItem2->property("enabled").toBool());
        QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!addMenu->property("visible").toBool());
    }
```

Note: `Engine` exposes `Q_INVOKABLE void selectMany(QVariantList ids, bool extend = false)`
(`src/engine.h:181`), so `document->selectMany({})` clears the selection. If deselection
proves impossible in the harness, drop the final block and instead assert
`templateItem->property("enabled").toBool()` while a node is selected in the first open.

- [ ] **Step 2: Reroute template-dialog tests through the Add dropdown**

In `weeklyTemplatePickerAddsBranch()` (line 358–363), replace:

```cpp
        auto *button=window->findChild<QObject *>("nodeTemplatesButton"); QVERIFY(button);
        QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
```

with:

```cpp
        auto *button=window->findChild<QQuickItem *>("addMenuButton"); QVERIFY(button);
        button->forceActiveFocus(); QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
        auto *templateButton=window->findChild<QObject *>("nodeTemplatesButton"); QVERIFY(templateButton);
        QTRY_VERIFY(QMetaObject::invokeMethod(templateButton,"clicked"));
        QTest::qWait(50);
```

Wait after clicking so the add menu can be believed closed before the dialog appears.
Make the same replacement in `meetingTemplateStartsIndividualNoteEditing()` (lines 404–405).

- [ ] **Step 3: Swap the focus-feedback list**

Line 520 currently reads:

```cpp
        for(const QString &name:{QString("fileMenuButton"),QString("zoomPercentage"),QString("searchButton"),QString("nodeTemplatesButton")}) {
```

Change to:

```cpp
        for(const QString &name:{QString("fileMenuButton"),QString("addMenuButton"),QString("zoomPercentage"),QString("searchButton")}) {
```

- [ ] **Step 4: Build and run the new test to verify it fails**

Run:
```sh
cd /Users/radu/Projects/mindmap-blue/qt-prototype
cmake --build build-macos --config Release --target ui_test --parallel 8
QT_QPA_PLATFORM=offscreen ./build-macos/ui_test addMenuCondensesActions
```
Expected: FAIL — `addButton` is null (`QVERIFY(button)` fired at `addMenuButton` lookup).
Also run `QT_QPA_PLATFORM=offscreen ./build-macos/ui_test weeklyTemplatePickerAddsBranch`
Expected: FAIL for the same reason.

- [ ] **Step 5: Commit the failing tests**

```sh
git add tests/ui_test.cpp && git commit -m "test: expect Add dropdown replacing toolbar add actions"
```

---

### Task 2: Add the `plus.svg` icon and toolbar restructure in QML

**Files:**
- Create: `qml/icons/plus.svg` (lucide "plus" path, matches existing stroke style)
- Modify: `resources.qrc` (add one icon entry alongside the other qml/icons entries)
- Modify: `qml/DocumentWorkspace.qml` (toolbar groups, lines ~467–543)

**Interfaces:**
- Consumes: objectName contract from Task 1; existing ids `canvas`, `controller`, `nodeTemplateDialog`; existing `window.commitEditor("")` helper; icon source pattern `qrc:/qml/icons/<name>.svg` from `IconButton` (line ~210).

- [ ] **Step 1: Create `qml/icons/plus.svg`**

Copy the style of an existing svg (open `qml/icons/list-plus.svg` and keep its `fill="none" stroke="currentColor"` headers/coords), body path is the lucide plus:

```svg
<path d="M5 12h14"/><path d="M12 5v14"/>
```

Build the file to match exactly how `qml/icons/corner-down-right.svg` declares its root
svg element (viewBox 0 0 24 24, stroke-width as used across the icon set, plus
`convert`/`stroke-linecap` round) — copy that file and replace only the contained path(s).

- [ ] **Step 2: Register it in `resources.qrc`**

Find the existing qml/icons entries in `resources.qrc` (e.g. `qml/icons/files.svg` at line 7)
and insert next to them:

```xml
    <file>qml/icons/plus.svg</file>
```

- [ ] **Step 3: Restructure the toolbar in `qml/DocumentWorkspace.qml`**

After the `fileButton` ToolbarButton closing brace (the `Menu { id: fileMenu … }` block
ends near line 509–510, inside the `documentActions` RowLayout), insert the new dropdown
button, copying File's shell verbatim:

```qml
                        ToolbarButton {
                            id: addMenuButton; objectName: "addMenuButton"
                            iconName: "plus"; text: "Add  ▾"
                            Accessible.name: "Add menu"
                            ToolTip.text: "New child, template or sibling nodes"
                            display: AbstractButton.TextBesideIcon
                            implicitWidth: 92; implicitHeight: window.width < 800 ? 32 : 36; font.pixelSize: 13
                            enabled: controller.selection.length > 0
                            checked: addMenu.visible
                            onClicked: addMenu.visible ? addMenu.close() : addMenu.open()
                            Keys.onDownPressed: addMenu.open()
                            Keys.onReturnPressed: addMenu.open()
                            Menu {
                                id: addMenu; objectName: "addActionsMenu"
                                y: addMenuButton.height + 8; x: 0; width: 240; padding: 6
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                onOpened: { currentIndex = 0; addChildItem.forceActiveFocus() }
                                background: Rectangle {
                                    radius: 10
                                    color: ShellTheme.colors["#172129"] || "#172129"
                                    border.color: ShellTheme.colors["#34434c"] || "#34434c"
                                }
                                MenuItem {
                                    id: addChildItem; objectName: "addChildButton"
                                    text: "Add child"; icon.source: "qrc:/qml/icons/corner-down-right.svg"; icon.color: window.ink
                                    onTriggered: { addMenu.close(); if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addChild() }
                                }
                                MenuItem {
                                    id: nodeTemplatesItem; objectName: "nodeTemplatesButton"
                                    text: "Add node template"; icon.source: "qrc:/qml/icons/calendar-week.svg"; icon.color: window.ink
                                    enabled: controller.selection.length === 1
                                    onTriggered: { addMenu.close(); if (!window.commitEditor("")) return;
                                        nodeTemplateDialog.visible ? nodeTemplateDialog.close() : nodeTemplateDialog.open() }
                                }
                                MenuItem {
                                    objectName: "addSiblingButton"
                                    text: "Add sibling"; icon.source: "qrc:/qml/icons/list-plus.svg"; icon.color: window.ink
                                    onTriggered: { addMenu.close(); if (!window.commitEditor("")) return; canvas.forceActiveFocus(); controller.addSibling() }
                                }
                            }
                            NodeTemplateDialog {
                                id: nodeTemplateDialog
                                controller: window.controller
                            }
                        }
```

Notes:
- `NodeTemplateDialog` moves verbatim from inside the old `nodeTemplatesButton` into this
  new button (same `id`, same `controller:` binding) — nothing about the dialog changes.
- `declaration order`: the `NodeTemplateDialog` sibling must stay declared inside the new
  `addMenuButton` so `window.controller` binding and the `id` are resolvable just like
  before.

Then delete the three obsolete toolbar buttons from the `editingActions` RowLayout
(the center group, currently lines ~523–541):

- [ ] **Step 3a: remove** `ToolbarButton { iconName: "corner-down-right"; text: "Add child"; … }` (line 523).
- [ ] **Step 3b: remove** the entire

```qml
                        ToolbarButton {
                            id: nodeTemplatesButton
                            objectName: "nodeTemplatesButton"
                            iconName: "calendar-week"
                            text: "Add node template"
                            …
                            NodeTemplateDialog { id: nodeTemplateDialog; controller: window.controller }
                        }
```

block (lines ~524–540).
- [ ] **Step 3c: remove** `ToolbarButton { iconName: "list-plus"; text: "Add sibling"; … }` (line 541).
- [ ] **Step 3d: keep** Undo, Redo, the `Rectangle { implicitWidth: 1; … }` separator, and
  the Fold/Expand branch button exactly as they are in `editingActions`.

- [ ] **Step 4: Build and run the targeted tests to verify they pass**

```sh
cmake --build build-macos --config Release --target ui_test --parallel 8
QT_QPA_PLATFORM=offscreen ./build-macos/ui_test addMenuCondensesActions
QT_QPA_PLATFORM=offscreen ./build-macos/ui_test weeklyTemplatePickerAddsBranch
QT_QPA_PLATFORM=offscreen ./build-macos/ui_test meetingTemplateStartsIndividualNoteEditing
QT_QPA_PLATFORM=offscreen ./build-macos/ui_test headerButtonsDoNotShowFocusFeedback
QT_QPA_PLATFORM=offscreen ./build-macos/ui_test toolbarGroupsAndNewDocument
QT_QPA_PLATFORM=offscreen ./build-macos/ui_test fileMenuKeyboardAndExport
```
Expected: all PASS. `fileMenuKeyboardAndExport` is untouched but sits beside the edited
block — it re-verifies the File menu didn't change behavior.

- [ ] **Step 5: Commit**

```sh
git add qml/DocumentWorkspace.qml qml/icons/plus.svg resources.qrc
git commit -m "feat: condense add actions into an Add toolbar dropdown"
```

---

### Task 3: Full verification, progress doc, restart running app

**Files:**
- Create: `docs/progress/<ISO-DATETIME>-add-dropdown-toolbar.md`

**Interfaces:** none (verification-only).

- [ ] **Step 1: Broad QML check**

```sh
python3 scripts/dev.py ui
```
Expected: `ctest -R ^ui$` passes all tests.

```sh
python3 scripts/dev.py check
```
Expected: fast tests pass.

- [ ] **Step 2: Build the app**

```sh
python3 scripts/dev.py build
```
Expected: `mindarchy` target builds with no QML errors.

- [ ] **Step 3: Restart the running app instance (per agents.md)**

Close any open Mindarchy instance and relaunch the freshly built app binary so the running
app reflects the new toolbar. Preserve unsaved sessions; if a session is unsaved at
restart-time, leave the running app alone and relaunch tests only on cancellation.

- [ ] **Step 4: Visual smoke check on a real window**

Open a map, select a node, and manually verify in the toolbar:
1. "Add ▾" sits immediately right of "File ▾" in the left documentActions group.
2. Opening it shows three items with icons: Add child / Add node template / Add sibling.
3. With no selection the button is disabled; with a single selected node the template item
   is enabled; Add child and Add sibling are always actionable.
4. Escape and click-outside close it; keyboard Down/Return work like in the File menu.
5. Undo/Redo/Fold remain in the center group.

- [ ] **Step 5: Write the progress doc**

Create `docs/progress/<YYYY-MM-DDTHH-MM-SS>-add-dropdown-toolbar.md` with the required
format from `agents.md`: Initial Prompt, the plan followed, Implementation Summary, Next
Steps.

- [ ] **Step 6: Commit**

```sh
git add docs/progress && git commit -m "docs: add dropdown toolbar progress notes"
```

## Ratified deviation (2026-09-20, user-approved)

The "Add node template" menu item calls `nodeTemplateDialog.open()` directly instead of the
plan's `visible ? close : open` toggle: the File/Add menu shell closes the menu before the
dialog becomes interactive, so the toggle branch is unreachable in practice, and the open
flaps under Qt 6.11 QTRY double-evaluation in tests. The Add button itself carries no
`enabled` gate (the design spec's "enabled unless every item is disabled" governs over the
snippet; child/sibling actions were always enabled on the old toolbar).
