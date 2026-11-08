#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "mapreader.h"
#include "mapwriter.h"
#include "maptovariantconverter.h"
#include "varianttomapconverter.h"
#include "tileset.h"
#include "wangset.h"

#include <QBuffer>
#include <QtTest/QtTest>

using namespace Tiled;

class test_MapReader : public QObject
{
    Q_OBJECT

private slots:
    void loadMap();
    void loadCellBasedTileset();
    void roundTripTmxCellBasedWangSet();
    void roundTripJsonCellBasedWangSet();
    void classicWangSetSerializationIdentical();
};

void test_MapReader::loadMap()
{
    MapReader reader;
    auto map = reader.readMap("../data/mapobject.tmx");

    // TODO: Also test tilesets (internal and external), properties and tile
    // layer data.

    QVERIFY(map.get());
    QCOMPARE(map->layerCount(), 2);
    QCOMPARE(map->width(), 100);
    QCOMPARE(map->height(), 80);
    QCOMPARE(map->tileWidth(), 32);
    QCOMPARE(map->tileHeight(), 32);

    TileLayer *tileLayer = dynamic_cast<TileLayer*>(map->layerAt(0));

    QVERIFY(tileLayer);
    QCOMPARE(tileLayer->width(), 100);
    QCOMPARE(tileLayer->height(), 80);

    ObjectGroup *objectGroup = dynamic_cast<ObjectGroup*>(map->layerAt(1));

    QVERIFY(objectGroup);
    QCOMPARE(objectGroup->name(), QLatin1String("Objects"));
    QCOMPARE(objectGroup->objects().count(), 1);

    MapObject *mapObject = objectGroup->objects().at(0);

    QCOMPARE(mapObject->name(), QLatin1String("Some object"));
    QCOMPARE(mapObject->className(), QLatin1String("WARP"));
    QCOMPARE(mapObject->x(), qreal(200));
    QCOMPARE(mapObject->y(), qreal(200));
    QCOMPARE(mapObject->width(), qreal(128));
    QCOMPARE(mapObject->height(), qreal(64));
}

void test_MapReader::loadCellBasedTileset()
{
    MapReader reader;
    auto tileset = reader.readTileset("../wangtiles/cellbased_grass_and_water.tsx");
    QVERIFY(tileset.data());
    QCOMPARE(tileset->wangSetCount(), 1);

    WangSet *ws = tileset->wangSet(0);
    QVERIFY(ws->isCellBased());
    QCOMPARE(ws->centerColor(0), 1);
    QCOMPARE(ws->centerColor(3), 1);
    QCOMPARE(ws->centerColor(22), 2);
    QCOMPARE(ws->centerColor(23), 2);

    Tile *loneTile3 = tileset->findTile(3);
    QVERIFY(loneTile3);
    QVERIFY(ws->wangIdOfTile(loneTile3).isEmpty());
}

void test_MapReader::roundTripTmxCellBasedWangSet()
{
    MapReader reader;
    auto tileset = reader.readTileset("../wangtiles/cellbased_grass_and_water.tsx");
    QVERIFY(tileset.data());

    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    MapWriter writer;
    writer.writeTileset(*tileset, &buffer);
    buffer.close();

    const QByteArray xmlData = buffer.data();
    QVERIFY(xmlData.contains("cellbased=\"1\""));
    QVERIFY(xmlData.contains("center=\"1\""));
    QVERIFY(xmlData.contains("center=\"2\""));

    buffer.open(QIODevice::ReadOnly);
    auto roundTripTileset = reader.readTileset(&buffer, "../wangtiles/");
    QVERIFY(roundTripTileset.data());
    QCOMPARE(roundTripTileset->wangSetCount(), 1);

    WangSet *ws = roundTripTileset->wangSet(0);
    QVERIFY(ws->isCellBased());
    QCOMPARE(ws->centerColor(0), 1);
    QCOMPARE(ws->centerColor(3), 1);
    QCOMPARE(ws->centerColor(22), 2);
    QCOMPARE(ws->centerColor(23), 2);
}

void test_MapReader::roundTripJsonCellBasedWangSet()
{
    MapReader reader;
    auto tileset = reader.readTileset("../wangtiles/cellbased_grass_and_water.tsx");
    QVERIFY(tileset.data());

    MapToVariantConverter toVariant;
    const QVariant tsVariant = toVariant.toVariant(*tileset, QDir(QStringLiteral("../wangtiles")));
    const QVariantMap tsMap = tsVariant.toMap();
    const QVariantList wsList = tsMap.value(QStringLiteral("wangsets")).toList();
    QCOMPARE(wsList.size(), 1);

    const QVariantMap wsMap = wsList.at(0).toMap();
    QVERIFY(wsMap.value(QStringLiteral("cellbased")).toBool());

    const QVariantList wtList = wsMap.value(QStringLiteral("wangtiles")).toList();
    bool foundCenter1 = false;
    bool foundCenter2 = false;
    for (const QVariant &wtVar : wtList) {
        const QVariantMap wtMap = wtVar.toMap();
        if (wtMap.value(QStringLiteral("tileid")).toInt() == 3) {
            QCOMPARE(wtMap.value(QStringLiteral("center")).toInt(), 1);
            foundCenter1 = true;
        }
        if (wtMap.value(QStringLiteral("tileid")).toInt() == 23) {
            QCOMPARE(wtMap.value(QStringLiteral("center")).toInt(), 2);
            foundCenter2 = true;
        }
    }
    QVERIFY(foundCenter1);
    QVERIFY(foundCenter2);

    VariantToMapConverter toMap;
    auto jsonTileset = toMap.toTileset(tsVariant, QDir(QStringLiteral("../wangtiles")));
    QVERIFY(jsonTileset.data());
    QCOMPARE(jsonTileset->wangSetCount(), 1);
    WangSet *jsonWs = jsonTileset->wangSet(0);
    QVERIFY(jsonWs->isCellBased());
    QCOMPARE(jsonWs->centerColor(0), 1);
    QCOMPARE(jsonWs->centerColor(3), 1);
    QCOMPARE(jsonWs->centerColor(22), 2);
    QCOMPARE(jsonWs->centerColor(23), 2);
}

void test_MapReader::classicWangSetSerializationIdentical()
{
    MapReader reader;
    auto classicTileset = reader.readTileset("../wangtiles/grassAndWater.tsx");
    QVERIFY(classicTileset.data());
    QCOMPARE(classicTileset->wangSetCount(), 1);
    QVERIFY(!classicTileset->wangSet(0)->isCellBased());

    // TMX serialization should never contain cellbased or center
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    MapWriter writer;
    writer.writeTileset(*classicTileset, &buffer);
    buffer.close();

    const QByteArray xmlData = buffer.data();
    QVERIFY(!xmlData.contains("cellbased"));
    QVERIFY(!xmlData.contains("center="));

    // JSON serialization should never contain cellbased or center
    MapToVariantConverter toVariant;
    const QVariant tsVariant = toVariant.toVariant(*classicTileset, QDir(QStringLiteral("../wangtiles")));
    const QVariantMap tsMap = tsVariant.toMap();
    const QVariantList wsList = tsMap.value(QStringLiteral("wangsets")).toList();
    QCOMPARE(wsList.size(), 1);
    const QVariantMap wsMap = wsList.at(0).toMap();
    QVERIFY(!wsMap.contains(QStringLiteral("cellbased")));

    const QVariantList wtList = wsMap.value(QStringLiteral("wangtiles")).toList();
    for (const QVariant &wtVar : wtList) {
        const QVariantMap wtMap = wtVar.toMap();
        QVERIFY(!wtMap.contains(QStringLiteral("center")));
    }
}

QTEST_MAIN(test_MapReader)
#include "test_mapreader.moc"
