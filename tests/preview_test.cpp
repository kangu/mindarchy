#include "engine.h"
#include "preview.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
class PreviewTest : public QObject {
    Q_OBJECT
private slots:
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
        QVERIFY(json.isObject()); QCOMPARE(json.object()["format"].toString(),QString("mindmap-lab"));
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
