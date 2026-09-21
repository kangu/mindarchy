#include "../src/windowmenubar.h"
#include "shelltheme.h"
#include "documentrecovery.h"
#include "../src/canvas.h"
#include "../src/engine.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPainter>
#include <QScopeGuard>
#include <QDir>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextDocument>
#include "recentpreview.h"
#include "viewportstate.h"
#include <QTextCursor>
#include <QtTest>

class UiTest : public QObject {
    Q_OBJECT
    QTemporaryDir shellThemeDirectory;
    Engine *document = nullptr;
    QQmlApplicationEngine *qml = nullptr;
    QQuickWindow *window = nullptr;
    MindCanvas *canvas = nullptr;
    QQuickItem *editor = nullptr;
    QQuickItem *findVisual(QQuickItem *item, const QString &name) {
        if (item->objectName() == name) return item;
        for (auto *child : item->childItems())
            if (auto *found = findVisual(child, name)) return found;
        return nullptr;
    }
    void stage(const QString &name) {
        const int delay = qEnvironmentVariableIntValue("MINDMAP_LIVE_TEST_MS");
        if (delay > 0) {
            window->setTitle("LIVE TEST — " + name);
            QTest::qWait(delay);
        }
    }
    QPoint screenCenter(int id) const {
        return canvas->mapToScene(canvas->mapFromWorld(document->nodes().value(id).rect.center()))
            .toPoint();
    }
    void drag(const QPoint &from, const QPoint &to) {
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        for (int step = 1; step <= 8; ++step)
            QTest::mouseMove(window, from + (to - from) * step / 8, 30);
        stage("Dragging branch — release to apply");
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
        QTest::qWait(300);
    }
    QString plain(int id) const {
        QTextDocument text;
        text.setHtml(document->nodes().value(id).text);
        return text.toPlainText();
    }
    void type(const QString &text) {
        for (QChar c : text)
            QTest::keyClick(window, c.toLatin1());
        stage("Editing: " + text);
    }
  private slots:
    void shareLivesInDocumentToolbar() {
        auto *workspace=window->findChild<QQuickItem *>("documentWorkspace"); QVERIFY(workspace);
        auto *button=workspace->findChild<QQuickItem *>("shareButton"); QVERIFY(button);
        QVERIFY(button->isVisible());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        auto *dialog=workspace->findChild<QObject *>("shareDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(workspace->property("modalInteraction").toBool());
        auto *invite=dialog->findChild<QObject *>("shareInvite"); QVERIFY(invite);
        QVERIFY(!invite->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
    }
    void branchStylePickerTracksUndo() {
        auto *picker=window->findChild<QObject *>("branchStylePicker"); QVERIFY(picker);
        QCOMPARE(picker->property("count").toInt(),7);
        const auto before=document->branchStyle();
        QVERIFY(QMetaObject::invokeMethod(picker,"activated",Q_ARG(int,6)));
        QCOMPARE(document->branchStyle(),QString("Elven filigree"));
        auto *stroke=window->findChild<QObject *>("style-branchStroke"); QVERIFY(stroke);
        QTRY_VERIFY(!stroke->property("enabled").toBool());
        QTRY_COMPARE(picker->property("currentIndex").toInt(),6);
        document->undo(); QCOMPARE(document->branchStyle(),before);
        QTRY_COMPARE(picker->property("currentIndex").toInt(),document->branchStyles().indexOf(before));
    }
    void imageClipboardShortcuts() {
        NodeImage image; QImage pixels(120,60,QImage::Format_RGB32); pixels.fill(Qt::red);
        QVERIFY(NodeImage::importPixels(pixels,image)); QVERIFY(document->setImage(2,image));
        canvas->fit(); QTest::qWait(250);
        const auto n=document->nodes()[2];
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(canvas->mapFromWorld(n.image.rect(n.rect).center())).toPoint());
        QTest::keyClick(window,Qt::Key_C,Qt::ControlModifier);
        document->select(3); canvas->forceActiveFocus(); QTest::keyClick(window,Qt::Key_V,Qt::ControlModifier);
        QVERIFY(document->hasImage(2)); QTRY_VERIFY(document->hasImage(3));
        QCOMPARE(document->nodes()[3].image.data,document->nodes()[2].image.data);
        QTest::keyClick(window,Qt::Key_X,Qt::ControlModifier); QTRY_VERIFY(!document->hasImage(3));
        QTest::keyClick(window,Qt::Key_Z,Qt::ControlModifier); QTRY_VERIFY(document->hasImage(3));
    }
    void imagePlacementSidebar() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs); tabs->setProperty("currentIndex",1);
        document->select(2);
        auto *section=findVisual(window->contentItem(),"imagePlacementSection"); QVERIFY(section); QVERIFY(!section->isVisible());
        NodeImage image; QImage pixels(120,60,QImage::Format_RGB32); pixels.fill(Qt::red);
        QVERIFY(NodeImage::importPixels(pixels,image)); QVERIFY(document->setImage(2,image)); QTRY_VERIFY(section->isVisible());
        for(const QString placement:{QString("right"),QString("top"),QString("bottom"),QString("left")}) {
            auto *button=findVisual(window->contentItem(),"image-placement-"+placement); QVERIFY(button);
            button->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
            QTRY_COMPARE(document->nodes()[2].image.placement,placement);
        }
        QVERIFY(document->removeImage(2)); QTRY_VERIFY(!section->isVisible());
    }
    void spaceTogglesSelectedImagePreview() {
        NodeImage image; QImage pixels(240,120,QImage::Format_RGB32); pixels.fill(Qt::blue);
        QVERIFY(NodeImage::importPixels(pixels,image)); QVERIFY(document->setImage(2,image));
        canvas->fit(); QTest::qWait(250);
        const auto node=document->nodes()[2];
        const auto point=canvas->mapToScene(canvas->mapFromWorld(node.image.rect(node.rect).center())).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point);
        const auto view=canvas->persistentView(); const auto revision=document->recoveryRevision();
#ifdef Q_OS_LINUX
        auto *popup=window->findChild<QObject*>("borderlessImagePreview"); QVERIFY(popup);
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(popup->property("opened").toBool());
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(!popup->property("opened").toBool());
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(popup->property("opened").toBool());
        QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!popup->property("opened").toBool());
        QCOMPARE(canvas->persistentView(),view); QCOMPARE(document->recoveryRevision(),revision);
        return;
#endif
        auto preview=window->findChild<QQuickWindow*>("nodeImagePreview"); QVERIFY(preview);
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(preview->isVisible());
#ifdef Q_OS_MACOS
        QVERIFY(preview->flags() & Qt::FramelessWindowHint);
        QCOMPARE(preview->flags() & Qt::WindowType_Mask,Qt::Tool);
        QVERIFY(preview->width()>0 && preview->height()>0);

#endif
        QTest::keyClick(preview,Qt::Key_Space); QTRY_VERIFY(!preview->isVisible());
        window->requestActivate(); canvas->forceActiveFocus();
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(preview->isVisible());
        QTest::keyClick(preview,Qt::Key_Escape); QTRY_VERIFY(!preview->isVisible());
#ifdef Q_OS_MACOS
        window->requestActivate(); canvas->forceActiveFocus();
        QTest::keyClick(window,Qt::Key_Space); QTRY_VERIFY(preview->isVisible());
        QTRY_VERIFY(preview->isActive());
        QTest::qWait(100);
        if(qEnvironmentVariableIsSet("MINDARCHY_PANEL_EVIDENCE")) QVERIFY(preview->grabWindow().save(qEnvironmentVariable("MINDARCHY_PANEL_EVIDENCE")));
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(QPointF(20,20)).toPoint()); QTRY_VERIFY(!preview->isVisible());
#endif
        QCOMPARE(canvas->persistentView(),view); QCOMPARE(document->recoveryRevision(),revision);
    }
    void imageFileDropResizeAndPreview() {
        document->loadFixture(3); document->setThemeId("beach-day");
        document->setText(1,"Portable images"); document->setText(2,"Drag an image onto a node"); document->setText(3,"Resize from its edges");
        canvas->fit(); QTest::qWait(250);
        QTemporaryDir dir; QImage picture(640,360,QImage::Format_RGB32);
        QPainter painter(&picture); QLinearGradient sky(0,0,640,360); sky.setColorAt(0,QColor("#77d7ea")); sky.setColorAt(1,QColor("#243e80")); painter.fillRect(picture.rect(),sky);
        painter.setBrush(QColor("#ffe0a0")); painter.setPen(Qt::NoPen); painter.drawEllipse(QPoint(470,100),45,45);
        painter.setBrush(QColor("#398b74")); painter.drawPolygon(QPolygon{{0,360},{240,110},{440,360}});
        painter.setBrush(QColor("#245b60")); painter.drawPolygon(QPolygon{{230,360},{440,170},{640,360}}); painter.end();
        const auto file=dir.filePath("landscape.png"); QVERIFY(picture.save(file));
        QMimeData mime; mime.setUrls({QUrl::fromLocalFile(file)});
        const auto point=screenCenter(2);
        QDragEnterEvent enter(point,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(window,&enter); QVERIFY(enter.isAccepted());
        QDragMoveEvent move(point,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(window,&move); QVERIFY(move.isAccepted());
        QDropEvent drop(point,Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QCoreApplication::sendEvent(window,&drop); QVERIFY(drop.isAccepted()); QVERIFY(document->hasImage(2));
        QTest::qWait(250); canvas->fit(); QTest::qWait(250);
        const auto r=canvas->imageSelectionRect(); QVERIFY(!r.isEmpty());
        const auto handle=canvas->mapToScene(QPointF(r.right(),r.center().y())).toPoint();
        drag(handle,handle+QPoint(60,0));
        QVERIFY(document->nodes()[2].image.width>120);
        const auto ratio=document->nodes()[2].image.size(); QVERIFY(std::abs(ratio.width()/ratio.height()-640./360)<.001);
        canvas->fit(); QTest::qWait(250);
        const auto evidence=qEnvironmentVariable("MINDARCHY_IMAGE_EVIDENCE_DIR");
        if(!evidence.isEmpty()) {
            QDir().mkpath(evidence); QVERIFY(window->grabWindow().save(evidence+"/node-image-resize.png"));
            QVERIFY(document->save(evidence+"/portable-images.omm"));
        }
        const auto imageRect=canvas->imageSelectionRect();
        QTest::mouseDClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(imageRect.center()).toPoint());
#ifdef Q_OS_LINUX
        auto *popup=window->findChild<QObject*>("borderlessImagePreview"); QVERIFY(popup); QTRY_VERIFY(popup->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup,"close"));
#else
        auto preview=window->findChild<QQuickWindow*>("nodeImagePreview"); QVERIFY(preview); QTRY_VERIFY(preview->isVisible()); preview->close();
#endif
        window->requestActivate(); canvas->forceActiveFocus();
        document->undo(); QCOMPARE(document->nodes()[2].image.width,120.);
    }
    void recentMenuRefreshesAndClearsSharedHistory() {
        QTemporaryDir dir; Engine writer(nullptr,Engine::InitialContent::Blank);
        writer.setRecentDirectory(dir.filePath("history"));
        QVERIFY(writer.save(dir.filePath("recent.omm")));
        document->setRecentDirectory(dir.filePath("history"));
        auto reset=qScopeGuard([&] { document->setRecentDirectory({}); });
        auto *recent=window->findChild<QObject *>("desktopRecentMenu"); QVERIFY(recent);
        QVERIFY(QMetaObject::invokeMethod(recent,"aboutToShow"));
        QCOMPARE(recent->property("documents").toList().size(),1);
        auto *clear=window->findChild<QObject *>("clearRecentAction"); QVERIFY(clear);
        QVERIFY(QMetaObject::invokeMethod(clear,"triggered"));
        QVERIFY(writer.recentDocuments().isEmpty()); QCOMPARE(recent->property("documents").toList().size(),0);
    }
    void desktopMenusExposeSharedCommands() {
        auto *file=window->findChild<QObject *>("desktopFileMenu");
        auto *windows=window->findChild<QObject *>("desktopWindowMenu");
        auto *help=window->findChild<QObject *>("desktopHelpMenu");
        QVERIFY(file); QVERIFY(windows); QVERIFY(help);
        QCOMPARE(file->property("count").toInt(),8); QCOMPARE(help->property("count").toInt(),1);
        QVERIFY(windows->property("count").toInt()>=10);
        auto *button=window->findChild<QQuickItem *>("applicationMenuButton"); QVERIFY(button);
#ifdef Q_OS_LINUX
        QVERIFY(button->isVisible());
        QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
        auto *menu=window->findChild<QObject *>("applicationMenu"); QVERIFY(menu);
        QTRY_VERIFY(menu->property("opened").toBool()); QCOMPARE(menu->property("count").toInt(),3);
        if(const auto path=qEnvironmentVariable("MINDARCHY_MENU_SCREENSHOT");!path.isEmpty()) { QTest::qWait(250); QVERIFY(window->grabWindow().save(path)); }
        QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!menu->property("opened").toBool());
#else
        QVERIFY(!button->isVisible());
#endif
        QSignalSpy newDocument(document,&Engine::newDocumentRequested);
        auto *action=window->findChild<QObject *>("desktopNewAction"); QVERIFY(action);
        QVERIFY(QMetaObject::invokeMethod(action,"triggered")); QCOMPARE(newDocument.size(),1);
        auto *shortcuts=window->findChild<QObject *>("windowsKeyboardShortcuts"); QVERIFY(shortcuts);
        action=window->findChild<QObject *>("desktopKeyboardShortcutsAction"); QVERIFY(action);
        QVERIFY(QMetaObject::invokeMethod(action,"triggered")); QTRY_VERIFY(shortcuts->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(shortcuts,"close"));
        QTRY_VERIFY(!shortcuts->property("visible").toBool()); window->requestActivate(); canvas->forceActiveFocus();
        QTRY_VERIFY(canvas->hasActiveFocus());
    }
    void typingReplacesSelectedTitle() {
        document->setText(2,"hello"); document->select(2); canvas->forceActiveFocus();
        type("yey");
        QTRY_VERIFY(canvas->editing());
        QCOMPARE(editor->property("text").toString().contains("yey"),true);
        QTest::keyClick(window,Qt::Key_Return); QTRY_VERIFY(!canvas->editing());
        QCOMPARE(plain(2),QString("yey"));
        document->undo(); QCOMPARE(plain(2),QString("hello"));
        for(const auto &text : {QString("Foo"),QString("Task"),QString("0.5"),QString("#1"),QString("<tag>")}) {
            canvas->forceActiveFocus(); type(text);
            QTRY_VERIFY(canvas->editing()); QTest::keyClick(window,Qt::Key_Return);
            QCOMPARE(plain(2),text); QVERIFY(!document->selectedTask());
        }
        canvas->forceActiveFocus(); QTest::keyClick(window,Qt::Key_T,Qt::AltModifier);
        QVERIFY(document->selectedTask()); QVERIFY(!canvas->editing());
        document->select(-1); QTest::keyClick(window,Qt::Key_A); QVERIFY(!canvas->editing());
        document->select(2); QTest::keyPress(window,Qt::Key_Space); QVERIFY(!canvas->editing()); QTest::keyRelease(window,Qt::Key_Space);
        document->setText(2,"keep"); canvas->forceActiveFocus();
        const auto zoom=canvas->zoom();
        QTest::keyClick(window,Qt::Key_Plus); QVERIFY(!canvas->editing()); QVERIFY(canvas->zoom()>zoom); QCOMPARE(plain(2),QString("keep"));
        QTest::keyClick(window,Qt::Key_Minus); QVERIFY(!canvas->editing()); QCOMPARE(plain(2),QString("keep"));
        QTest::keyClick(window,Qt::Key_Equal); QVERIFY(!canvas->editing()); QVERIFY(canvas->zoom()>zoom);
    }
    void resourcesInspectorAddsEditsAndRemoves() {
        document->select(2);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs); tabs->setProperty("currentIndex",1);
        auto *button=window->findChild<QObject *>("addWebResource"); QVERIFY(button);
        QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
        auto *dialog=window->findChild<QObject *>("resourceDialog"); QVERIFY(dialog); QTRY_VERIFY(dialog->property("opened").toBool());
        auto *name=window->findChild<QObject *>("resourceName"); auto *target=window->findChild<QObject *>("resourceTarget");
        QVERIFY(name); QVERIFY(target); name->setProperty("text","Project reference"); target->setProperty("text","https://example.com/project");
        auto *save=window->findChild<QObject *>("saveResource"); QVERIFY(save); QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QTRY_VERIFY(!dialog->property("opened").toBool()); QCOMPARE(document->selectedResources().size(),1);
        QCOMPARE(document->selectedResources()[0].toMap()["name"].toString(),QString("Project reference"));
        QTest::qWait(50);
        auto *editResource=findVisual(window->contentItem(),"editResource0"); QVERIFY(editResource);
        QVERIFY(QMetaObject::invokeMethod(editResource,"clicked")); QTRY_VERIFY(dialog->property("opened").toBool());
        name->setProperty("text","Updated reference"); QVERIFY(QMetaObject::invokeMethod(save,"clicked"));
        QCOMPARE(document->selectedResources()[0].toMap()["name"].toString(),QString("Updated reference"));
        auto *addFile=window->findChild<QObject *>("addFileResource"); QVERIFY(addFile);
        QVERIFY(QMetaObject::invokeMethod(addFile,"clicked")); QTRY_VERIFY(dialog->property("opened").toBool());
        name->setProperty("text","Design brief"); target->setProperty("text","/tmp/mindarchy-design-brief.pdf");
        QVERIFY(QMetaObject::invokeMethod(save,"clicked")); QCOMPARE(document->selectedResources().size(),2);
        QTest::qWait(150);
        if(const auto path=qEnvironmentVariable("MINDARCHY_RESOURCES_SCREENSHOT");!path.isEmpty()) {
            auto *item=qobject_cast<QQuickItem *>(button); QVERIFY(item);
            // Scroll the node inspector to its resource section for visual QA.
            for(auto *parent=item->parentItem();parent;parent=parent->parentItem()) {
                if(parent->metaObject()->indexOfProperty("contentY")>=0) {
                    parent->setProperty("contentY",std::max(0.0,item->mapToItem(parent,QPointF()).y()+parent->property("contentY").toDouble()-parent->height()+80)); break;
                }
            }
            QTest::qWait(300); QVERIFY(window->grabWindow().save(path));
        }
        auto *remove=findVisual(window->contentItem(),"removeResource0"); QVERIFY(remove);
        QVERIFY(QMetaObject::invokeMethod(remove,"clicked")); QCOMPARE(document->selectedResources().size(),1);
        document->undo(); QCOMPARE(document->selectedResources().size(),2);
    }
    void branchClipboardAndFocusUseCanvasShortcuts() {
        auto *clipboard=QGuiApplication::clipboard();
        auto *backup=new QMimeData;
        if(const auto *mime=clipboard->mimeData()) for(const auto &format:mime->formats()) backup->setData(format,mime->data(format));
        const auto restore=qScopeGuard([&]{clipboard->setMimeData(backup);});
        document->select(2); const auto initial=document->nodeCount();
        const auto revision=document->recoveryRevision();
        QTest::keyClick(window,Qt::Key_C,Qt::ControlModifier);
        QVERIFY(clipboard->mimeData()->hasFormat("application/x-mindarchy-branches+json"));
        QCOMPARE(document->recoveryRevision(),revision);
        Engine other(nullptr,Engine::InitialContent::Blank);
        QVERIFY2(other.pasteBranches(),qPrintable(other.error())); QVERIFY(other.nodeCount()>1);
        document->select(1); QTest::keyClick(window,Qt::Key_V,Qt::ControlModifier);
        QVERIFY(document->nodeCount()>initial);
        document->undo(); QCOMPARE(document->nodeCount(),initial);
        document->select(2); canvas->forceActiveFocus();
        const auto pan=QPointF(canvas->panX(),canvas->panY());const auto zoom=canvas->zoom();
        QTest::keyClick(window,Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier);
        QVERIFY(canvas->focusActive());
        auto *bar=window->findChild<QQuickItem *>("focusBreadcrumbBar"); QVERIFY(bar);QVERIFY(bar->isVisible());
        if(const auto path=qEnvironmentVariable("MINDARCHY_FOCUS_SCREENSHOT");!path.isEmpty()) {
            QTest::qWait(250);QVERIFY(window->grabWindow().save(path));
        }
        canvas->zoomIn();QTest::keyClick(window,Qt::Key_Escape);
        QVERIFY(!canvas->focusActive());QVERIFY(!bar->isVisible());
        QTRY_COMPARE(QPointF(canvas->panX(),canvas->panY()),pan);QCOMPARE(canvas->zoom(),zoom);
        QVERIFY(!window->findChild<QObject *>("focusBranchButton"));
        QVERIFY(!window->findChild<QObject *>("branchClipboardButton"));
        QTest::keyClick(window,Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier); QVERIFY(canvas->focusActive());
        QTest::keyClick(window,Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier); QVERIFY(!canvas->focusActive());
        document->selectMany({2,3}); const int connections=document->connectionCount();
        QTest::keyClick(window,Qt::Key_L,Qt::ControlModifier);
        QCOMPARE(document->connectionCount(),connections+1);
        document->undo(); QCOMPARE(document->connectionCount(),connections);
        document->select(2);
        // Text editing must retain ordinary copy/paste instead of copying nodes.
        canvas->beginEdit(2);QTRY_VERIFY(editor->hasActiveFocus());
        clipboard->setText("inline text");
        QVERIFY(QMetaObject::invokeMethod(editor,"selectAll"));
        QTest::keyClick(window,Qt::Key_V,Qt::ControlModifier);
        QCOMPARE(document->nodeCount(),initial);QVERIFY(editor->property("text").toString().contains("inline text"));
        QTest::keyClick(window,Qt::Key_Return);QTRY_VERIFY(!canvas->editing());
    }

    void headerSearchCyclesCentersAndFlashes() {
        document->setText(2,"Unique searchable alpha"); document->setText(3,"Unique searchable beta");
        auto *button=window->findChild<QObject *>("searchButton"); QVERIFY(button);
        QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
        auto *field=window->findChild<QQuickItem *>("mindmapSearchField"); QVERIFY(field); QVERIFY(field->hasActiveFocus());
        field->setProperty("text","uniq searchable");
        const double zoom=canvas->zoom();
        QCOMPARE(window->property("searchIndex").toInt(),-1);
        QTest::qWait(100); QCOMPARE(window->property("searchIndex").toInt(),-1);
        QTRY_COMPARE(window->property("searchIndex").toInt(),0);
        const int first=document->selectedId(); QVERIFY(first==2 || first==3);
        const auto center=canvas->mapFromWorld(document->nodes().value(first).rect.center());
        QVERIFY(QLineF(center,QPointF(canvas->width()/2,canvas->height()/2)).length()<0.01);
        QCOMPARE(canvas->zoom(),zoom); QVERIFY(field->hasActiveFocus());
        auto *flash=window->findChild<QQuickItem *>("searchResultFlash"); QVERIFY(flash);
        QTRY_VERIFY(flash->opacity()>0.1);
        QTest::keyClick(window,Qt::Key_Return); QVERIFY(document->selectedId()!=first);
        QTest::keyClick(window,Qt::Key_Return); QCOMPARE(document->selectedId(),first);
        QTRY_VERIFY(flash->opacity()<0.01);
        field->setProperty("text","zzzznomatching1234"); QTest::keyClick(window,Qt::Key_Return);
        QCOMPARE(document->selectedId(),first);
        QTest::keyClick(window,Qt::Key_Escape); QVERIFY(!window->property("searchOpen").toBool());
        QTest::keySequence(window,QKeySequence::Find); QTRY_VERIFY(window->property("searchOpen").toBool());
        QTest::keyClick(window,Qt::Key_Escape);
    }
    void weeklyTemplatePickerAddsBranch() {
        document->select(1); const int before=document->nodeCount();
        auto *button=window->findChild<QQuickItem *>("addMenuButton"); QVERIFY(button);
        button->forceActiveFocus(); QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
        auto *templateButton=window->findChild<QObject *>("nodeTemplatesButton"); QVERIFY(templateButton);
        QTRY_VERIFY(QMetaObject::invokeMethod(templateButton,"clicked"));
        QTest::qWait(50);
        auto *dialog=window->findChild<QObject *>("nodeTemplateDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *content=dialog->property("contentItem").value<QQuickItem *>(); QVERIFY(content);
        auto *choice=findVisual(content,"nodeTemplateChoice"); QVERIFY(choice);
        QVERIFY(QMetaObject::invokeMethod(choice,"clicked"));
        QTRY_VERIFY(dialog->property("needsWeek").toBool());
        dialog->setProperty("month",document->templateCalendar("2026-09-01"));
        QTest::qWait(250);
        auto *week=findVisual(content,"templateWeek_2026-09-07"); QVERIFY(week);
        QVERIFY(QMetaObject::invokeMethod(week,"clicked"));
        QCOMPARE(dialog->property("selectedMonday").toString(),QString("2026-09-07"));
        auto *otherWeek=findVisual(content,"templateWeek_2026-09-21"); QVERIFY(otherWeek);
        // Popup size changes animate: synchronize the scene before using its hit coordinates.
        QSignalSpy framePresented(window,&QQuickWindow::frameSwapped);
        window->requestUpdate(); QVERIFY(framePresented.wait(1000));
        // Click Wednesday, well away from the week-number column.
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
            otherWeek->mapToScene(QPointF(otherWeek->width()*3.5/8,otherWeek->height()/2)).toPoint());
        QCOMPARE(dialog->property("selectedMonday").toString(),QString("2026-09-21"));
        QVERIFY(dialog->property("selectedLabel").toString().contains("Week 39"));
        QCOMPARE(document->nodeCount(),before);
        QVERIFY(dialog->property("visible").toBool());
        // The end of the row (Sunday) selects the same week too.
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
            week->mapToScene(QPointF(week->width()*7.5/8,week->height()/2)).toPoint());
        QCOMPARE(dialog->property("selectedMonday").toString(),QString("2026-09-07"));
        if(qEnvironmentVariableIsSet("MINDARCHY_TEMPLATE_SCREENSHOT")) {
            QTest::qWait(150); QVERIFY(window->grabWindow().save(qEnvironmentVariable("MINDARCHY_TEMPLATE_SCREENSHOT")));
        }
        auto *add=findVisual(content,"addNodeTemplate"); QVERIFY(add);
        QVERIFY(QMetaObject::invokeMethod(add,"clicked"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(document->nodeCount(),before+11);
        QTRY_VERIFY(canvas->editing());
        QTRY_VERIFY(editor->hasActiveFocus()); type("First weekly task"); QTest::keyClick(window,Qt::Key_Escape);
        QVERIFY(document->selectedTask()); QCOMPARE(plain(document->selectedId()),QString("First weekly task"));
    }
    void meetingTemplateStartsIndividualNoteEditing() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs); tabs->setProperty("currentIndex",1);
        document->select(2); const auto previous=document->nodes().value(2);
        QVERIFY(!window->findChild<QObject *>("applyMeetingTemplate"));
        auto *button=window->findChild<QQuickItem *>("addMenuButton"); QVERIFY(button);
        button->forceActiveFocus(); QVERIFY(QMetaObject::invokeMethod(button,"clicked"));
        auto *templateButton=window->findChild<QObject *>("nodeTemplatesButton"); QVERIFY(templateButton);
        QTRY_VERIFY(QMetaObject::invokeMethod(templateButton,"clicked"));
        QTest::qWait(50);
        auto *dialog=window->findChild<QObject *>("nodeTemplateDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *content=dialog->property("contentItem").value<QQuickItem *>(); QVERIFY(content);
        QVERIFY(!dialog->property("needsWeek").toBool());
        auto *calendar=findVisual(content,"templateWeekCalendar"); QVERIFY(calendar); QVERIFY(!calendar->isVisible());
        auto *meetingOption=findVisual(content,"templateOption-meeting-notes"); QVERIFY(meetingOption);
        QVERIFY(QMetaObject::invokeMethod(meetingOption,"clicked"));
        QTRY_VERIFY(canvas->editing()); QTRY_VERIFY(editor->hasActiveFocus());
        const int meeting=document->nodes().value(2).children.last();
        QCOMPARE(document->nodes().value(2).text,previous.text);
        QCOMPARE(document->nodes().value(2).children.size(),previous.children.size()+1);
        QCOMPARE(document->selectedEntryPrompt(),QString("Capture a note…"));
        QTextDocument draft; draft.setHtml(editor->property("text").toString());
        QVERIFY(draft.toPlainText().trimmed().isEmpty());
        type("Meeting discussion"); QTest::keyClick(window,Qt::Key_Escape);
        QVERIFY(!canvas->editing()); document->select(meeting);
        auto *attendees=window->findChild<QQuickItem *>("meetingAttendees"); QVERIFY(attendees); QVERIFY(attendees->isVisible());
        attendees->forceActiveFocus(); type("Alex, Sam");
        QCOMPARE(document->selectedMeeting().value("attendees").toString(),QString("Alex, Sam"));
        if(qEnvironmentVariableIsSet("MINDMAP_MEETING_SCREENSHOT")) {
            canvas->fit(); QTest::qWait(250);
            QVERIFY(window->grabWindow().save(qEnvironmentVariable("MINDMAP_MEETING_SCREENSHOT")));
        }
        document->select(2); QVERIFY(!attendees->isVisible());
    }
    void integratedColorPickerPresetsCustomAndCancel() {
        auto *button=window->findChild<QObject *>("style-fill-picker");
        QVERIFY(button);
        const auto original=document->appearance(1).fill;
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QObject *picker=nullptr;
        for (auto *candidate : window->findChildren<QObject *>("appColorPicker"))
            if (candidate->property("visible").toBool()) picker=candidate;
        QVERIFY(picker);
        if (qEnvironmentVariableIsSet("MINDMAP_COLOR_SCREENSHOT")) {
            QTest::qWait(200); QVERIFY(window->grabWindow().save(qEnvironmentVariable("MINDMAP_COLOR_SCREENSHOT")));
        }
        auto *content=picker->property("contentItem").value<QQuickItem *>();
        QVERIFY(content);
        auto *preset=findVisual(content,"preset-ef8585");
        QVERIFY(preset); QVERIFY(QMetaObject::invokeMethod(preset, "clicked"));
        QCOMPARE(document->appearance(1).fill, original);
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QCOMPARE(document->appearance(1).fill.name(), QString("#ef8585"));
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        auto *hex=findVisual(content,"colorHex");
        QVERIFY(hex); hex->setProperty("text", "#8040a0e0");
        QVERIFY(QMetaObject::invokeMethod(hex, "textEdited"));
        QCOMPARE(picker->property("selectedColor").value<QColor>().name(QColor::HexArgb), QString("#8040a0e0"));
        QVERIFY(QMetaObject::invokeMethod(picker, "reject"));
        QCOMPARE(document->appearance(1).fill.name(), QString("#ef8585"));
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        hex->setProperty("text", "not a color");
        QVERIFY(!picker->property("validHex").toBool());
        hex->setProperty("text", "#8040a0e0"); QMetaObject::invokeMethod(hex,"textEdited");
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QCOMPARE(document->appearance(1).fill.name(QColor::HexArgb), QString("#8040a0e0"));
    }
    void recoveryCapturesTypingNotesDateAndQuitWithoutPrompt() {
        QTemporaryDir dir; const auto path=dir.filePath("window.recovery");
        DocumentRecovery recovery(document,window,canvas,path);
        canvas->initializeView(); QTest::qWait(50);
        canvas->beginEdit(2); type("Uncommitted typing");
        auto *notes=window->findChild<QObject *>("notesEditor"); QVERIFY(notes);
        notes->setProperty("text","Unapplied notes");
        QTest::qWait(1100);
        Engine recovered; QVariantMap draft;
        QVERIFY(recovered.openRecovery(path,&draft));
        QCOMPARE(draft["editingId"].toInt(),2);
        QTextDocument text; text.setHtml(draft["text"].toString()); QCOMPARE(text.toPlainText(),QString("Uncommitted typing"));
        QCOMPARE(draft["notes"].toString(),QString("Unapplied notes"));
        const auto currentText=document->nodes().value(2).text;
        QVERIFY(currentText!=draft["text"].toString()); // snapshot did not commit or alter undo history
        bool quitSaved=false;
        const auto quitConnection=connect(document,&Engine::quitRequested,this,[&] { quitSaved=recovery.prepareQuit(); });
        window->requestActivate(); QTest::qWait(50);
        const QKeySequence quitKey(QKeySequence::Quit);
        if(QGuiApplication::platformName()=="cocoa") QVERIFY(!quitKey.isEmpty());
        // The offscreen platform supplies no StandardKey.Quit binding.
        if(quitKey.isEmpty()) emit document->quitRequested();
        else QTest::keySequence(window,quitKey);
        QTRY_VERIFY(quitSaved);
        disconnect(quitConnection);
        QVERIFY(window->property("quitPending").toBool());
        QVERIFY(!window->findChild<QObject *>("closeConfirmation")->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(window,"abortSessionQuit"));
        canvas->endEdit();
        QVERIFY(QMetaObject::invokeMethod(window,"restoreRecoveryDraft",Q_ARG(QVariant,draft)));
        QVERIFY(canvas->editing()); QCOMPARE(editor->property("text").toString(),draft["text"].toString());
        QCOMPARE(notes->property("text").toString(),QString("Unapplied notes"));
        canvas->endEdit(); document->select(3); document->setNodeKind(3,"date");
        auto *date=window->findChild<QObject *>("dateEntryDialog"); QVERIFY(date);
        QVERIFY(QMetaObject::invokeMethod(date,"openEntry",Q_ARG(QVariant,3),Q_ARG(QVariant,QString("2026-09-09")),Q_ARG(QVariant,QString("Before"))));
        auto *entry=date->findChild<QObject *>("dateEntryText"); QVERIFY(entry); entry->setProperty("text","Unfinished date");
        QVERIFY(recovery.checkpoint()); QVERIFY(recovered.openRecovery(path,&draft));
        QCOMPARE(draft["date"].toMap()["text"].toString(),QString("Unfinished date"));
        QVERIFY(QMetaObject::invokeMethod(date,"close"));
        QVERIFY(QMetaObject::invokeMethod(window,"restoreRecoveryDraft",Q_ARG(QVariant,draft)));
        QTRY_VERIFY(date->property("opened").toBool());
        QCOMPARE(entry->property("text").toString(),QString("Unfinished date"));
        QVERIFY(QMetaObject::invokeMethod(date,"close"));
        recovery.remove(); QVERIFY(!QFileInfo::exists(path)); QVERIFY(!recovery.checkpoint());
    }
    void failedRecoveryKeepsWindowEditable() {
        QTemporaryDir dir;
        DocumentRecovery recovery(document,window,canvas,dir.filePath("missing/window.recovery"));
        QVERIFY(!recovery.prepareQuit());
        QVERIFY(!window->property("quitPending").toBool());
        QVERIFY(window->contentItem()->isEnabled());
        QVERIFY(window->isVisible());
        QVERIFY(document->error().contains("recovery"));
    }
    void headerButtonsDoNotShowFocusFeedback() {
        QTest::mouseMove(window,QPoint(window->width()/2,window->height()-100));
        for(const QString &name:{QString("fileMenuButton"),QString("addMenuButton"),QString("zoomPercentage"),QString("searchButton")}) {
            auto *button=window->findChild<QQuickItem *>(name); QVERIFY(button);
            QCOMPARE(button->property("focusPolicy").toInt(),int(Qt::NoFocus));
            button->forceActiveFocus(); QTRY_VERIFY(button->hasActiveFocus());
            QVERIFY(!button->property("hovered").toBool());
            auto *background=qvariant_cast<QObject *>(button->property("background")); QVERIFY(background);
            QCOMPARE(QQmlProperty(background,"border.width",qml->rootContext()).read().toReal(),0.);
            const QQmlProperty tooltip(button,"ToolTip.visible",qmlContext(button));
            QVERIFY(tooltip.isValid()); QVERIFY(!tooltip.read().toBool());
        }
        // Active toggles retain their state indication, independently of focus.
        auto *search=window->findChild<QQuickItem *>("searchButton");
        window->setProperty("searchOpen",true);
        auto *background=qvariant_cast<QObject *>(search->property("background"));
        QCOMPARE(QQmlProperty(background,"border.width",qml->rootContext()).read().toReal(),1.);
        window->setProperty("searchOpen",false); canvas->forceActiveFocus();
    }
    void documentHeaderTracksSaveAndEditing() {
        QTemporaryDir dir;
        auto *name = window->findChild<QQuickItem *>("documentNameLabel");
        auto *edited = window->findChild<QQuickItem *>("documentEditedLabel");
        QVERIFY(name); QVERIFY(edited);
        QVERIFY(document->save(dir.filePath("My project.omm")));
        QTRY_COMPARE(name->property("text").toString(), QString("My project"));
        QTRY_VERIFY(!edited->isVisible());
        auto *toolbar = window->findChild<QQuickItem *>("mainToolbar");
        QVERIFY(toolbar);
        QTRY_VERIFY(qAbs(name->mapToScene(QPointF(0, name->height()/2)).y()
            - toolbar->mapToScene(QPointF(0, toolbar->height()/2)).y()) < 1);
        canvas->beginEdit(1);
        QTRY_VERIFY(canvas->editing());
        QVERIFY(!window->property("documentEdited").toBool());
        type("Changed title");
        QTRY_VERIFY(window->property("documentEdited").toBool());
        QTRY_COMPARE(edited->opacity(),1.);
        const qreal dirtyY=name->y();
        const qreal cleanY=(name->parentItem()->height()-name->height())/2;
        QVERIFY(dirtyY<cleanY);
        QVERIFY(QMetaObject::invokeMethod(window, "saveDocument", Q_ARG(QVariant, false)));
        QVERIFY(edited->isVisible()); // Saving starts the fade instead of removing the label.
        QTest::qWait(60);
        QVERIFY(edited->opacity()>0 && edited->opacity()<1);
        QVERIFY(name->y()>dirtyY && name->y()<cleanY);
        QVERIFY(qAbs(name->y()-(cleanY-edited->opacity()*(edited->height()+2)/2))<.1);
        QTRY_VERIFY(!edited->isVisible());
        QTRY_VERIFY(qAbs(name->y()-cleanY)<.1);
        QVERIFY(!document->edited());
        if (QOperatingSystemVersion::currentType() == QOperatingSystemVersion::MacOS) {
            document->setText(1, "Changed again");
            QTRY_COMPARE(edited->opacity(), 1.);
            auto *mouse = window->findChild<QQuickItem *>("documentTitleMouse");
            QVERIFY(mouse);
            const QPoint position = mouse->mapToScene(QPointF(mouse->width()/2, mouse->height()/2)).toPoint();
            QTest::mouseMove(window, QPoint(window->width()/2, 150));
            QTest::qWait(180);
            const qreal initialX = name->mapToScene(QPointF()).x();
            const QPointF editedPosition = edited->mapToScene(QPointF());
            QTest::mouseMove(window, position);
            QTRY_VERIFY(name->mapToScene(QPointF()).x() > initialX + 19);
            QCOMPARE(edited->mapToScene(QPointF()), editedPosition);
            QSignalSpy menu(document, &Engine::nativeFolderMenuRequested);
            QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, position);
            QCOMPARE(menu.count(), 1);
            QTest::mouseMove(window, QPoint(window->width()/2, 150));
            QTRY_VERIFY(qAbs(name->mapToScene(QPointF()).x()-initialX) < 1);
            QCOMPARE(edited->mapToScene(QPointF()), editedPosition);
        }
    }
    void quitConfirmationWaitsForSessionAndCloseShortcutForgets() {
        QTemporaryDir dir;
        QVERIFY(document->save(dir.filePath("shortcuts.omm")));
        QSignalSpy votes(document, &Engine::quitDecision);
        QSignalSpy closes(document, &Engine::windowCloseApproved);
        QVERIFY(QMetaObject::invokeMethod(window, "requestClose", Q_ARG(QVariant, false), Q_ARG(QVariant, true)));
        QCOMPARE(votes.count(), 1); QVERIFY(votes.first().first().toBool());
        QVERIFY(window->isVisible()); QCOMPARE(closes.count(), 0);
        QVERIFY(QMetaObject::invokeMethod(window, "abortSessionQuit"));
        QVERIFY(window->contentItem()->isEnabled());
        QTest::keySequence(window, QKeySequence(QKeySequence::Close));
        QTRY_COMPARE(closes.count(), 1);
        QVERIFY(closes.first().first().toBool());
        QTRY_VERIFY(!window->isVisible());
        window->setProperty("allowClose", false);
        window->show();
    }
    void saveOverwritesOpenedDocument() {
        QTemporaryDir dir;
        const QString path = dir.filePath("existing.omm");
        QVERIFY(document->save(path));
        QVERIFY(document->open(path));
        document->setText(1, "Saved by shortcut");
        QTest::keySequence(window, QKeySequence(QKeySequence::Save));
        QTRY_VERIFY(!document->hasUnsavedChanges());
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.selectedText(), QString("Saved by shortcut"));
        document->setText(1, "Saved by toolbar");
        auto *fileButton=window->findChild<QQuickItem *>("fileMenuButton"); QVERIFY(fileButton);
        fileButton->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        auto *fileMenu=window->findChild<QObject *>("fileActionsMenu"); QVERIFY(fileMenu);
        QTRY_VERIFY(fileMenu->property("opened").toBool());
        auto *button = window->findChild<QQuickItem *>("saveDocumentButton");
        QVERIFY(button);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
        QTRY_VERIFY(!document->hasUnsavedChanges());
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.selectedText(), QString("Saved by toolbar"));
        QVERIFY(window->isVisible());
    }
    void mouseHandleStartsTypingInNewChild() {
        document->select(-1);
        const int parent=2, before=document->nodeCount();
        const auto rect=canvas->nodeRect(parent);
        const auto anchor=canvas->mapFromWorld({rect.right(),rect.center().y()});
        QTest::mouseMove(window,canvas->mapToScene(canvas->mapFromWorld(rect.center())).toPoint());
        QTRY_COMPARE(canvas->hoveredId(),parent);
        QTest::mouseMove(window,canvas->mapToScene(anchor+QPointF(18,0)).toPoint());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,canvas->mapToScene(anchor+QPointF(18,0)).toPoint());
        QCOMPARE(document->nodeCount(),before+1);
        QCOMPARE(document->nodes().value(document->selectedId()).parent,parent);
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Created with mouse");
        QTest::keyClick(window,Qt::Key_Escape);
        QTextDocument text; text.setHtml(document->selectedText());
        QCOMPARE(text.toPlainText(),QString("Created with mouse"));
    }
    void untouchedNewDocumentClosesWithoutPrompt() {
        Engine blank(nullptr,Engine::InitialContent::Blank);
        window->setProperty("controller",QVariant::fromValue(&blank));
        canvas->beginEdit(1);
        QTRY_VERIFY(editor->hasActiveFocus());
        QVERIFY(!blank.edited());
        QVERIFY(QMetaObject::invokeMethod(window,"requestClose",Q_ARG(QVariant,false),Q_ARG(QVariant,false)));
        QTRY_VERIFY(!window->isVisible());
        QVERIFY(!blank.edited());
        auto *dialog=window->findChild<QObject *>("closeConfirmation");
        QVERIFY(dialog); QVERIFY(!dialog->property("visible").toBool());
        window->setProperty("controller",QVariant::fromValue(document));
        window->setProperty("allowClose",false);
        window->show();
    }
    void closeConfirmationCanCancelOrSave() {
        QTemporaryDir dir;
        QVERIFY(!window->close());
        auto *dialog = window->findChild<QObject *>("closeConfirmation");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QVERIFY(window->isVisible());
        const QString path = dir.filePath("close.omm");
        QVERIFY(document->save(path));
        document->setText(1, "Save on close");
        QVERIFY(!window->close());
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(window, "saveBeforeClosing"));
        QTRY_VERIFY(!window->isVisible());
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.selectedText(), QString("Save on close"));
        window->setProperty("allowClose", false);
        window->show();
    }
    void initTestCase() {
        auto *shellTheme = new ShellTheme(shellThemeDirectory.path(), this);
        qmlRegisterSingletonInstance("Mindarchy", 1, 0, "ShellTheme", shellTheme);
        qmlRegisterUncreatableType<Engine>("Mindarchy", 1, 0, "Engine", "Provided by application");
        qmlRegisterType<MindCanvas>("Mindarchy", 1, 0, "MindCanvas");
        document = new Engine(this);
        qml = new QQmlApplicationEngine(this);
        qml->addImageProvider("recent",new RecentPreviewProvider);
        qml->rootContext()->setContextProperty("engine", document);
        qml->load(QUrl("qrc:/qml/Main.qml"));
        QVERIFY2(!qml->rootObjects().isEmpty(), "The full application QML must load");
        window = qobject_cast<QQuickWindow *>(qml->rootObjects().first());
        QVERIFY(window);
#ifdef Q_OS_WIN
        new WindowMenuBar(window);
#endif
        canvas = window->findChild<MindCanvas *>("mindCanvas");
        editor = window->findChild<QQuickItem *>("titleEditor");
        QVERIFY(canvas);
        QVERIFY(editor);
        if (qEnvironmentVariableIntValue("MINDMAP_LIVE_TEST_MS") > 0)
            window->showMaximized();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }
    void init() {
        canvas->endEdit();
        document->loadFixture(15);
        document->setLayout("Horizontal");
        document->setManual(false);
        document->setThemeId("lab");
        document->select(1);
        canvas->fit();
        if(QGuiApplication::platformName()=="cocoa") { window->requestActivate(); QVERIFY(QTest::qWaitForWindowActive(window)); }
        canvas->forceActiveFocus();
        QTest::qWait(350);
        QVERIFY(canvas->hasActiveFocus());
        QCOMPARE(window->activeFocusItem(), canvas);
        stage(QString::fromLatin1(QTest::currentTestFunction()));
    }
    void headerDragLeavesMacWindowButtons() {
        if(QGuiApplication::platformName()!="cocoa") QSKIP("macOS traffic lights");
        auto *drag=window->findChild<QQuickItem *>("headerDragArea");
        QVERIFY(drag);
        QVERIFY(drag->x()>=90);
        QVERIFY(drag->width()<window->width());
    }
    void windowsHeaderReservesWindowControls() {
#ifndef Q_OS_WIN
        QSKIP("Windows integrated frame");
#else
        QVERIFY(!window->flags().testFlag(Qt::ExpandedClientAreaHint));
        QVERIFY(window->flags().testFlag(Qt::CustomizeWindowHint));
        QVERIFY(!window->flags().testFlag(Qt::WindowTitleHint));
        QVERIFY(!window->flags().testFlag(Qt::FramelessWindowHint));
        auto *controls = window->findChild<QQuickItem *>("windowControls");
        QVERIFY(controls); QVERIFY(controls->isVisible());
        QCOMPARE(controls->width(), 138.0);
        auto *viewport = window->findChild<QQuickItem *>("toolbarViewport");
        auto *drag = window->findChild<QQuickItem *>("headerDragArea");
        QVERIFY(viewport); QVERIFY(drag); QVERIFY(drag->isEnabled());
        QVERIFY(window->width() - viewport->x() - viewport->width() >= 138);
        QVERIFY(!viewport->property("interactive").toBool());
#endif
    }
    void windowsMenuOnlyAppearsOnAlt() {
#ifndef Q_OS_WIN
        QSKIP("Windows menu behavior");
#else
        QVERIFY(!window->property("windowsMenuVisible").toBool());
        QCOMPARE(window->property("windowsMenuHeight").toDouble(), 0.0);
        QTest::keyClick(window, Qt::Key_Alt);
        QVERIFY(window->property("windowsMenuVisible").toBool());
        QVERIFY(window->property("windowsMenuHeight").toDouble() > 0);
        QTest::keyClick(window, Qt::Key_Alt);
        QVERIFY(!window->property("windowsMenuVisible").toBool());
        QVERIFY(canvas->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Alt);
        QTest::keyClick(window, Qt::Key_Escape);
        QVERIFY(!window->property("windowsMenuVisible").toBool());
        QVERIFY(canvas->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Alt);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(350, 200));
        QVERIFY(!window->property("windowsMenuVisible").toBool());
#endif
    }
    void systemThemePrefersStateDirectoryAndHandlesMigrationLive() {
        QTemporaryDir dir;
        const auto state=dir.filePath("state/current"), legacy=dir.filePath("config/current");
        auto write=[](const QString &root,const QByteArray &background) {
            QDir().mkpath(root+"/theme"); QFile file(root+"/theme/colors.toml");
            if(!file.open(QIODevice::WriteOnly)) return false;
            return file.write("background = '"+background+"'\nforeground = '#e6d9db'\naccent = '#f38d70'\nred = '#fd6883'\n")>0;
        };
        QVERIFY(write(legacy,"#111111"));
        ShellTheme theme(QStringList{state,legacy});
        QCOMPARE(theme.colors()["#172129"].value<QColor>(),QColor("#111111"));
        QVERIFY(write(state,"#2c2525"));
        QTRY_COMPARE_WITH_TIMEOUT(theme.colors()["#172129"].value<QColor>(),QColor("#2c2525"),2500);
        QCOMPARE(theme.colors()["#f08b83"].value<QColor>(),QColor("#fd6883"));
        QVERIFY(QDir(state+"/theme").removeRecursively());
        QTest::qWait(1200);
        QCOMPARE(theme.colors()["#172129"].value<QColor>(),QColor("#2c2525"));
        QVERIFY(write(state,"#faf4ed"));
        QTRY_COMPARE_WITH_TIMEOUT(theme.colors()["#172129"].value<QColor>(),QColor("#faf4ed"),2500);
        QVERIFY(write(legacy,"#000000")); QTest::qWait(1200);
        QCOMPARE(theme.colors()["#172129"].value<QColor>(),QColor("#faf4ed"));
    }
    void systemThemeUpdatesShellLive() {
        const QString root=shellThemeDirectory.path();
        auto writeTheme=[&](QString directory,QByteArray background,QByteArray foreground) {
            QDir().mkpath(directory);
            QFile file(directory+"/colors.toml");
            if(!file.open(QIODevice::WriteOnly)) return false;
            return file.write("background = '"+background+"'\nforeground = '"+foreground+"'\naccent = '#7aa2f7'\n")>0;
        };
        auto *toolbar=window->findChild<QQuickItem *>("mainToolbar"); QVERIFY(toolbar);
        const auto documentTheme=document->themeId();
        const auto mapPath=root+"/map.omm";
        QVERIFY(document->save(mapPath));
        QFile mapFile(mapPath); QVERIFY(mapFile.open(QIODevice::ReadOnly));
        const auto before=mapFile.readAll(); mapFile.close();
        const auto zoom=canvas->zoom();
        QVERIFY(writeTheme(root+"/theme","#1a1b26","#a9b1d6"));
        QTRY_COMPARE_WITH_TIMEOUT(toolbar->property("color").value<QColor>(),QColor("#1a1b26"),2500);
        QCOMPARE(window->property("ink").value<QColor>(),QColor("#a9b1d6"));
        // Omarchy removes the current directory and renames a freshly generated one.
        QVERIFY(writeTheme(root+"/next-theme","#faf4ed","#575279"));
        QVERIFY(QDir(root+"/theme").removeRecursively());
        QVERIFY(QDir().rename(root+"/next-theme",root+"/theme"));
        QTRY_COMPARE_WITH_TIMEOUT(toolbar->property("color").value<QColor>(),QColor("#faf4ed"),2500);
        QCOMPARE(window->property("ink").value<QColor>(),QColor("#575279"));
        if (const auto screenshot=qEnvironmentVariable("MINDARCHY_TEST_SCREENSHOT"); !screenshot.isEmpty()) {
            QTest::qWait(100); QVERIFY(window->grabWindow().save(screenshot));
        }
        QCOMPARE(document->themeId(),documentTheme);
        QVERIFY(!document->hasUnsavedChanges());
        QVERIFY(document->save(mapPath));
        QVERIFY(mapFile.open(QIODevice::ReadOnly)); QCOMPARE(mapFile.readAll(),before); mapFile.close();
        QCOMPARE(canvas->zoom(),zoom);
        QFile malformed(root+"/theme/colors.toml"); QVERIFY(malformed.open(QIODevice::WriteOnly));
        malformed.write("background = 'invalid'"); malformed.close();
        QTest::qWait(1200);
        QCOMPARE(toolbar->property("color").value<QColor>(),QColor("#faf4ed"));
        QVERIFY(writeTheme(root+"/theme","#1a1b26","#a9b1d6"));
        QTRY_COMPARE_WITH_TIMEOUT(toolbar->property("color").value<QColor>(),QColor("#1a1b26"),2500);
    }
    void sharedTabStripGeometry_data() {
        QTest::addColumn<bool>("outlineRequested");
        QTest::addColumn<bool>("inspectorRequested");
        QTest::addColumn<int>("windowWidth");
        QTest::addColumn<bool>("outlineShown");
        QTest::addColumn<bool>("inspectorShown");
        QTest::newRow("none") << false << false << 1380 << false << false;
        QTest::newRow("outline") << true << false << 1380 << true << false;
        QTest::newRow("inspector") << false << true << 1380 << false << true;
        QTest::newRow("both") << true << true << 1380 << true << true;
        QTest::newRow("minimum-both") << true << true << 600 << true << false;
        QTest::newRow("minimum-inspector") << false << true << 600 << false << true;
    }
    void sharedTabStripGeometry() {
        QFETCH(bool, outlineRequested); QFETCH(bool, inspectorRequested);
        QFETCH(int, windowWidth); QFETCH(bool, outlineShown); QFETCH(bool, inspectorShown);
        const auto originalSize = window->size();
        const auto oldTabs = window->property("documentTabs");
        const auto oldOutline = window->property("outlineVisible");
        const auto oldInspector = window->property("inspectorVisible");
        auto restore = qScopeGuard([&] {
            window->setProperty("documentTabs", oldTabs);
            window->setProperty("outlineVisible", oldOutline);
            window->setProperty("inspectorVisible", oldInspector);
            window->resize(originalSize);
        });
        window->setProperty("outlineVisible", outlineRequested);
        window->setProperty("inspectorVisible", inspectorRequested);
        window->resize(windowWidth, 900);
        auto *strip = findVisual(window->contentItem(), "documentTabStrip");
        auto *toolbar = findVisual(window->contentItem(), "mainToolbar");
        auto *left = findVisual(window->contentItem(), "outlineSidebar");
        auto *right = findVisual(window->contentItem(), "inspectorSidebar");
        QVERIFY(strip); QVERIFY(toolbar); QVERIFY(left); QVERIFY(right);
        const QVariantList oneTab{QVariantMap{{"id", 1}, {"title", "First"}, {"edited", false}}};
        auto twoTabs = oneTab;
        twoTabs.append(QVariantMap{{"id", 2}, {"title", "Second"}, {"edited", false}});
        window->setProperty("documentTabs", oneTab);
        QTRY_VERIFY(!strip->isVisible()); QCOMPARE(strip->height(), 0.0);
        QTRY_COMPARE(canvas->mapToScene(QPointF()).y(), toolbar->mapToScene(QPointF(0, toolbar->height())).y());
        window->setProperty("documentTabs", twoTabs);
        QTRY_COMPARE(left->isVisible(), outlineShown);
        QTRY_COMPARE(right->isVisible(), inspectorShown);
        QTest::qWait(100);
        const auto rect = [](QQuickItem *item) { return item->mapRectToScene(item->boundingRect()); };
        QCOMPARE(toolbar->height(), 60.0);
        QVERIFY(strip->isVisible()); QCOMPARE(strip->height(), 36.0);
        QCOMPARE(rect(strip).top(), rect(toolbar).bottom());
        QCOMPARE(rect(strip).left(), rect(canvas).left());
        QCOMPARE(rect(strip).right(), rect(canvas).right());
        QCOMPARE(rect(strip).bottom(), rect(canvas).top());
        QVERIFY(canvas->width() >= 320);
        if (outlineShown) {
            QCOMPARE(left->width(), 224.0);
            QCOMPARE(rect(left).top(), rect(toolbar).bottom());
            QVERIFY(rect(left).right() <= rect(strip).left());
            QVERIFY(!rect(left).intersects(rect(strip)));
        }
        if (inspectorShown) {
            QCOMPARE(right->width(), 274.0);
            QCOMPARE(rect(right).top(), rect(toolbar).bottom());
            QVERIFY(rect(right).left() >= rect(strip).right());
            QVERIFY(!rect(right).intersects(rect(strip)));
        }
        QCOMPARE(window->property("outlineVisible").toBool(), outlineRequested);
        QCOMPARE(window->property("inspectorVisible").toBool(), inspectorRequested);
        window->setProperty("documentTabs", oneTab);
        QTRY_VERIFY(!strip->isVisible()); QCOMPARE(strip->height(), 0.0);
        QTRY_COMPARE(rect(canvas).top(), rect(toolbar).bottom());
        window->resize(1380, 900);
        QTRY_COMPARE(left->isVisible(), outlineRequested);
        QTRY_COMPARE(right->isVisible(), inspectorRequested);
    }
    void sharedTabOverflowFocusAndTargetedClose() {
        const auto oldTabs = window->property("documentTabs");
        const auto oldId = window->property("documentTabId");
        auto restore = qScopeGuard([&] {
            window->setProperty("documentTabs", oldTabs);
            window->setProperty("documentTabId", oldId);
            canvas->forceActiveFocus();
        });
        QVariantList documents;
        for (int i = 1; i <= 20; ++i)
            documents.append(QVariantMap{{"id", i}, {"title", QString("Document %1 with a long descriptive title").arg(i)}, {"edited", i == 5}});
        window->setProperty("documentTabs", documents);
        window->setProperty("documentTabId", 20);
        auto *strip = findVisual(window->contentItem(), "documentTabStrip");
        auto *scroller = findVisual(window->contentItem(), "documentTabScroller");
        auto *list = findVisual(window->contentItem(), "documentTabListButton");
        QVERIFY(strip); QVERIFY(scroller); QVERIFY(list);
        QTRY_VERIFY(list->isVisible());
        QTRY_VERIFY(scroller->property("contentX").toDouble() > 0);
        auto *last = findVisual(window->contentItem(), "document-tab-20");
        QVERIFY(last); last->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Home);
        auto *first = findVisual(window->contentItem(), "document-tab-1");
        QVERIFY(first); QTRY_VERIFY(first->hasActiveFocus());
        QSignalSpy actions(document, &Engine::tabActionRequested);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(actions.count(), 1);
        QCOMPARE(actions.last().at(0).toString(), QString("activate"));
        QCOMPARE(actions.last().at(1).toLongLong(), 1);
        QTest::keyClick(window, Qt::Key_End); QTRY_VERIFY(last->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Escape); QTRY_VERIFY(canvas->hasActiveFocus());
        auto *close = findVisual(window->contentItem(), "close-document-tab-5");
        QVERIFY(close); QVERIFY(QMetaObject::invokeMethod(close, "clicked"));
        QCOMPARE(actions.count(), 2);
        QCOMPARE(actions.last().at(0).toString(), QString("close"));
        QCOMPARE(actions.last().at(1).toLongLong(), 5);
        first->forceActiveFocus();
        QVERIFY(QMetaObject::invokeMethod(strip, "reveal", Q_ARG(QVariant, 0)));
        QTest::qWait(50);
        QSignalSpy reordered(window, SIGNAL(tabMoveRequested(double,int)));
        const auto from = first->mapToScene(QPointF(30, first->height() / 2)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(window, from + QPoint(180, 0), 50);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, from + QPoint(180, 0));
        QCOMPARE(reordered.count(), 1);
        QCOMPARE(reordered.first().at(0).toDouble(), 1.0);
        QCOMPARE(reordered.first().at(1).toInt(), 1);
        QCOMPARE(actions.count(), 2); // Dragging never also activates the source tab.
        auto *create = findVisual(window->contentItem(), "newTabButton");
        QVERIFY(create); QVERIFY(QMetaObject::invokeMethod(create, "clicked"));
        QCOMPARE(actions.count(), 3);
        QCOMPARE(actions.last().at(0).toString(), QString("new"));
    }
    void resizingWindowPreservesUserZoom() {
        const auto originalSize=window->size();
        const bool outline=window->property("outlineVisible").toBool();
        const bool inspector=window->property("inspectorVisible").toBool();
        canvas->resetZoom();
        canvas->zoomIn();
        canvas->panBy(135,-80);
        const double zoom=canvas->zoom();
        const auto center=canvas->mapToWorld({canvas->width()/2,canvas->height()/2});
        auto checkView=[&] {
            QCOMPARE(canvas->zoom(),zoom);
            QVERIFY(QLineF(canvas->mapToWorld({canvas->width()/2,canvas->height()/2}),center).length()<.000001);
        };
        for (const QSize size : {QSize(950,650),QSize(1380,900),QSize(700,750)}) {
            window->resize(size);
            QTest::qWait(250);
            checkView();
        }
        window->setProperty("outlineVisible",!outline);
        window->setProperty("inspectorVisible",!inspector);
        QTest::qWait(250);
        checkView();
        window->setProperty("outlineVisible",outline);
        window->setProperty("inspectorVisible",inspector);
        window->resize(originalSize);
    }
    void compactZoomDropdownActions() {
        auto *percentage=window->findChild<QQuickItem *>("zoomPercentage"); QVERIFY(percentage);
        auto *menu=window->findChild<QObject *>("zoomMenu"); QVERIFY(menu);
        auto choose=[&](const char *name,bool staysOpen=false) {
            if(!menu->property("visible").toBool())
                QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,percentage->mapToScene(QPointF(percentage->width()/2,percentage->height()/2)).toPoint());
            QTRY_VERIFY(menu->property("opened").toBool());
            auto *item=window->findChild<QQuickItem *>(name); QVERIFY(item);
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
            QTest::qWait(100);
            QCOMPARE(menu->property("visible").toBool(),staysOpen);
            if(const auto path=qEnvironmentVariable("MINDARCHY_ZOOM_MENU_SCREENSHOT"); staysOpen && !path.isEmpty())
                QVERIFY(window->grabWindow().save(path));
        };
        canvas->resetZoom(); choose("zoomInAction",true); QCOMPARE(canvas->zoom(),1.25);
        choose("zoomInAction",true); QCOMPARE(canvas->zoom(),1.5625);
        choose("zoomOutAction",true); QCOMPARE(canvas->zoom(),1.25);
        choose("zoomOutAction",true); QCOMPARE(canvas->zoom(),1.);
        choose("zoomInAction",true);
        QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!menu->property("visible").toBool());
        choose("zoomActualSizeAction"); QCOMPARE(canvas->zoom(),1.);
        canvas->fit(); const auto fitted=canvas->zoom();
        canvas->zoomIn(); choose("zoomFitAction"); QCOMPARE(canvas->zoom(),fitted);
    }
    void fileMenuKeyboardAndExport() {
        auto *button=window->findChild<QQuickItem *>("fileMenuButton"); QVERIFY(button);
        QVERIFY(button->height()<=36);
        auto *menu=window->findChild<QObject *>("fileActionsMenu"); QVERIFY(menu);
        button->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Return);
        QTRY_VERIFY(menu->property("opened").toBool());
        QCOMPARE(menu->property("count").toInt(),5);
        auto *first=window->findChild<QQuickItem *>("newDocumentButton"); QVERIFY(first);
        QTRY_VERIFY(first->hasActiveFocus());
        if(const auto path=qEnvironmentVariable("MINDARCHY_FILE_MENU_SCREENSHOT");!path.isEmpty()) {
            QTest::qWait(180); QVERIFY(window->grabWindow().save(path));
        }
        QTest::keyClick(window,Qt::Key_Escape); QTRY_VERIFY(!menu->property("visible").toBool());
        button->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Down);
        QTRY_VERIFY(menu->property("opened").toBool());
        auto *exportItem=window->findChild<QQuickItem *>("exportDocumentButton"); QVERIFY(exportItem);
        exportItem->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        auto *dialog=window->findChild<QObject *>("exportImageDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QTest::keyClick(QGuiApplication::focusWindow()?QGuiApplication::focusWindow():window,Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool()); window->requestActivate(); canvas->forceActiveFocus();
    }
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
    void toolbarGroupsAndNewDocument() {
        auto *left=window->findChild<QQuickItem *>("documentActions");
        auto *center=window->findChild<QQuickItem *>("editingActions");
        auto *right=window->findChild<QQuickItem *>("panelActions");
        auto *button=window->findChild<QQuickItem *>("newDocumentButton");
        QVERIFY(left); QVERIFY(center); QVERIFY(right); QVERIFY(button);
        auto *zoom=window->findChild<QQuickItem *>("zoomControls"); QVERIFY(zoom);
        QVERIFY(zoom->mapToScene(QPointF(0,zoom->height())).y()<=canvas->mapToScene(QPointF()).y());
        auto *percentage=window->findChild<QQuickItem *>("zoomPercentage"); QVERIFY(percentage);
        canvas->zoomIn(); const auto zoomBeforeMenu=canvas->zoom();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,percentage->mapToScene(QPointF(percentage->width()/2,percentage->height()/2)).toPoint());
        auto *menu=window->findChild<QObject *>("zoomMenu"); QVERIFY(menu);
        QTRY_VERIFY(menu->property("opened").toBool()); QCOMPARE(canvas->zoom(),zoomBeforeMenu);
        auto *actualSize=window->findChild<QQuickItem *>("zoomActualSizeAction"); QVERIFY(actualSize);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,actualSize->mapToScene(QPointF(actualSize->width()/2,actualSize->height()/2)).toPoint());
        QCOMPARE(canvas->zoom(),1.);
        QTRY_VERIFY(!menu->property("visible").toBool());
        QVERIFY(zoom->width()<=80);
        auto x=[](QQuickItem *item) {return item->mapToScene(QPointF()).x();};
        QVERIFY(x(left)+left->width()<x(center));
        QVERIFY(x(center)+center->width()<x(right));
        QVERIFY(qAbs(x(center)+center->width()/2-window->width()/2)<1);
        QSignalSpy requested(document,&Engine::newDocumentRequested);
        auto *fileButton=window->findChild<QQuickItem *>("fileMenuButton"); QVERIFY(fileButton);
        fileButton->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        auto *fileMenu=window->findChild<QObject *>("fileActionsMenu"); QVERIFY(fileMenu);
        QTRY_VERIFY(fileMenu->property("opened").toBool());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        QCOMPARE(requested.count(),1); QCOMPARE(document->nodeCount(),15);
        QTest::keySequence(window,QKeySequence(QKeySequence::New));
        QTRY_COMPARE(requested.count(),2);
        const auto originalSize=window->size();
        for(int width:{600,950,1380}) {
            window->resize(width,900); QTest::qWait(100);
            QVERIFY(x(left)+left->width()<x(center));
            QVERIFY(x(center)+center->width()<x(right));
            QVERIFY(x(left)>=0);
            if(width>=950) QVERIFY(x(right)+right->width()<=window->width());
            else QVERIFY(right->parentItem()->width()>=x(right)+right->width()-x(left));
        }
        window->resize(originalSize);
    }
    void cleanupTestCase() {
        delete qml;
        qml = nullptr;
    }
    void convertNodeUsingInspector() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs); tabs->setProperty("currentIndex",1);
        document->select(2); const auto title=document->selectedText(); const auto count=document->nodeCount();
        auto choose=[&](QString name) {
            auto *item=findVisual(window->contentItem(),"node-type-"+name);
            if(!item) return false; item->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space); return true;
        };
        QVERIFY(choose("Task")); QTRY_VERIFY(document->selectedTask());
        auto *completed=window->findChild<QQuickItem *>("taskCompleted"); QVERIFY(completed); QVERIFY(!completed->isVisible());
        const auto tasks=document->nodes().value(2).children;
        for(int child:tasks) {
            document->select(child); QVERIFY(completed->isVisible());
            if(!document->selectedChecked()) { completed->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space); }
        }
        document->select(2); QVERIFY(document->selectedChecked()); QVERIFY(!completed->isVisible());
        const QString dir=qEnvironmentVariable("MINDMAP_TYPE_SCREENSHOTS");
        if(!dir.isEmpty()) {QDir().mkpath(dir); QTest::qWait(150); QVERIFY(window->grabWindow().save(dir+"/task.png"));}
        QVERIFY(choose("Date")); QTRY_COMPARE(document->selectedKind(),QString("date")); QCOMPARE(document->nodeCount(),count);
        QVERIFY(!document->selectedTask()); QVERIFY(!completed->isVisible());
        if(!dir.isEmpty()) {canvas->fit(); QTest::qWait(250); QVERIFY(window->grabWindow().save(dir+"/date.png"));}
        document->undo(); QVERIFY(document->selectedTask()); QVERIFY(document->selectedChecked());
        QVERIFY(choose("Text")); QTRY_COMPARE(document->selectedKind(),QString("text"));
        QVERIFY(!document->selectedTask()); QCOMPARE(document->selectedText(),title);
        auto *options=window->findChild<QQuickItem *>("nodeTypeOptions"); QVERIFY(options); QVERIFY(!options->isVisible());
        tabs->setProperty("currentIndex",0);
    }
    void dateNodeCreateEditHoverAndDrag() {
        document->addDateNode("month"); canvas->revealNode(document->selectedId());
        const int id=document->selectedId(); QCOMPARE(document->nodes().value(id).kind,QString("date"));
        QVERIFY(!canvas->editing()); QVERIFY(document->configureDateNode(id,"month","2026-09-08"));
        canvas->fit(); QTest::qWait(250);
        auto *dialog=window->findChild<QObject *>("dateEntryDialog"); QVERIFY(dialog);
        auto *field=window->findChild<QQuickItem *>("dateEntryText"); QVERIFY(field);
        auto button=[this](const QString &name) { auto *item=window->findChild<QQuickItem *>(name); if(item) {item->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);} return item!=nullptr; };
        const auto days=Calendar::days(document->nodes().value(id).calendar);
        const int index=days.indexOf(QDate(2026,9,8)); QVERIFY(index>=0);
        auto point=[&] { return canvas->mapToScene(canvas->mapFromWorld(canvas->nodeRect(id).topLeft()+Calendar::cell(index,Calendar::weekGutter(document->nodes().value(id).calendar)).center()*(document->appearance(id).fontSize/15.))).toPoint(); };
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point());
        QTRY_VERIFY(dialog->property("opened").toBool()); QVERIFY(!dialog->property("existing").toBool());
        field->setProperty("text","Design review"); QVERIFY(button("dateEntryCancel"));
        QTRY_VERIFY(!dialog->property("opened").toBool()); QVERIFY(document->dateEntry(id,"2026-09-08").isEmpty());
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point()); QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text","Design review"); QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Design review"));
        QTest::mouseMove(window,point()+QPoint(0,40)); QTest::mouseMove(window,point());
        QTRY_COMPARE(canvas->dateHoverText(),QString("Design review"));
        auto *tooltip=window->findChild<QObject *>("dateEntryTooltip"); QVERIFY(tooltip);
        QCOMPARE(tooltip->property("delay").toInt(),0);
        QVERIFY(tooltip->property("visible").toBool());
        QCOMPARE(tooltip->property("text").toString(),QString("Design review"));
        const QString dir=qEnvironmentVariable("MINDMAP_DATE_SCREENSHOTS");
        if(!dir.isEmpty()) {QDir().mkpath(dir); QTest::qWait(600); QVERIFY(window->grabWindow().save(dir+"/month-hover.png"));}
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point()); QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(dialog->property("existing").toBool()); QCOMPARE(field->property("text").toString(),QString("Design review"));
        field->setProperty("text","Updated review");
        if(!dir.isEmpty()) {QTest::qWait(100); QVERIFY(window->grabWindow().save(dir+"/edit-entry.png"));}
        QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Updated review"));
        document->setManual(true); QTest::qWait(250); const auto before=document->nodes().value(id).rect;
        const auto from=point(); QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(window,from+QPoint(35,20),60); QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from+QPoint(35,20));
        QVERIFY(!dialog->property("opened").toBool()); QVERIFY(document->nodes().value(id).rect!=before);
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Updated review"));
        QVERIFY(document->configureDateNode(id,"week","2026-09-08")); canvas->fit(); QTest::qWait(250);
        if(!dir.isEmpty()) QVERIFY(window->grabWindow().save(dir+"/week.png"));
        auto clickControl=[&](QRectF control) {
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                canvas->mapToScene(canvas->mapFromWorld(canvas->nodeRect(id).topLeft()+control.center())).toPoint());
            QTest::qWait(200);
        };
        // The old arrow locations are inert in both calendar views.
        for(const QString &mode : {QString("week"),QString("month")}) {
            QVERIFY(document->configureDateNode(id,mode,"2026-09-08"));
            canvas->fit(); QTest::qWait(250);
            clickControl(QRectF(256,10,28,28));
            QCOMPARE(document->nodes().value(id).calendar.anchor,QDate(2026,9,8));
            clickControl(QRectF(10,10,28,28));
            QCOMPARE(document->nodes().value(id).calendar.anchor,QDate(2026,9,8));
            QVERIFY(!dialog->property("opened").toBool());
        }
        QVERIFY(!window->findChild<QObject *>("dateNodePrevious"));
        QVERIFY(!window->findChild<QObject *>("dateNodeNext"));
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs); tabs->setProperty("currentIndex",1);
        auto *view=window->findChild<QQuickItem *>("dateNodeMonth"); QVERIFY(view);
        view->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_COMPARE(document->nodes().value(id).calendar.view,QString("month"));
        QCOMPARE(document->dateEntry(id,"2026-09-08"),QString("Updated review"));
        if(!dir.isEmpty()) {QTest::qWait(250); QVERIFY(window->grabWindow().save(dir+"/date-inspector.png"));}
        tabs->setProperty("currentIndex",0);
        canvas->editDateEntry(id,"2026-09-08"); QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(button("dateEntryRemove")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QVERIFY(document->dateEntry(id,"2026-09-08").isEmpty());
        canvas->editDateEntry(id,"2026-09-08"); QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text","12.5"); QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(Calendar::totals(document->nodes().value(id).calendar).month,12.5);
        const auto numericSize=document->nodes().value(id).rect.size();
        canvas->editDateEntry(id,"2026-09-08"); QTRY_VERIFY(dialog->property("opened").toBool());
        field->setProperty("text","7.5"); QVERIFY(button("dateEntrySave")); QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(Calendar::totals(document->nodes().value(id).calendar).month,7.5);
        QCOMPARE(document->nodes().value(id).rect.size(),numericSize);
        QVERIFY(document->setDateEntry(id,"2026-09-01","10"));
        QVERIFY(document->setDateEntry(id,"2026-09-16","-2.5"));
        canvas->fit(); QTest::qWait(250);
        if(!dir.isEmpty()) QVERIFY(window->grabWindow().save(dir+"/month-sums.png"));
    }
    void dateInspectorStylesAndScaledDayTargets() {
        document->select(2); QVERIFY(document->setNodeKind(2,"date"));
        QVERIFY(document->configureDateNode(2,"month","2026-09-08"));
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); QVERIFY(tabs);
        tabs->setProperty("currentIndex",1);
        auto *panel=window->findChild<QQuickItem *>("nodeStylePanel"); QVERIFY(panel);
        for(const auto &name : {"style-borderWidth","style-border","style-fontFamily","style-fontSize","style-textColor"}) {
            auto *control=window->findChild<QQuickItem *>(name); QVERIFY(control);
            QTRY_VERIFY(control->isVisible());
        }
        QVERIFY(!window->findChild<QQuickItem *>("style-fixedWidth")->isVisible());
        const auto before=document->contentSize(2);
        auto apply=[panel](QString key,QVariant value) {
            QVariant accepted;
            return QMetaObject::invokeMethod(panel,"apply",Q_RETURN_ARG(QVariant,accepted),
                Q_ARG(QVariant,key),Q_ARG(QVariant,value)) && accepted.toBool();
        };
        QVERIFY(!apply("fontSize",30)); QVERIFY(apply("italic",true));
        QVERIFY(apply("textColor",QString("#683286"))); QVERIFY(apply("borderWidth",3));
        QCOMPARE(document->contentSize(2),before);
        QVERIFY(Calendar::textFont(document->nodes().value(2).text).italic());
        QCOMPARE(document->appearance(2).text,QColor("#683286"));
        QCOMPARE(document->appearance(2).borderWidth,3.);
        canvas->fit(); QTest::qWait(300);
        const auto data=document->nodes().value(2).calendar;
        const int index=Calendar::days(data).indexOf(QDate(2026,9,8));
        const auto cell=Calendar::cell(index,Calendar::weekGutter(data));
        const auto point=canvas->mapToScene(canvas->mapFromWorld(canvas->nodeRect(2).topLeft()+cell.center()*1.2)).toPoint();
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point);
        auto *dialog=window->findChild<QObject *>("dateEntryDialog"); QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *cancel=window->findChild<QQuickItem *>("dateEntryCancel"); QVERIFY(cancel);
        cancel->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(!dialog->property("opened").toBool());
        document->resetNodeStyle(); QCOMPARE(document->contentSize(2),before);
    }
    void welcomeKeyboardNavigationAndActivation() {
        QTemporaryDir directory; document->setRecentDirectory(directory.path());
        for(int i=0;i<6;++i) {
            Engine map; map.setRecentDirectory(directory.path());
            QVERIFY(map.save(directory.filePath(QString("Keyboard %1.omm").arg(i))));
        }
        QVERIFY(QFile::remove(directory.filePath("Keyboard 4.omm"))); // second card unavailable
        window->resize(1380,900); window->setProperty("welcomeVisible",true);
        auto focused=[this](const QString &name) { return window->activeFocusItem() && window->activeFocusItem()->objectName()==name; };
        QTRY_VERIFY(focused("welcomeCard0"));
        QTest::keyClick(window,Qt::Key_Right); QVERIFY(focused("welcomeCard2"));
        QTest::keyClick(window,Qt::Key_Down); QVERIFY(focused("welcomeCard5"));
        QTest::keyClick(window,Qt::Key_Up); QVERIFY(focused("welcomeCard2"));
        QTest::keyClick(window,Qt::Key_Home); QVERIFY(focused("welcomeCard0"));
        QTest::keyClick(window,Qt::Key_End); QVERIFY(focused("welcomeCard5"));
        QTest::keyClick(window,Qt::Key_Tab); QVERIFY(focused("welcomeNewMap"));
        QTest::keyClick(window,Qt::Key_Tab); QVERIFY(focused("welcomeOpenMap"));
        QTest::keyClick(window,Qt::Key_Return);
        auto *picker=window->findChild<QObject *>("welcomeFilePicker"); QVERIFY(picker);
        QTRY_VERIFY(picker->property("visible").toBool());
        QTest::keyClick(QGuiApplication::focusWindow()?QGuiApplication::focusWindow():window,Qt::Key_Escape);
        QTRY_VERIFY(!picker->property("visible").toBool());
        QTRY_VERIFY(focused("welcomeOpenMap"));
        QTest::keyClick(window,Qt::Key_Tab); QVERIFY(focused("welcomeCard0"));
        QTest::keyClick(window,Qt::Key_Backtab,Qt::ShiftModifier); QVERIFY(focused("welcomeOpenMap"));
        QTest::keyClick(window,Qt::Key_Escape); QVERIFY(focused("welcomeNewMap"));
        window->resize(600,640); QTest::keyClick(window,Qt::Key_End);
        QTRY_VERIFY(focused("welcomeCard5"));
        auto *last=window->activeFocusItem();
        QTRY_VERIFY(last->mapToScene(QPointF(0,last->height())).y()<=window->height());
        QTest::keyClick(window,Qt::Key_Home); QTRY_VERIFY(focused("welcomeCard0"));
        QTest::keyClick(window,Qt::Key_Return);
        QTRY_VERIFY(!window->property("welcomeVisible").toBool());
        QCOMPARE(document->documentPath(),QFileInfo(directory.filePath("Keyboard 5.omm")).canonicalFilePath());
        window->resize(1380,900); document->setRecentDirectory(""); document->loadFixture(1);
        window->setProperty("welcomeVisible",true); QTRY_VERIFY(focused("welcomeNewMap"));
        QTest::keyClick(window,Qt::Key_Return);
        QTRY_VERIFY(!window->property("welcomeVisible").toBool()); QTRY_VERIFY(canvas->editing()); canvas->endEdit();
    }
    void welcomeGridPreviewsAndOpening() {
        QTemporaryDir directory;
        QSettings views(directory.filePath("views.ini"),QSettings::IniFormat);
        ViewportState viewport(canvas,document,&views);
        document->setRecentDirectory(directory.path());
        for(int i=0;i<6;++i) {
            Engine map; map.loadFixture(8+i); map.setLayout(i%2?"Vertical":"Horizontal"); map.setThemeId(i%2?"porcelain":"sage");
            map.setRecentDirectory(directory.path());
            QVERIFY(map.save(directory.filePath(QString("Project %1.omm").arg(i+1))));
        }
        const QVariantList expectedView{1.4,234.,-120.};
        views.setValue(ViewportState::keyFor(directory.filePath("Project 6.omm")),expectedView); views.sync();
        window->setProperty("welcomeVisible",true);
        auto *home=window->findChild<QQuickItem *>("welcomeScreen"); QTRY_VERIFY(home);
        auto *grid=window->findChild<QQuickItem *>("welcomeGrid"); QVERIFY(grid);
        QCOMPARE(grid->property("columns").toInt(),3);
        QQuickItem *preview=nullptr; QTRY_VERIFY((preview=findVisual(home,"welcomeThumbnail0")));
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("status").toInt(),1,15000);
        if(const auto path=qEnvironmentVariable("MINDARCHY_WELCOME_SCREENSHOT");!path.isEmpty()) {
            QTest::qWait(200); QVERIFY(window->grabWindow().save(path));
        }
        window->resize(800,900); QTRY_COMPARE(grid->property("columns").toInt(),2);
        window->resize(600,900); QTRY_COMPARE(grid->property("columns").toInt(),1);
        window->resize(1380,900); QTRY_COMPARE(grid->property("columns").toInt(),3);
        auto *card=findVisual(home,"welcomeCard0"); QVERIFY(card);
        card->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(!window->property("welcomeVisible").toBool());
        QCOMPARE(document->documentPath(),QFileInfo(directory.filePath("Project 6.omm")).canonicalFilePath());
        QVERIFY(document->nodeCount()>1);
        const auto actualView=canvas->persistentView();
        for(int i=0;i<3;++i) QVERIFY(qAbs(actualView[i].toDouble()-expectedView[i].toDouble())<1e-8);
        // Empty-state creation is tested with a fresh document fixture.
        document->setRecentDirectory(""); document->loadFixture(1);
        window->setProperty("welcomeVisible",true);
        auto *create=window->findChild<QQuickItem *>("welcomeNewMap"); QVERIFY(create);
        create->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(!window->property("welcomeVisible").toBool());
        QTRY_VERIFY(canvas->editing()); canvas->endEdit();
    }
    void pastedFontSizesFollowNodeDepth() {
        document->select(2); canvas->beginEdit(2);
        auto *editor=window->findChild<QQuickItem *>("titleEditor"); QVERIFY(editor);
        editor->setProperty("text",QString("<span style='font-size:72pt;font-weight:700'>Pasted title</span>"));
        QTextDocument text; text.setHtml(editor->property("text").toString());
        QTextCursor cursor(&text); cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
        QCOMPARE(cursor.charFormat().font().pixelSize(),18);
        QVERIFY(cursor.charFormat().font().bold());
        QCOMPARE(text.toPlainText(),QString("Pasted title"));
        QTest::keyClick(window,Qt::Key_Return); QTRY_VERIFY(!canvas->editing());
        QCOMPARE(document->selectedStyle()["fontSize"].toInt(),18);
        QVERIFY(document->selectedStyle()["bold"].toBool());
    }
    void nodePanelAppliesStylesAndProtectsDraft() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); tabs->setProperty("currentIndex",1);
        auto *panel=window->findChild<QQuickItem *>("nodeStylePanel"); QVERIFY(panel);
        document->select(2);
        auto *shape=window->findChild<QQuickItem *>("style-shape"); QVERIFY(shape);
        shape->forceActiveFocus(); QTest::keyClick(window,Qt::Key_End);
        QTRY_COMPARE(document->appearance(2).shape,NodeShape::Octagon);
        auto *fixed=window->findChild<QQuickItem *>("style-fixedWidth"); QVERIFY(fixed);
        fixed->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QTRY_VERIFY(document->nodes().value(2).style.value("width").toDouble()>0);
        auto apply=[panel](QString key,QVariant value) {
            QVariant accepted;
            const bool called=QMetaObject::invokeMethod(panel,"apply",Q_RETURN_ARG(QVariant,accepted),
                Q_ARG(QVariant,key),Q_ARG(QVariant,value));
            return called && accepted.toBool();
        };
        QVERIFY(!apply("fontSize",28)); QVERIFY(apply("bold",true)); QVERIFY(apply("width",240));
        QVERIFY(apply("borderWidth",3)); QVERIFY(apply("borderStyle",2));
        QVERIFY(apply("branchStroke",3)); QVERIFY(apply("branchWidth",4));
        QVERIFY(apply("fill",QString("#d7e9b4"))); QVERIFY(apply("textColor",QString("#24311a")));
        QVERIFY(apply("alignment",1));
        QCOMPARE(document->selectedStyle()["fontSize"].toDouble(),18.);
        QVERIFY(document->selectedStyle()["bold"].toBool());
        auto *family=window->findChild<QQuickItem *>("style-fontFamily"); QVERIFY(family);
        QCOMPARE(family->property("editText").toString(),document->selectedStyle()["fontFamily"].toString());
        canvas->fit(); canvas->beginEdit(2);
        QTest::keyClick(window,Qt::Key_Right);
        type(" with a longer title");
        const auto before=canvas->editingRect();
        QTest::qWait(150);
        const QString dir=qEnvironmentVariable("MINDMAP_NODE_SCREENSHOTS");
        if(!dir.isEmpty()) { QDir().mkpath(dir); QVERIFY(window->grabWindow().save(dir+"/node-panel-editing.png")); }
        QTest::keyClick(window,Qt::Key_Return); QTRY_VERIFY(!canvas->editing());
        QCOMPARE(document->nodes().value(2).rect.size()*canvas->zoom(),before.size());
        QCOMPARE(document->selectedStyle()["fontSize"].toDouble(),18.);
        QVERIFY(document->selectedStyle()["bold"].toBool());
        QTest::qWait(200);
        if(!dir.isEmpty()) {
            QVERIFY(window->grabWindow().save(dir+"/node-panel-committed.png"));
            auto *scroll=window->findChild<QQuickItem *>("inspectorScroll"); QVERIFY(scroll);
            auto *flick=qvariant_cast<QObject *>(scroll->property("contentItem")); QVERIFY(flick);
            flick->setProperty("contentY",650); QTest::qWait(150);
            QVERIFY(window->grabWindow().save(dir+"/node-panel-font.png"));
            flick->setProperty("contentY",0);
        }
        canvas->beginEdit(2); editor->setProperty("text",QString(17000,'x'));
        QVERIFY(!apply("shape",1)); QCOMPARE(document->appearance(2).shape,NodeShape::Octagon);
        QVERIFY(canvas->editing()); canvas->endEdit();
        tabs->setProperty("currentIndex",0);
    }
    void researchThemesApplyOptionalLayouts() {
        window->setProperty("inspectorVisible",true);
        auto *tabs=window->findChild<QQuickItem *>("inspectorTabs"); tabs->setProperty("currentIndex",2);
        auto *toggle=window->findChild<QQuickItem *>("useThemeLayouts"); QVERIFY(toggle);
        toggle->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
        QVERIFY(window->property("useThemeLayouts").toBool());
        for(const auto &id:QStringList{"canopy","atlas","studio","nocturne"}) {
            auto *card=findVisual(window->contentItem(),"theme-"+id); QVERIFY(card);
            card->forceActiveFocus(); QTest::keyClick(window,Qt::Key_Space);
            QTRY_COMPARE(document->themeId(),id);
            QCOMPARE(document->layout(),Themes::layoutRecipe(id)["layout"].toString());
            QTest::qWait(250);
            const QString dir=qEnvironmentVariable("MINDMAP_RESEARCH_SCREENSHOTS");
            if(!dir.isEmpty()) {
                auto *scroll=window->findChild<QQuickItem *>("inspectorScroll"); QVERIFY(scroll);
                auto *flick=qvariant_cast<QObject *>(scroll->property("contentItem")); QVERIFY(flick);
                flick->setProperty("contentY",flick->property("contentY").toDouble()+card->mapToScene({0,0}).y()-scroll->mapToScene({0,0}).y());
                QTest::qWait(100); QDir().mkpath(dir);
                QVERIFY(window->grabWindow().save(dir+"/"+id+".png"));
            }
        }
        window->setProperty("useThemeLayouts",false);
        document->setLayout("Vertical");
        auto *card=findVisual(window->contentItem(),"theme-canopy"); card->forceActiveFocus();
        QTest::keyClick(window,Qt::Key_Space); QCOMPARE(document->themeId(),QString("canopy"));
        QCOMPARE(document->layout(),QString("Vertical"));
        canvas->beginEdit(document->selectedId()); editor->setProperty("text",QString(17000,'x'));
        window->setProperty("useThemeLayouts",true);
        QVariant accepted; QVERIFY(QMetaObject::invokeMethod(window,"applyTheme",Q_RETURN_ARG(QVariant,accepted),Q_ARG(QVariant,QString("atlas"))));
        QVERIFY(!accepted.toBool()); QCOMPARE(document->themeId(),QString("canopy"));
        canvas->endEdit(); window->setProperty("useThemeLayouts",false); tabs->setProperty("currentIndex",0);
    }
    void themeCardsApplyAllPresets() {
        window->setProperty("inspectorVisible", true);
        auto *tabs = window->findChild<QQuickItem *>("inspectorTabs");
        QVERIFY(tabs);
        tabs->setProperty("currentIndex", 2);
        for (const QString &id : {QString("beach-day"), QString("holographic"),
                                  QString("retro"), QString("arcade"),
                                  QString("paper"), QString("forest"), QString("midnight"),
                                  QString("porcelain"),QString("omarchy"),QString("sky"),QString("starlight"),QString("sage"),QString("blush"),QString("graphite")}) {
            auto *card = findVisual(window->contentItem(), "theme-" + id);
            QVERIFY(card);
            // Activating a keyboard-focused Button uses its actual clicked signal path.
            card->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Space);
            QTRY_COMPARE(document->themeId(), id);
            QVERIFY(card->property("checked").toBool());
            QTest::qWait(150);
            QTemporaryDir exportDir;
            QVERIFY(exportDir.isValid());
            const QString png = exportDir.filePath("theme.png");
            QSignalSpy exported(canvas, &MindCanvas::exportFinished);
            QVERIFY(canvas->exportPng(png));
            QTRY_COMPARE(exported.count(), 1);
            QVERIFY(exported.first().at(1).toBool());
            QImage captured(png);
            QVERIFY(!captured.isNull());
            QCOMPARE(captured.pixelColor(5,5), document->canvasColor());
            const QString dir = qEnvironmentVariable("MINDMAP_THEME_SCREENSHOTS");
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                QVERIFY(window->grabWindow().save(dir + "/theme-" + id + ".png"));
            }
        }
        tabs->setProperty("currentIndex", 0);
        canvas->forceActiveFocus();
    }
    void themesPreservePendingNotes() {
        auto *notes = window->findChild<QQuickItem *>("notesEditor");
        QVERIFY(notes);
        notes->setProperty("text", "Pending notes");
        document->setThemeId("retro");
        QCOMPARE(notes->property("text").toString(), QString("Pending notes"));
        QCOMPARE(document->selectedNotes(), QString());
        document->select(2);
        QCOMPARE(notes->property("text").toString(), document->selectedNotes());
    }
    void themesApplyThroughUiAndProtectDraft() {
        const int count = document->nodeCount();
        const auto selected = document->selectedIds();
        QVariant accepted;
        QVERIFY(QMetaObject::invokeMethod(window, "applyTheme", Q_RETURN_ARG(QVariant, accepted),
                                         Q_ARG(QVariant, QString("beach-day"))));
        QVERIFY(accepted.toBool());
        QCOMPARE(document->property("themeId").toString(), QString("beach-day"));
        QCOMPARE(document->nodeCount(), count);
        QCOMPARE(document->selectedIds(), selected);
        document->undo();
        QVERIFY(document->property("themeId").toString() != QString("beach-day"));
        document->redo();
        QCOMPARE(document->property("themeId").toString(), QString("beach-day"));
        canvas->beginEdit(document->selectedId());
        editor->setProperty("text", QString(17000, 'x'));
        QVERIFY(QMetaObject::invokeMethod(window, "applyTheme", Q_RETURN_ARG(QVariant, accepted),
                                         Q_ARG(QVariant, QString("retro"))));
        QVERIFY(!accepted.toBool());
        QCOMPARE(document->property("themeId").toString(), QString("beach-day"));
        QVERIFY(canvas->editing());
        QTextDocument draft;
        draft.setHtml(editor->property("text").toString());
        QCOMPARE(draft.toPlainText(), QString(17000, 'x'));
    }
    void inlineEditingKeepsViewportAndDocumentStable() {
        canvas->zoomAt({canvas->width()/2, canvas->height()/2}, .6 / canvas->zoom());
        const double zoomBefore = canvas->zoom();
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(canvas->editing());
        QCOMPARE(canvas->zoom(), zoomBefore);
        const int id = canvas->editingId();
        const QString stored = document->nodes().value(id).text;
        const auto beforeNodes = document->nodes();
        const QPointF anchor = canvas->editingRect().topLeft();
        QSignalSpy outlineChanges(document, &Engine::outlineChanged);
        editor->setProperty("text", "A longer title typed directly into its node");
        QTest::qWait(50);
        QCOMPARE(document->nodes().value(id).text, stored);
        QCOMPARE(outlineChanges.count(), 0);
        QCOMPARE(canvas->editingRect().topLeft(), anchor);
        for (auto it=beforeNodes.cbegin(); it!=beforeNodes.cend(); ++it)
            QCOMPARE(document->nodes().value(it.key()).rect, it->rect);
        const QSizeF preview = canvas->editingRect().size();
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_VERIFY(!canvas->editing());
        const QRectF finalRect = document->nodes().value(id).rect;
        QTest::qWait(230); // Commit allows the automatic layout to settle.
        QCOMPARE(canvas->zoom(),zoomBefore);
        QVERIFY(std::abs(finalRect.width()*canvas->zoom()-preview.width()) < .5);
        QVERIFY(std::abs(finalRect.height()*canvas->zoom()-preview.height()) < .5);
        QCOMPARE(plain(id), QString("A longer title typed directly into its node"));
    }
    void inlineEditingAcrossThemes() {
        for (const QString &theme : {QString("beach-day"), QString("holographic"),
                                     QString("retro"), QString("arcade"),
                                  QString("paper"), QString("forest"), QString("midnight")}) {
            document->setThemeId(theme);
            document->select(1);
            canvas->zoomAt({canvas->width()/2, canvas->height()/2}, 1 / canvas->zoom());
            canvas->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Tab);
            QTRY_VERIFY(editor->hasActiveFocus());
            type("An idea taking shape");
            QTest::qWait(100);
            const int id = canvas->editingId();
            const QRectF preview = canvas->editingRect();
            const QString dir = qEnvironmentVariable("MINDMAP_INLINE_SCREENSHOTS");
            if (!dir.isEmpty()) {
                QDir().mkpath(dir);
                QVERIFY(window->grabWindow().save(dir + "/" + theme + "-editing.png"));
            }
            QTest::keyClick(window, Qt::Key_Return);
            QTRY_VERIFY(!canvas->editing());
            QTest::qWait(100);
            const QRectF result = document->nodes().value(id).rect;
            QVERIFY(QLineF(canvas->mapFromWorld(result.topLeft()), preview.topLeft()).length() < .5);
            QCOMPARE(result.size() * canvas->zoom(), preview.size());
            if (!dir.isEmpty())
                QVERIFY(window->grabWindow().save(dir + "/" + theme + "-committed.png"));
        }
    }
    void tabCreatesChildAndFocusesEditor() {
        int parent = document->selectedId();
        int count = document->nodeCount();
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(document->nodes().value(document->selectedId()).parent, parent);
        QVERIFY(canvas->editing());
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Tab child");
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QVERIFY(!canvas->editing());
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(plain(document->selectedId()), QString("Tab child"));
        QVERIFY(canvas->hasActiveFocus());
        stage("Title committed — no extra sibling created");
    }
    void returnCommitsThenNextReturnCreatesSibling() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        int parent = document->nodes().value(id).parent;
        int count = document->nodeCount();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Committed title");
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QCOMPARE(document->nodeCount(), count);
        QCOMPARE(plain(id), QString("Committed title"));
        QVERIFY(!canvas->editing());
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(document->nodes().value(document->selectedId()).parent, parent);
        QTRY_VERIFY(editor->hasActiveFocus());
        stage("Second Return created a sibling");
    }
    void tabDuringEditingCommitsAndCreatesChild() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        int count = document->nodeCount();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Parent title");
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QCOMPARE(plain(id), QString("Parent title"));
        QCOMPARE(document->nodeCount(), count + 1);
        QCOMPARE(document->nodes().value(document->selectedId()).parent, id);
        QTRY_VERIFY(editor->hasActiveFocus());
        QCOMPARE(canvas->editingId(), document->selectedId());
        stage("Tab committed parent and opened a child");
    }
    void richTextShortcutPersistsFormatting() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        int id = document->selectedId();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Bold title");
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        stage("Bold formatting applied to selected title");
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QCOMPARE(plain(id), QString("Bold title"));
        QVERIFY(document->nodes().value(id).text.contains("font-weight:700"));
    }
    void rejectedTitleStaysInEditor() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        QString original = document->nodes().value(id).text;
        QTRY_VERIFY(editor->hasActiveFocus());
        editor->setProperty("text", QString(17000, QLatin1Char('x')));
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        QVERIFY(editor->hasActiveFocus());
        QCOMPARE(document->nodes().value(id).text, original);
        QVERIFY(!document->error().isEmpty());
    }
    void canvasClickCommitsPendingTitle() {
        QTest::keyClick(window, Qt::Key_Tab);
        QTest::qWait(25);
        QVERIFY(canvas->editing());
        int id = document->selectedId();
        QTRY_VERIFY(editor->hasActiveFocus());
        type("Click committed");
        QPointF point = canvas->mapToScene(QPointF(16, canvas->height() - 16));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        QTRY_VERIFY(!canvas->editing());
        QCOMPARE(plain(id), QString("Click committed"));
        stage("Canvas click saved the draft");
    }
    void layoutsAndFoldingPreserveHierarchy() {
        const int count = document->nodeCount();
        const int root = 1;
        QVERIFY(!document->nodes().value(root).children.isEmpty());
        const int branch = document->nodes().value(root).children.first();
        const auto children = document->nodes().value(branch).children;
        QVERIFY(!children.isEmpty());
        for (const QString &layout :
             {QString("Horizontal"), QString("Vertical"), QString("Compact")}) {
            document->setLayout(layout);
            QTest::qWait(300);
            canvas->fit();
            QCOMPARE(document->nodeCount(), count);
            QCOMPARE(document->nodes().value(branch).children, children);
            stage(layout + " layout — hierarchy preserved");
        }
        document->select(branch);
        const int visible = document->visibleCount();
        QTest::keyClick(window, Qt::Key_F, Qt::AltModifier);
        QTest::qWait(300);
        QVERIFY(document->selectedFolded());
        QVERIFY(document->visibleCount() < visible);
        QCOMPARE(document->nodeCount(), count);
        canvas->fit();
        stage("Folded branch — descendants retained");
        QTest::keyClick(window, Qt::Key_F, Qt::AltModifier);
        QTest::qWait(300);
        QVERIFY(!document->selectedFolded());
        QCOMPARE(document->visibleCount(), visible);
        canvas->fit();
        stage("Expanded branch — descendants restored");
    }
    void manualDragMirrorsLiveAndMatchesDrop() {
        document->setManual(true);
        const int branch=document->nodes().value(1).children.first();
        const int child=document->nodes().value(branch).children.first();
        QTest::qWait(250);
        canvas->zoomAt({canvas->width()/2,canvas->height()/2},.6/canvas->zoom());
        canvas->panBy(canvas->width()*.5-canvas->mapFromWorld(document->nodes().value(1).rect.center()).x(),0);
        const auto original=document->nodes();
        const QPoint from=screenCenter(branch);
        const QPoint to(screenCenter(1).x()-110,from.y()+20);
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(window,to,60);
        QTRY_VERIFY(canvas->dragging());
        QVERIFY(canvas->nodeRect(child).center().x()<canvas->nodeRect(branch).center().x());
        QCOMPARE(document->nodes().value(child).rect,original.value(child).rect);
        QTest::mouseMove(window,from,60);
        QVERIFY(canvas->nodeRect(child).center().x()>canvas->nodeRect(branch).center().x());
        QTest::mouseMove(window,to,60);
        const auto previewBranch=canvas->nodeRect(branch), previewChild=canvas->nodeRect(child);
        const QString dir=qEnvironmentVariable("MINDMAP_MIRROR_SCREENSHOTS");
        if(!dir.isEmpty()) { QDir().mkpath(dir); QTest::qWait(100); QVERIFY(window->grabWindow().save(dir+"/dragging-left.png")); }
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(document->nodes().value(branch).rect,previewBranch);
        QCOMPARE(document->nodes().value(child).rect,previewChild);
        QCOMPARE(canvas->nodeRect(child),previewChild);
        QTest::qWait(250); QCOMPARE(canvas->nodeRect(child),previewChild);
        if(!dir.isEmpty()) QVERIFY(window->grabWindow().save(dir+"/dropped-left.png"));
        document->undo(); QCOMPARE(document->nodes().value(child).rect,original.value(child).rect);
    }
    void manualDragMovesSubtree() {
        const int root = 1;
        QVERIFY(!document->nodes().value(root).children.isEmpty());
        const int branch = document->nodes().value(root).children.first();
        const int child = document->nodes().value(branch).children.first();
        document->setManual(true);
        QTest::qWait(300);
        canvas->fit();
        const QPointF before = document->nodes().value(branch).rect.center();
        const QPointF childBefore = document->nodes().value(child).rect.center();
        const QPoint from = screenCenter(branch);
        stage("Manual placement — dragging a whole subtree");
        drag(from, from + QPoint(40, 35));
        const QPointF delta = document->nodes().value(branch).rect.center() - before;
        QVERIFY(QLineF(QPointF(), delta).length() > 5);
        QVERIFY(QLineF(document->nodes().value(child).rect.center() - childBefore, delta).length() <
                0.01);
        stage("Subtree moved together");
        document->undo();
        QCOMPARE(document->nodes().value(branch).rect.center(), before);
        stage("Undo restored manual placement");
    }
    void automaticDragReordersSiblingsInGaps() {
        document->setManual(false);
        for (const QString layout : {QString("Horizontal"),QString("Vertical"),QString("Compact")}) {
            document->setLayout(layout); QTest::qWait(250); canvas->fit();
            const auto original=document->nodes().value(1).children;
            QVERIFY(original.size()>=3);
            const int moving=original[1], first=original[0];
            const auto rect=document->nodes().value(first).rect;
            const QPointF before=layout=="Vertical" ? QPointF(rect.left()-10,rect.center().y())
                                                       : QPointF(rect.center().x(),rect.top()-10);
            drag(screenCenter(moving),canvas->mapToScene(canvas->mapFromWorld(before)).toPoint());
            auto expected=original; expected.removeAll(moving); expected.prepend(moving);
            QCOMPARE(document->nodes().value(1).children,expected);
            QCOMPARE(document->nodes().value(moving).parent,1);
            document->undo(); QCOMPARE(document->nodes().value(1).children,original);
            QTest::qWait(250);
            const auto last=document->nodes().value(original.last()).rect;
            const QPointF after=layout=="Vertical" ? QPointF(last.right()+10,last.center().y())
                                                      : QPointF(last.center().x(),last.bottom()+10);
            drag(screenCenter(moving),canvas->mapToScene(canvas->mapFromWorld(after)).toPoint());
            expected=original; expected.removeAll(moving); expected.append(moving);
            QCOMPARE(document->nodes().value(1).children,expected);
            document->undo(); QCOMPARE(document->nodes().value(1).children,original);
            QTest::qWait(250);
            const auto third=document->nodes().value(original[2]).rect;
            const QPointF middle=layout=="Vertical" ? QPointF(third.left()-10,third.center().y())
                                                       : QPointF(third.center().x(),third.top()-10);
            drag(screenCenter(first),canvas->mapToScene(canvas->mapFromWorld(middle)).toPoint());
            expected=original; expected.removeAll(first); expected.insert(1,first);
            QCOMPARE(document->nodes().value(1).children,expected);
            document->undo(); QCOMPARE(document->nodes().value(1).children,original);
            document->redo(); QCOMPARE(document->nodes().value(1).children,expected);
            document->undo();
        }
        document->setLayout("Horizontal");
    }
    void multipleBranchesDragToNewParentInBothModes() {
        for(bool manual:{false,true}) for(const QString layout:{QString("Horizontal"),QString("Vertical")}) {
            document->setLayout(layout); document->setManual(manual); QTest::qWait(250); canvas->fit();
            const auto siblings=document->nodes()[1].children;
            QVERIFY(siblings.size()>=3);
            const int first=siblings[0],second=siblings[1],target=siblings[2];
            const int nested=document->nodes()[first].children.first();
            document->selectMany({first,second,nested});
            const auto before=document->nodes()[1].children;
            drag(screenCenter(first),screenCenter(target));
            QCOMPARE(document->nodes()[first].parent,target);
            QCOMPARE(document->nodes()[second].parent,target);
            QCOMPARE(document->nodes()[nested].parent,first);
            QCOMPARE(document->selectedIds(),QSet<int>({first,second,nested}));
            document->undo(); QCOMPARE(document->nodes()[1].children,before);
            QTest::qWait(250); canvas->fit();
            // Single-branch manual drops use the same parent target handling.
            document->select(first);
            drag(screenCenter(first),screenCenter(target));
            QCOMPARE(document->nodes()[first].parent,target);
            document->undo(); QCOMPARE(document->nodes()[1].children,before);
        }
    }
    void automaticDragReparentsBranch() {
        const int root = 1;
        const auto siblings = document->nodes().value(root).children;
        QVERIFY(siblings.size() >= 2);
        const int branch = siblings.first(), target = siblings.at(1);
        document->setManual(false);
        document->setThemeId("lab");
        QTest::qWait(300);
        canvas->fit();
        stage("Automatic layout — drop branch onto another parent");
        drag(screenCenter(branch), screenCenter(target));
        QCOMPARE(document->nodes().value(branch).parent, target);
        stage("Branch attached to highlighted parent");
        document->undo();
        QCOMPARE(document->nodes().value(branch).parent, root);
        stage("Undo restored original hierarchy");
    }
    void rejectedDraftCannotBeReplacedByAnotherEditor() {
        const int originalId = document->selectedId();
        const int otherId = document->nodes().value(originalId).children.first();
        canvas->beginEdit(originalId);
        QTRY_VERIFY(editor->hasActiveFocus());
        const QString original = document->nodes().value(originalId).text;
        editor->setProperty("text", QString(17000, QLatin1Char('x')));
        canvas->beginEdit(otherId);
        QCOMPARE(canvas->editingId(), originalId);
        QVERIFY(editor->property("text").toString().size() >= 17000);
        QCOMPARE(document->nodes().value(originalId).text, original);
        QVERIFY(!document->error().isEmpty());
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    UiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ui_test.moc"
