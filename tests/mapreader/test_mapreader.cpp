#include "map.h"
#include "mapobject.h"
#include "grouplayer.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "mapreader.h"

#include <QtTest/QtTest>

using namespace Tiled;

class test_MapReader : public QObject
{
    Q_OBJECT

private slots:
    void loadMap();
    void loadComment();
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

void test_MapReader::loadComment()
{
    MapReader reader;
    auto map = reader.readMap("../data/mapobject-comment.tmx");

    QVERIFY(map.get());

    ObjectGroup *objectGroup = dynamic_cast<ObjectGroup*>(map->layerAt(0));
    QVERIFY(objectGroup);
    QCOMPARE(objectGroup->name(), QLatin1String("Items"));
    QCOMPARE(objectGroup->comment(), QLatin1String("Spawn points for the whole level"));
    QCOMPARE(objectGroup->objects().count(), 3);

    // Comment and color are both set
    MapObject *first = objectGroup->objects().at(0);
    QCOMPARE(first->name(), QLatin1String("Common crate"));
    QCOMPARE(first->comment(), QLatin1String("Common loot, respawns every 5 minutes"));
    QCOMPARE(first->color(), QColor("#00ff00"));
    QVERIFY(first->propertyChanged(MapObject::CommentProperty));
    QVERIFY(first->propertyChanged(MapObject::ColorProperty));

    // Comment set, no color
    MapObject *second = objectGroup->objects().at(1);
    QCOMPARE(second->comment(), QLatin1String("Opens the second quest stage"));
    QVERIFY(!second->color().isValid());
    QVERIFY(!second->propertyChanged(MapObject::ColorProperty));
    QVERIFY(second->propertyChanged(MapObject::CommentProperty));

    // Neither set
    MapObject *third = objectGroup->objects().at(2);
    QVERIFY(third->comment().isEmpty());
    QVERIFY(!third->color().isValid());
    QVERIFY(!third->propertyChanged(MapObject::CommentProperty));

    GroupLayer *groupLayer = dynamic_cast<GroupLayer*>(map->layerAt(1));
    QVERIFY(groupLayer);
    QCOMPARE(groupLayer->comment(), QLatin1String("Purely visual"));

    // A layer nested in a group keeps its own comment
    auto *nested = groupLayer->layerAt(0);
    QVERIFY(nested);
    QVERIFY(nested->comment().isEmpty());
}

QTEST_MAIN(test_MapReader)
#include "test_mapreader.moc"