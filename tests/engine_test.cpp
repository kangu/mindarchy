#include "../src/engine.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextCursor>
#include <QtTest>
#include <cmath>
class EngineTest : public QObject {
    Q_OBJECT
  private slots:
    void nodeStylesPersistAndUndoAsOneCommand() {
        Engine e; e.selectMany({2,3});
        const auto original=e.nodes().value(2).text;
        QVERIFY(e.applyNodeStyle({{"shape",7},{"fill",QString("#123456")},{"borderWidth",3.5},
            {"borderStyle",2},{"branchStroke",3},{"branchWidth",4.5},{"width",240},
            {"fontSize",28},{"bold",true},{"alignment",1}}));
        for(int id:{2,3}) {
            QCOMPARE(e.appearance(id).shape,NodeShape::Octagon);
            QCOMPARE(e.appearance(id).fill,QColor("#123456"));
            QCOMPARE(e.nodes().value(id).rect.width(),240.);
        }
        QCOMPARE(e.selectedStyle()["fontSize"].toDouble(),28.);
        QVERIFY(e.selectedStyle()["bold"].toBool());
        QCOMPARE(e.selectedStyle()["alignment"].toInt(),1);
        e.undo(); QCOMPARE(e.nodes().value(2).text,original); QVERIFY(e.nodes().value(3).style.isEmpty());
        e.redo(); QCOMPARE(e.appearance(3).branchStroke,Qt::DotLine);
        QTemporaryDir dir; const auto path=dir.filePath("styles.json"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path));
        QCOMPARE(loaded.nodes().value(2).style,e.nodes().value(2).style);
        QCOMPARE(loaded.nodes().value(2).rect.size(),e.nodes().value(2).rect.size());
        e.setThemeId("retro"); QCOMPARE(e.appearance(2).fill,QColor("#123456"));
        e.resetNodeStyle(); QVERIFY(e.nodes().value(2).style.isEmpty());
        QCOMPARE(e.appearance(2).shape,Themes::appearance("retro",1,0).shape);
        e.undo(); QCOMPARE(e.appearance(2).shape,NodeShape::Octagon);
    }
    void nodeStylesValidateAtomicallyAndReportMixedValues() {
        Engine e; e.select(2); QVERIFY(e.applyNodeStyle({{"fill",QString("#123456")},{"width",180}}));
        e.selectMany({2,3}); QVERIFY(e.selectedStyle()["mixed"].toStringList().contains("fill"));
        const auto before=e.nodes().value(2).style;
        QVERIFY(!e.applyNodeStyle({{"fill",QString("#ffffff")},{"width",-1}}));
        QCOMPARE(e.nodes().value(2).style,before); QVERIFY(e.nodes().value(3).style.isEmpty());
        QVERIFY(!e.applyNodeStyle({{"shape",99}}));
        QVERIFY(!e.applyNodeStyle({{"branchStroke",0}}));
        QVERIFY(!e.applyNodeStyle({{"fontSize",10000}}));
        QTemporaryDir dir; const auto path=dir.filePath("bad.json"); QVERIFY(e.save(path));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); auto json=QJsonDocument::fromJson(file.readAll()).object(); file.close();
        auto nodes=json["nodes"].toArray(); auto node=nodes[0].toObject();
        node["style"]=QJsonObject{{"shape",999}}; nodes[0]=node; json["nodes"]=nodes;
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(json).toJson()); file.close();
        QVERIFY(!e.open(path)); QCOMPARE(e.nodes().value(2).style,before);
    }
    void textColorOverridesRichRunsAndThicknessCanReset() {
        Engine e; e.select(2);
        QVERIFY(e.setText(2,"<span style='color:red;font-size:12pt'>Colored title</span>"));
        QVERIFY(e.applyNodeStyle({{"textColor",QString("#123456")},{"fontSize",30},{"underline",true},{"strike",true},{"branchWidth",7}}));
        QTextDocument doc; doc.setHtml(e.selectedText()); QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
        QCOMPARE(cursor.charFormat().foreground().color(),QColor("#123456"));
        QCOMPARE(e.selectedStyle()["fontSize"].toDouble(),30.);
        QVERIFY(e.selectedStyle()["underline"].toBool()); QVERIFY(e.selectedStyle()["strike"].toBool());
        QVERIFY(!e.selectedStyle()["themeBranchWidth"].toBool());
        e.resetBranchWidth(); QVERIFY(e.selectedStyle()["themeBranchWidth"].toBool());
        QCOMPARE(e.appearance(2).text,QColor("#123456"));
        e.undo(); QCOMPARE(e.appearance(2).branchWidth,7.);
    }
    void fixedWidthWrapsDraftWithFinalMetrics() {
        Engine e; e.select(2); QVERIFY(e.applyNodeStyle({{"width",160},{"fontSize",26},{"italic",true}}));
        const QString title="A longer title which wraps across several lines";
        const auto preview=e.previewTextSize(2,title);
        QVERIFY(e.setText(2,title)); QCOMPARE(e.nodes().value(2).rect.size(),preview);
        QCOMPARE(preview.width(),160.); QVERIFY(preview.height()>42);
        QVERIFY(e.applyNodeStyle({{"width",0}})); QVERIFY(e.nodes().value(2).rect.width()>160);
    }
    void manualBranchesMirrorAndPersist() {
        Engine e; e.loadFixture(45); e.setManual(true);
        const int branch=e.nodes().value(1).children.first();
        const int child=e.nodes().value(branch).children.first();
        e.moveManual(child,20,12);
        const auto before=e.nodes();
        const QPointF delta(e.nodes().value(1).rect.center().x()-e.nodes().value(branch).rect.center().x()-150,25);
        const auto preview=e.manualGeometry(branch,delta);
        QCOMPARE(e.nodes().value(branch).rect,before.value(branch).rect);
        QVERIFY(preview.value(child).center().x()<preview.value(branch).center().x());
        QCOMPARE(preview.value(child).center().x()-preview.value(branch).center().x(),
                 -(before.value(child).rect.center().x()-before.value(branch).rect.center().x()));
        e.moveManual(branch,delta.x(),delta.y());
        for(int id:e.visibleIds()) QCOMPARE(e.nodes().value(id).rect,preview.value(id));
        const auto mirrored=e.nodes();
        e.undo(); for(int id:e.visibleIds()) QCOMPARE(e.nodes().value(id).rect,before.value(id).rect);
        e.redo(); QCOMPARE(e.nodes().value(child).rect,mirrored.value(child).rect);
        const QPointF childBefore=e.nodes().value(child).rect.center();
        e.moveManual(child,30,15); QCOMPARE(e.nodes().value(child).rect.center(),childBefore+QPointF(30,15));
        e.undo();
        QTemporaryDir dir; const auto path=dir.filePath("mirrored.json"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path));
        for(int id:e.visibleIds()) QCOMPARE(loaded.nodes().value(id).rect,e.nodes().value(id).rect);
        e.select(branch); e.toggleFold(); e.toggleFold();
        QCOMPARE(e.nodes().value(child).rect,mirrored.value(child).rect);
        e.moveManual(branch,-delta.x(),-delta.y());
        for(int id:e.visibleIds()) QCOMPARE(e.nodes().value(id).rect,before.value(id).rect);
    }
    void nodeTypeConversionPreservesContent() {
        Engine e; e.select(2); e.toggleTask(); const auto original=e.nodes().value(2);
        QVERIFY(e.setNodeKind(2,"date")); QVERIFY(e.nodeCount()>2);
        QCOMPARE(e.nodes().value(2).children,original.children);
        QCOMPARE(e.nodes().value(2).text,original.text); QVERIFY(!e.nodes().value(2).task);
        e.undo(); QVERIFY(e.nodes().value(2).task); QCOMPARE(e.nodes().value(2).kind,QString("text"));
        e.redo(); QVERIFY(e.setDateEntry(2,"2026-09-08","Retained entry"));
        QVERIFY(e.setNodeKind(2,"text")); QCOMPARE(e.nodes().value(2).text,original.text);
        QTemporaryDir dir; QVERIFY(e.save(dir.filePath("types.json")));
        Engine loaded; QVERIFY(loaded.open(dir.filePath("types.json")));
        QVERIFY(loaded.setNodeKind(2,"date")); QCOMPARE(loaded.dateEntry(2,"2026-09-08"),QString("Retained entry"));
    }
    void calendarNumericTotals() {
        CalendarData c; c.view="month"; c.anchor=QDate(2026,9,8);
        const auto plainSize=Calendar::size(c); QVERIFY(!Calendar::totals(c).enabled);
        c.entries={{"2026-09-01","10"},{"2026-09-06","-2.5"},{"2026-09-07","3,25"},
                   {"2026-09-08","meeting"},{"2026-09-30","0"},{"2026-10-01","999"}};
        const auto sums=Calendar::totals(c); QVERIFY(sums.enabled);
        QCOMPARE(sums.weeks.size(),5); QCOMPARE(sums.weeks[0],7.5); QCOMPARE(sums.weeks[1],3.25);
        QCOMPARE(sums.weeks[4],0.); QCOMPARE(sums.month,10.75);
        QVERIFY(Calendar::size(c).width()>plainSize.width()); QCOMPARE(Calendar::size(c).height(),plainSize.height()+36);
        const auto key=Calendar::key(c); c.entries["2026-09-01"]="12"; QVERIFY(Calendar::key(c)!=key);
        c.view="week"; c.anchor=QDate(2026,9,30);
        QCOMPARE(Calendar::totals(c).weeks[0],999.); QCOMPARE(Calendar::size(c).height(),114.);
        double value; QVERIFY(Calendar::numericValue("  +.5  ",value)); QCOMPARE(value,.5);
        QVERIFY(!Calendar::numericValue("12 hours",value)); QVERIFY(!Calendar::numericValue("1,000.50",value));
        QVERIFY(!Calendar::numericValue("NaN",value)); QVERIFY(!Calendar::numericValue("",value));
        Engine e; e.select(2); QVERIFY(e.setNodeKind(2,"date"));
        QVERIFY(e.configureDateNode(2,"month","2026-09-08"));
        QVERIFY(e.setDateEntry(2,"2026-09-08","4")); QCOMPARE(Calendar::totals(e.nodes().value(2).calendar).month,4.);
        e.undo(); QVERIFY(!Calendar::totals(e.nodes().value(2).calendar).enabled);
        e.redo(); QCOMPARE(Calendar::totals(e.nodes().value(2).calendar).month,4.);
    }
    void calendarPeriodsEntriesAndHistory() {
        Engine e; const int count=e.nodeCount(); e.addDateNode("week"); const int id=e.selectedId();
        QCOMPARE(e.nodeCount(),count+1); QCOMPARE(e.nodes().value(id).kind,QString("date"));
        QCOMPARE(e.nodes().value(id).calendar.anchor,QDate::currentDate());
        QVERIFY(e.configureDateNode(id,"week","2025-12-31"));
        auto days=Calendar::days(e.nodes().value(id).calendar);
        QCOMPARE(days.size(),7); QCOMPARE(days.first(),QDate(2025,12,29)); QCOMPARE(days.last(),QDate(2026,1,4));
        QVERIFY(e.setDateEntry(id,"2026-01-01","Start a new project"));
        const auto size=e.nodes().value(id).rect.size();
        QVERIFY(e.setDateEntry(id,"2026-01-01","Revised plan")); QCOMPARE(e.nodes().value(id).rect.size(),size);
        e.undo(); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Start a new project"));
        e.redo(); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QVERIFY(e.configureDateNode(id,"month","2024-02-15"));
        days=Calendar::days(e.nodes().value(id).calendar); QCOMPARE(days.size(),35);
        QVERIFY(days.contains(QDate(2024,2,29))); QVERIFY(!days.first().isValid());
        e.undo(); QCOMPARE(e.nodes().value(id).calendar.view,QString("week"));
        QVERIFY(e.configureDateNode(id,"month","2026-01-01"));
        QVERIFY(e.shiftDateNode(id,1)); QCOMPARE(e.nodes().value(id).calendar.anchor,QDate(2026,2,1));
        QVERIFY(e.shiftDateNode(id,-1)); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QTemporaryDir dir; const auto path=dir.filePath("calendar.json"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path));
        QCOMPARE(loaded.nodes().value(id).calendar.entries,e.nodes().value(id).calendar.entries);
        QCOMPARE(loaded.nodes().value(id).rect.size(),e.nodes().value(id).rect.size());
        QVERIFY(!e.setDateEntry(id,"2026-02-30","Invalid"));
        QVERIFY(!e.setDateEntry(id,"2026-01-01",QString(4097,'x')));
        QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QVERIFY(e.setDateEntry(id,"2026-01-01","")); QVERIFY(e.dateEntry(id,"2026-01-01").isEmpty());
        e.undo(); QCOMPARE(e.dateEntry(id,"2026-01-01"),QString("Revised plan"));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); auto json=QJsonDocument::fromJson(file.readAll()).object(); file.close();
        auto nodes=json["nodes"].toArray(); auto n=nodes.last().toObject(); auto c=n["calendar"].toObject();
        c["anchor"]="bad date"; n["calendar"]=c; nodes[nodes.size()-1]=n; json["nodes"]=nodes;
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(json).toJson()); file.close();
        QVERIFY(!loaded.open(path)); QCOMPARE(loaded.dateEntry(id,"2026-01-01"),QString("Revised plan"));
    }
    void seed() {
        Engine e;
        QVERIFY(e.nodeCount() >= 12);
        QCOMPARE(e.nodes().value(1).parent, -1);
        QVERIFY(e.selectedText().contains("Mindmap"));
        QCOMPARE(e.nodeCount(), e.visibleCount());
    }
    void researchThemeRecipesAreAtomicAndReadable() {
        auto luminance=[](QColor c) {
            auto linear=[](double v) { return v<=.04045 ? v/12.92 : std::pow((v+.055)/1.055,2.4); };
            return .2126*linear(c.redF())+.7152*linear(c.greenF())+.0722*linear(c.blueF());
        };
        for(const auto &id:QStringList{"canopy","atlas","studio","nocturne"}) {
            Engine e; e.select(2); e.applyNodeStyle({{"fill",QString("#abcdef")}});
            e.setManual(true); const auto original=e.nodes(); const auto oldTheme=e.themeId();
            QSignalSpy changes(&e,&Engine::changed);
            QVERIFY(e.applyThemeRecipe(id)); QCOMPARE(changes.count(),1);
            QCOMPARE(e.themeId(),id); QVERIFY(!e.manual());
            QCOMPARE(e.layout(),Themes::layoutRecipe(id)["layout"].toString());
            for(auto it=original.begin();it!=original.end();++it) {
                QCOMPARE(e.nodes().value(it.key()).text,it->text);
                QCOMPARE(e.nodes().value(it.key()).children,it->children);
                QCOMPARE(e.nodes().value(it.key()).style,it->style);
            }
            e.undo(); QCOMPARE(e.themeId(),oldTheme); QVERIFY(e.manual());
            e.redo(); QCOMPARE(e.themeId(),id);
            QTemporaryDir dir; const auto file=dir.filePath("recipe.json"); QVERIFY(e.save(file));
            Engine loaded; QVERIFY(loaded.open(file)); QCOMPARE(loaded.themeId(),id); QCOMPARE(loaded.layout(),e.layout());
            const QString example=QFINDTESTDATA(qPrintable("../examples/research-templates/"+id+".json"));
            QVERIFY(!example.isEmpty()); Engine starter; QVERIFY(starter.open(example));
            QCOMPARE(starter.themeId(),id); QCOMPARE(starter.nodeCount(),13);
            for(int depth=0;depth<5;++depth) for(int branch=0;branch<6;++branch) {
                const auto a=Themes::appearance(id,depth,branch);
                const double fg=luminance(a.text),bg=luminance(a.fill.alpha()?a.fill:Themes::get(id).canvas);
                QVERIFY2((std::max(fg,bg)+.05)/(std::min(fg,bg)+.05)>=4.5,qPrintable(id));
            }
        }
        Engine invalid; const auto old=invalid.themeId(); QVERIFY(!invalid.applyThemeRecipe("missing"));
        QCOMPARE(invalid.themeId(),old); QCOMPARE(Themes::get("missing").id,QString("lab"));
    }
    void themeCatalogAndAppearance() {
        Engine e;
        QCOMPARE(e.themeId(), QString("lab"));
        const QVariantList themes = e.themes();
        QVERIFY(themes.size() >= 5);
        const QStringList expectedIds{"beach-day", "holographic", "retro", "arcade", "lab", "canopy", "atlas", "studio", "nocturne"};
        for (int i = 0; i < expectedIds.size(); ++i) {
            const QVariantMap entry = themes[i].toMap();
            QCOMPARE(entry.value("id").toString(), expectedIds[i]);
            QVERIFY(entry.value("name").isValid());
            QVERIFY(entry.value("canvas").value<QColor>().isValid());
            QCOMPARE(entry.value("palette").toList().size(), 6);
        }
        QCOMPARE(e.canvasColor(), QColor("#111920"));
        e.setThemeId("beach-day");
        QCOMPARE(e.canvasColor(), QColor("#faf9f6"));
        const NodeAppearance root = e.appearance(1);
        QCOMPARE(root.fill, QColor("#f3ddb4"));
        QCOMPARE(root.shape, NodeShape::Rounded);
        const int branch = e.nodes().value(1).children.first();
        const NodeAppearance child = e.appearance(branch);
        QCOMPARE(child.fill, QColor("#ffffff"));
        QCOMPARE(child.border, QColor("#98a5cc"));
        const int grandchild = e.nodes().value(branch).children.first();
        const NodeAppearance deeper = e.appearance(grandchild);
        QCOMPARE(deeper.shape, NodeShape::Underline);
        QCOMPARE(deeper.fill.alpha(), 0);
        QCOMPARE(deeper.branch, QColor("#98a5cc"));
    }
    void themeUsesTopLevelBranchOrdinalAfterMove() {
        Engine e;
        e.setThemeId("arcade");
        const auto branches = e.nodes().value(1).children;
        const int descendant = e.nodes().value(branches[0]).children.first();
        QCOMPARE(e.appearance(descendant).branch, QColor("#b86cc7"));
        e.moveNode(descendant, branches[1]);
        QCOMPARE(e.appearance(descendant).branch, QColor("#e36b73"));
    }
    void themeHistoryAndPersistenceAreTransactional() {
        Engine e;
        e.setLayout("Vertical");
        e.setThemeId("retro");
        QCOMPARE(e.themeId(), QString("retro"));
        e.undo();
        QCOMPARE(e.themeId(), QString("lab"));
        QCOMPARE(e.layout(), QString("Vertical"));
        e.redo();
        QCOMPARE(e.themeId(), QString("retro"));
        e.setThemeId("unknown");
        QCOMPARE(e.themeId(), QString("retro"));
        QVERIFY(e.error().contains("theme", Qt::CaseInsensitive));

        QTemporaryDir dir;
        const QString path = dir.filePath("themed.json");
        QVERIFY(e.save(path));
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.themeId(), QString("retro"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        json["themeId"] = "missing";
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(json).toJson());
        f.close();
        QVERIFY(!loaded.open(path));
        QCOMPARE(loaded.themeId(), QString("retro"));
        json.remove("themeId");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(json).toJson());
        f.close();
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.themeId(), QString("lab"));
    }
    void editingAndHistory() {
        Engine e;
        int count = e.nodeCount();
        QSignalSpy edit(&e, &Engine::editRequested);
        e.addChild();
        int id = e.selectedId();
        QCOMPARE(e.nodeCount(), count + 1);
        QCOMPARE(e.nodes()[id].parent, 1);
        QCOMPARE(edit.size(), 1);
        e.setText(id, "<b>New idea</b>");
        e.setNotes("Details");
        e.toggleTask();
        e.toggleChecked();
        QVERIFY(e.selectedTask());
        QVERIFY(e.selectedChecked());
        QCOMPARE(e.selectedNotes(), QString("Details"));
        e.undo();
        QVERIFY(!e.selectedChecked());
        e.redo();
        QVERIFY(e.selectedChecked());
        e.addSibling();
        int sibling = e.selectedId();
        QCOMPARE(e.nodes()[sibling].parent, 1);
        e.select(id);
        e.select(sibling, true);
        e.removeSelected();
        QCOMPARE(e.nodeCount(), count);
        e.undo();
        QCOMPARE(e.nodeCount(), count + 2);
    }
    void folding() {
        Engine e;
        int branch = e.nodes()[1].children.first();
        e.select(branch);
        int before = e.visibleCount();
        e.toggleFold();
        QVERIFY(e.visibleCount() < before);
        QVERIFY(e.selectedFolded());
        e.undo();
        QCOMPARE(e.visibleCount(), before);
        e.select(e.nodes()[branch].children.first());
        e.select(branch, true);
        e.toggleFold();
        QVERIFY(e.visibleIds().contains(e.selectedId()));
    }
    void reparenting() {
        Engine e;
        auto branches = e.nodes()[1].children;
        int a = branches[0], b = branches[1], child = e.nodes()[a].children.first();
        e.moveNode(a, child);
        QCOMPARE(e.nodes()[a].parent, 1);
        QVERIFY(!e.error().isEmpty());
        e.moveNode(child, b);
        QCOMPARE(e.nodes()[child].parent, b);
        QVERIFY(!e.nodes()[a].children.contains(child));
        QVERIFY(e.nodes()[b].children.contains(child));
        e.undo();
        QCOMPARE(e.nodes()[child].parent, a);
        e.redo();
        QCOMPARE(e.nodes()[child].parent, b);
        e.moveNode(b, 1, a);
        QCOMPARE(e.nodes()[1].children.first(), b);
        e.moveNode(1, b);
        QCOMPARE(e.nodes()[1].parent, -1);
    }
    void layouts() {
        Engine e;
        e.loadFixture(1000);
        e.setText(9, QString(600, 'W'));
        for (QString layout : {"Horizontal", "Vertical", "Compact"}) {
            e.setLayout(layout);
            QCOMPARE(e.visibleCount(), 1000);
            for (int i = 0; i < e.visibleIds().size(); ++i) {
                QRectF a = e.nodes()[e.visibleIds()[i]].rect;
                QVERIFY(a.width() > 0 && a.height() > 0);
                QVERIFY(e.bounds().contains(a));
                for (int j = i + 1; j < e.visibleIds().size(); ++j)
                    QVERIFY2(!a.intersects(e.nodes()[e.visibleIds()[j]].rect), qPrintable(layout));
            }
        }
        e.setLayout("Horizontal");
        QRectF narrow = e.bounds();
        e.setSpacing("Wide");
        QVERIFY(e.bounds().height() > narrow.height());
        e.select(1);
        e.navigate("right");
        QVERIFY(e.selectedId() != 1);
        e.navigate("left");
        QCOMPARE(e.selectedId(), 1);
        e.setLayout("Vertical");
        e.navigate("down");
        QVERIFY(e.selectedId() != 1);
        e.navigate("up");
        QCOMPARE(e.selectedId(), 1);
        e.setManual(true);
        int id = e.selectedId();
        QPointF pos = e.nodes()[id].rect.topLeft();
        e.moveManual(id, 20, 30);
        QCOMPARE(e.nodes()[id].rect.topLeft(), pos + QPointF(20, 30));
        e.undo();
        QCOMPARE(e.nodes()[id].rect.topLeft(), pos);
    }
    void persistence() {
        Engine e;
        QTemporaryDir dir;
        QString path = dir.filePath("map.json");
        e.setText(1, "Saved map");
        e.setNotes("Notes");
        e.setLayout("Vertical");
        QVERIFY(e.save(path));
        Engine read;
        QVERIFY(read.open(path));
        QCOMPARE(read.selectedText(), QString("Saved map"));
        QCOMPARE(read.selectedNotes(), QString("Notes"));
        QCOMPARE(read.layout(), QString("Vertical"));
        QCOMPARE(read.nodeCount(), e.nodeCount());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        QJsonArray nodes = json["nodes"].toArray();
        nodes.append(nodes.first());
        json["nodes"] = nodes;
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(json).toJson());
        f.close();
        int before = read.nodeCount();
        QVERIFY(!read.open(path));
        QCOMPARE(read.nodeCount(), before);
        QCOMPARE(read.selectedText(), QString("Saved map"));
        QVERIFY(!read.save(dir.path()));
    }
    void malformedDocuments() {
        Engine e;
        QTemporaryDir dir;
        QString path = dir.filePath("bad.json");
        QVERIFY(e.save(path));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonObject original = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        for (int mutation = 0; mutation < 7; ++mutation) {
            QJsonObject obj = original;
            QJsonArray nodes = obj["nodes"].toArray();
            QJsonObject root = nodes[0].toObject();
            QJsonObject second = nodes[1].toObject();
            if (mutation == 0)
                obj["version"] = 1.5;
            if (mutation == 1) {
                root["parent"] = 2;
                nodes[0] = root;
            }
            if (mutation == 2) {
                second["parent"] = 999;
                nodes[1] = second;
            }
            if (mutation == 3) {
                QJsonArray children = second["children"].toArray();
                children.append(1);
                second["children"] = children;
                nodes[1] = second;
            }
            if (mutation == 4) {
                root["children"] = QJsonArray();
                nodes[0] = root;
            }
            if (mutation == 5) {
                second["x"] = 1e20;
                nodes[1] = second;
            }
            if (mutation == 6) {
                second["id"] = 2.5;
                nodes[1] = second;
            }
            obj["nodes"] = nodes;
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QJsonDocument(obj).toJson());
            f.close();
            QVERIFY2(!e.open(path), qPrintable(QString("Mutation %1").arg(mutation)));
            QCOMPARE(e.nodeCount(), 15);
            QCOMPARE(e.selectedText(), QString("Mindmap Lab"));
        }
    }
    void relationships() {
        Engine e;
        e.select(2);
        e.select(3, true);
        e.connectSelection();
        QCOMPARE(e.connectionCount(), 1);
        e.connectSelection();
        QCOMPARE(e.connectionCount(), 1);
        e.undo();
        QCOMPARE(e.connectionCount(), 0);
        e.redo();
        QCOMPARE(e.connectionCount(), 1);
        QTemporaryDir dir;
        QString path = dir.filePath("connections.json");
        QVERIFY(e.save(path));
        Engine loaded;
        QVERIFY(loaded.open(path));
        QCOMPARE(loaded.connections(), e.connections());
        e.select(2);
        e.removeSelected();
        QCOMPARE(e.connectionCount(), 0);
        e.undo();
        QCOMPARE(e.connectionCount(), 1);
    }
    void selectionNotificationsAndFoldedNavigation() {
        Engine e;
        QSignalSpy changed(&e, &Engine::changed), outline(&e, &Engine::outlineChanged);
        e.selectMany({2, 3, 4});
        QCOMPARE(e.selection().size(), 3);
        QCOMPARE(e.selectedId(), 4);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(outline.size(), 0);
        e.select(2);
        e.toggleFold();
        int count = e.visibleCount();
        e.navigate("right");
        QCOMPARE(e.selectedId(), 2);
        QVERIFY(e.selectedFolded());
        QCOMPARE(e.visibleCount(), count);
        QVERIFY(e.setText(2, "Updated"));
        QVERIFY(!e.setText(2, QString(16385, 'x')));
        QCOMPARE(e.selectedText(), QString("Updated"));
        QVERIFY(outline.size() > 0);
    }
    void deselection() {
        Engine e;
        int count = e.nodeCount();
        e.select(-1);
        QCOMPARE(e.selectedId(), -1);
        QVERIFY(e.selection().isEmpty());
        e.toggleTask();
        e.toggleChecked();
        e.setNotes("none");
        QCOMPARE(e.nodeCount(), count);
        e.setSpacing("Wide");
        QCOMPARE(e.selectedId(), -1);
        QVERIFY(e.selection().isEmpty());
        e.navigate("right");
        QCOMPARE(e.selectedId(), 1);
    }
    void compactInvariants() {
        Engine e;
        e.setSpacing("Wide");
        e.setManual(true);
        e.setLayout("Compact");
        QVERIFY(!e.manual());
        e.setManual(true);
        QVERIFY(!e.manual());
        e.setSpacing("Narrow");
        QCOMPARE(e.spacing(), QString("Wide"));
    }
    void parentRelativeManualOffsets() {
        Engine e;
        e.setManual(true);
        int child = e.nodes()[1].children.first();
        int grandchild = e.nodes()[child].children.first();
        QPointF root = e.nodes()[1].rect.topLeft(), c = e.nodes()[child].rect.topLeft(),
                g = e.nodes()[grandchild].rect.topLeft();
        e.moveManual(1, 40, 60);
        QCOMPARE(e.nodes()[1].rect.topLeft(), root + QPointF(40, 60));
        QCOMPARE(e.nodes()[child].rect.topLeft(), c + QPointF(40, 60));
        QCOMPARE(e.nodes()[grandchild].rect.topLeft(), g + QPointF(40, 60));
        e.moveManual(child, 10, 20);
        QCOMPARE(e.nodes()[grandchild].rect.topLeft(), g + QPointF(50, 80));
        e.undo();
        QCOMPARE(e.nodes()[grandchild].rect.topLeft(), g + QPointF(40, 60));
    }
    void rejectsOversizedRichText() {
        Engine e;
        QString before = e.selectedText();
        QString huge = "<span style='font-size:100000px'>Huge</span>";
        QVERIFY(!e.setText(1, huge));
        QCOMPARE(e.selectedText(), before);
        QVERIFY(!e.canUndo());
        QVERIFY(e.error().contains("too tall"));
        QVERIFY(!e.setText(1, QString("Line<br>").repeated(1000)));
        QCOMPARE(e.selectedText(), before);
        QVERIFY(e.setText(1, "<b>Normal rich text</b>"));
        QTemporaryDir dir;
        QString path = dir.filePath("huge.json");
        QVERIFY(e.save(path));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        QJsonArray nodes = obj["nodes"].toArray();
        QJsonObject root = nodes[0].toObject();
        root["text"] = huge;
        nodes[0] = root;
        obj["nodes"] = nodes;
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(obj).toJson());
        f.close();
        QVERIFY(!e.open(path));
        QCOMPARE(e.selectedText(), QString("<b>Normal rich text</b>"));
        QVERIFY(e.error().contains("too tall"));
    }
    void stableCompactPacking() {
        Engine narrow, wide;
        narrow.setSpacing("Narrow");
        wide.setSpacing("Wide");
        narrow.setLayout("Compact");
        wide.setLayout("Compact");
        QCOMPARE(narrow.bounds(), wide.bounds());
        Engine manual;
        manual.setManual(true);
        manual.setSpacing("Wide");
        QCOMPARE(manual.spacing(), QString("Standard"));
    }
    void textMeasurementUpdates() {
        Engine e;
        e.setText(1, "Short title");
        QSizeF shortSize = e.nodes()[1].rect.size();
        e.setText(1, QString(350, 'W'));
        QSizeF longSize = e.nodes()[1].rect.size();
        QVERIFY(longSize.height() > shortSize.height());
        e.setLayout("Vertical");
        QCOMPARE(e.nodes()[1].rect.size(), longSize);
        e.toggleTask();
        QCOMPARE(e.nodes()[1].rect.width(), longSize.width() + 20);
        QCOMPARE(e.nodes()[1].rect.height(), longSize.height());
        QTextDocument doc;
        QFont font("sans-serif", 11);
        font.setPixelSize(15);
        doc.setDefaultFont(font);
        doc.setDocumentMargin(0);
        doc.setDefaultStyleSheet("body,p {color:#e9eff4; margin:0;} a {color:#8be4cf;}");
        doc.setHtml(e.selectedText());
        doc.setTextWidth(e.nodes()[1].rect.width() - 50);
        QVERIFY(e.nodes()[1].rect.height() >= doc.size().height() + 16);
        e.undo();
        QCOMPARE(e.nodes()[1].rect.size(), longSize);
        e.setText(1, "Short title");
        QCOMPARE(e.nodes()[1].rect.size(), shortSize);
        e.toggleChecked();
        QVERIFY(e.selectedTask());
        QCOMPARE(e.nodes()[1].rect.width(), shortSize.width() + 20);
    }
    void boundedHistory() {
        Engine e;
        for (int i = 0; i < 60; ++i)
            e.setText(1, QString("Revision %1").arg(i));
        int steps = 0;
        while (e.canUndo()) {
            e.undo();
            ++steps;
            QVERIFY(steps <= 40);
        }
        QCOMPARE(steps, 40);
        QVERIFY(e.canRedo());
        e.setText(1, "New history branch");
        QVERIFY(!e.canRedo());
    }
    void fixtureLimitAndPerformance() {
        Engine e;
        for (int count : {1000, 10000}) {
            e.loadFixture(count);
            QCOMPARE(e.nodeCount(), count);
            QCOMPARE(e.visibleCount(), count);
            qInfo() << count << "initial fixture layout milliseconds" << e.layoutMs();
            e.setLayout("Vertical");
            for (QString layout : {"Horizontal", "Vertical", "Compact"}) {
                e.setLayout(layout);
                qInfo() << count << layout << "layout milliseconds" << e.layoutMs();
            }
        }
        e.loadFixture(100001);
        QCOMPARE(e.nodeCount(), 10000);
        QVERIFY(!e.error().isEmpty());
    }
};
QTEST_MAIN(EngineTest)
#include "engine_test.moc"
