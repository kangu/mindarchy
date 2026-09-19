#include "engine.h"
#include "appfont.h"
#include "preview.h"
#include "drawing.h"
#include <QFile>
#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
class PreviewTest : public QObject {
    Q_OBJECT
private slots:
    void automaticFontSizeFollowsHierarchy() {
        Engine e(nullptr,Engine::InitialContent::Blank);
        const QString rich="<span style='font-size:48pt;font-weight:700;font-style:italic'>Same title</span>";
        QVector<int> ids{1};
        QVERIFY(e.setText(1,rich));
        for(int i=0;i<8;++i) { e.addChild(); ids.append(e.selectedId()); QVERIFY(e.setText(e.selectedId(),rich)); }
        for(int depth=0;depth<ids.size();++depth) {
            const int id=ids[depth]; e.select(id);
            QCOMPARE(e.selectedStyle()["fontSize"].toInt(),mindarchyNodeFontSize(depth));
            QVERIFY(e.selectedStyle()["bold"].toBool()); QVERIFY(e.selectedStyle()["italic"].toBool());
            QCOMPARE(e.contentSize(id),e.previewContentSize(id,rich));
            if(depth>0) QVERIFY(e.contentSize(id).width()<=e.contentSize(ids[depth-1]).width());
        }
        const auto small=e.contentSize(ids[4]);
        e.moveNode(ids[4],1); QCOMPARE(e.appearance(ids[4]).fontSize,18);
        QVERIFY(e.contentSize(ids[4]).width()>small.width());
        QCOMPARE(e.appearance(ids[5]).fontSize,17);
        e.undo(); QCOMPARE(e.contentSize(ids[4]),small);
        QTemporaryDir dir; const auto path=dir.filePath("depth.omm"); QVERIFY(e.save(path));
        Engine loaded; QVERIFY(loaded.open(path));
        for(int id:ids) QCOMPARE(loaded.contentSize(id),e.contentSize(id));
        QVERIFY(!renderMapPreview(loaded,QSize(900,600)).isNull());
        QTextDocument doc; doc.setHtml(rich); applyMindarchyNodeSize(doc,14);
        QTextCursor cursor(&doc); cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
        QCOMPARE(cursor.charFormat().font().pixelSize(),14);
        QVERIFY(cursor.charFormat().fontItalic());
    }

    void artisticGeometryIsStableAndThemeAware() {
        using namespace MapDrawing;
        const QStringList styles=branchStyles(); QCOMPARE(styles.size(),7);
        QSet<QByteArray> rendered;
        for(const auto &style:styles.mid(2)) {
            for(bool vertical:{false,true}) for(bool reversed:{false,true}) {
                const QPointF a(20,40),b=vertical?QPointF(140,300):QPointF(reversed?-240:300,150);
                const auto light=branchGeometry(a,b,style,vertical,QColor("#56836c"),Qt::white,2,1,17);
                const auto dark=branchGeometry(a,b,style,vertical,QColor("#56836c"),QColor("#15231d"),2,1,17);
                QVERIFY(!light.isEmpty()); QCOMPARE(light.first().path.first(),a); QCOMPARE(light.first().path.last(),b);
                QVERIFY(dark.first().color.lightnessF()>light.first().color.lightnessF());
                const auto again=branchGeometry(a,b,style,vertical,QColor("#56836c"),Qt::white,2,1,17);
                QCOMPARE(light.size(),again.size());
                for(int i=0;i<light.size();++i) {
                    QCOMPARE(light[i].path,again[i].path);
                    const auto mesh=branchTriangles(light[i]); QVERIFY(!mesh.isEmpty()); QCOMPARE(mesh.size()%3,0);
                    for(const auto &p:mesh) QVERIFY(std::isfinite(p.x())&&std::isfinite(p.y()));
                }
                const auto low=branchGeometry(a,b,style,vertical,QColor("#56836c"),Qt::white,2,1,17,.2);
                QVERIFY(low.size()<light.size()); QCOMPARE(low.first().path,light.first().path);
            }
            QVERIFY(branchGeometry({0,0},{0,0},style,false,Qt::black,Qt::white,2,1,1).isEmpty());
            QVERIFY(branchGeometry({0,0},{80,30},style,false,Qt::black,Qt::white,0,1,1).isEmpty());
            QImage image(360,220,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
            QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
            paintBranches(painter,branchGeometry({20,40},{330,170},style,false,QColor("#56836c"),Qt::white,2,1,17));
            painter.end(); rendered.insert(QByteArray(reinterpret_cast<const char *>(image.constBits()),image.sizeInBytes()));
        }
        QCOMPARE(rendered.size(),5);
    }
    void artisticStyleRoundTripAndUndo() {
        QTemporaryDir directory;
        for (const QString name : {"Botanical graphite", "Living oak", "Sumi branch", "Silver birch", "Elven filigree"}) {
            Engine engine; engine.loadFixture(8); engine.setManual(true);
            const auto rect=engine.nodes()[2].rect;
            engine.setBranchStyle(name);
            QCOMPARE(engine.branchStyle(), name);
            QCOMPARE(engine.nodes()[2].rect,rect);
            engine.undo(); QCOMPARE(engine.branchStyle(),QString("Rounded"));
            engine.redo(); QCOMPARE(engine.branchStyle(),name);
            const auto path=directory.filePath("style.omm");
            QVERIFY(engine.save(path)); Engine loaded; QVERIFY(loaded.open(path));
            QCOMPARE(loaded.branchStyle(),name);
            for (const QString layout : {"Horizontal", "Vertical", "Compact"}) {
                loaded.setLayout(layout);
                QVERIFY(!renderMapPreview(loaded,QSize(512,512)).isNull());
            }
        }
    }

    void embeddedImageIsPainted() {
        Engine engine(nullptr,Engine::InitialContent::Blank);
        QImage pixels(80,40,QImage::Format_RGB32); pixels.fill(QColor("#ff0000"));
        NodeImage image; QVERIFY(NodeImage::importPixels(pixels,image)); QVERIFY(engine.setImage(1,image));
        const auto preview=renderMapPreview(engine); int red=0;
        for(int y=0;y<preview.height();++y) for(int x=0;x<preview.width();++x) {
            const auto c=preview.pixelColor(x,y); if(c.red()>245 && c.green()<10 && c.blue()<10) ++red;
        }
        QVERIFY(red>1000);
    }
    void plainJsonRoundTripAndFreshPreview() {
        QTemporaryDir dir;
        Engine engine; engine.loadFixture(3); engine.select(2);
        QVERIFY(engine.setNodeKind(2,"date"));
        QVERIFY(engine.configureDateNode(2,"month","2026-09-01"));
        QVERIFY(engine.setDateEntry(2,"2026-09-08","125.50"));
        QVERIFY(engine.setDateEntry(2,"2026-09-09","24.50"));
        auto path=dir.filePath("Unicode café map.omm");
        QVERIFY(engine.save(path));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly));
        auto json=QJsonDocument::fromJson(file.readAll());
        file.close(); // Windows cannot atomically replace a file held open by this reader.
        QVERIFY(json.isObject()); QCOMPARE(json.object()["format"].toString(),QString("mindarchy"));
        Engine loaded; QVERIFY(loaded.open(QUrl::fromLocalFile(path).toString()));
        QCOMPARE(loaded.dateEntry(2,"2026-09-08"),QString("125.50"));
        const auto before=renderMapPreview(loaded);
        QVERIFY(!before.isNull()); QVERIFY(before.width()<=1600); QVERIFY(before.height()<=1000);
        QVERIFY(loaded.setDateEntry(2,"2026-09-08","200"));
        QVERIFY(loaded.save(path)); QVERIFY(engine.open(path));
        QVERIFY(before!=renderMapPreview(engine));
        QVERIFY(QFile::copy(path,dir.filePath("same.json")));
        Engine legacy; QVERIFY(legacy.open(dir.filePath("same.json")));
        QCOMPARE(renderMapPreview(engine),renderMapPreview(legacy));
    }
    void boundsAndLayouts() {
        Engine engine;
        for(auto layout:{"Horizontal","Vertical","Compact"}) {
            engine.setLayout(layout);
            const auto image=renderMapPreview(engine,QSize(256,256));
            QVERIFY(!image.isNull()); QVERIFY(image.width()<=256); QVERIFY(image.height()<=256);
        }
        engine.loadFixture(10000);
        auto overview=renderMapPreview(engine,QSize(512,512));
        QVERIFY(!overview.isNull()); QVERIFY(overview.width()<=512); QVERIFY(overview.height()<=512);
    }
};
QTEST_MAIN(PreviewTest)
#include "preview_test.moc"
