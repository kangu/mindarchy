#include "collaboration/liveoperations.h"
#include <QtTest>
class LiveOperationsTest : public QObject {
    Q_OBJECT
  private slots:
    void deletionWinsAndInvalidMovesAreAtomic() {
        QJsonObject c{{"props", QJsonObject{}},
                      {"nodes", QJsonObject{{"r", QJsonObject{{"parent", ""}, {"order", 0}}},
                                            {"a", QJsonObject{{"parent", "r"}, {"order", 0}, {"text", "old"}}},
                                            {"b", QJsonObject{{"parent", "a"}, {"order", 0}}}}},
                      {"edges", QJsonObject{}}};
        const auto initial = c;
        QVERIFY(!LiveOperations::apply(
            c, QJsonArray{QJsonObject{{"op", "set"}, {"path", QJsonArray{"nodes", "a", "parent"}}, {"value", "b"}}}));
        QCOMPARE(c, initial);
        QVERIFY(
            LiveOperations::apply(c, QJsonArray{QJsonObject{{"op", "remove"}, {"path", QJsonArray{"nodes", "a"}}}}));
        QCOMPARE(c["nodes"].toObject().size(), 1);
        QVERIFY(LiveOperations::apply(
            c, QJsonArray{QJsonObject{{"op", "set"}, {"path", QJsonArray{"nodes", "a", "text"}}, {"value", "late"}}}));
        QCOMPARE(c["nodes"].toObject().size(), 1);
        auto withEdge = initial;
        withEdge["edges"] = QJsonObject{{"a", QJsonObject{{"b", true}}}};
        auto edits = LiveOperations::diff(initial, withEdge);
        QCOMPARE(edits.first().toObject()["path"].toArray(), QJsonArray({"edges", "a", "b"}));
    }
    void merge() {
        QJsonObject base{
            {"props", QJsonObject{}},
            {"nodes", QJsonObject{{"r", QJsonObject{{"parent", ""}, {"order", 0}, {"text", "root"}, {"notes", ""}}}}},
            {"edges", QJsonObject{}}};
        auto a = base, b = base;
        auto nodes = a["nodes"].toObject(), node = nodes["r"].toObject();
        node["text"] = "A";
        nodes["r"] = node;
        a["nodes"] = nodes;
        nodes = b["nodes"].toObject();
        node = nodes["r"].toObject();
        node["notes"] = "B";
        nodes["r"] = node;
        b["nodes"] = nodes;
        QVERIFY(LiveOperations::diff(base, base).isEmpty());
        QString error;
        QVERIFY(LiveOperations::apply(b, LiveOperations::diff(base, a), &error));
        QCOMPARE(b["nodes"].toObject()["r"].toObject()["text"].toString(), QString("A"));
        QCOMPARE(b["nodes"].toObject()["r"].toObject()["notes"].toString(), QString("B"));
        QCOMPARE(LiveOperations::normalize(LiveOperations::project(b)), b);
    }
};
QTEST_GUILESS_MAIN(LiveOperationsTest)
#include "liveoperations_test.moc"
