/*
 * layerdefinitioninstantiator.cpp
 * Copyright 2026
 *
 * This file is part of Tiled.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "layerdefinitioninstantiator.h"

#include "addremovelayer.h"
#include "changelayer.h"
#include "documentmanager.h"
#include "grouplayer.h"
#include "imagelayer.h"
#include "layer.h"
#include "map.h"
#include "mapdocument.h"
#include "objectgroup.h"
#include "project.h"
#include "projectmanager.h"
#include "tilelayer.h"

#include <QCoreApplication>
#include <QHash>
#include <QUndoStack>

namespace Tiled {
namespace LayerDefinitionInstantiator {

static Layer *createLayer(const LayerDefinition &definition, const QSize &mapSize)
{
    switch (definition.type) {
    case LayerDefinition::TileLayerDef:
        return new TileLayer(definition.name, 0, 0, mapSize.width(), mapSize.height());
    case LayerDefinition::ObjectGroupDef:
        return new ObjectGroup(definition.name, 0, 0);
    case LayerDefinition::ImageLayerDef:
        return new ImageLayer(definition.name, 0, 0);
    case LayerDefinition::GroupLayerDef:
        return new GroupLayer(definition.name, 0, 0);
    }
    return nullptr;
}

/**
 * Recursively makes sure every LayerDefinition whose parentId is
 * \a parentDefinitionId has a matching layer instance underneath
 * \a parentLayer (nullptr meaning the top of the map).
 *
 * NOTE: for now, newly created layers are simply appended at the end of
 * their parent. Interleaving them at a precise position relative to
 * layers the user added manually is left as a future improvement.
 */
static void reconcileLevel(MapDocument *mapDocument,
                           const Project &project,
                           const QString &parentDefinitionId,
                           GroupLayer *parentLayer,
                           QHash<QString, Layer *> &existingByDefinitionId)
{
    const QVector<const LayerDefinition *> children = project.layerDefinitions().childrenOf(parentDefinitionId);

    for (const LayerDefinition *definition : children) {
        Layer *layer = existingByDefinitionId.value(definition->id, nullptr);

        if (!layer) {
            Layer *newLayer = createLayer(*definition, mapDocument->map()->size());
            if (!newLayer)
                continue;

            newLayer->setDefinitionId(definition->id);
            newLayer->setVisible(definition->visibleByDefault);
            if (!definition->defaultProperties.isEmpty())
                newLayer->setProperties(definition->defaultProperties);

            const int index = parentLayer ? parentLayer->layerCount()
                                          : mapDocument->map()->layerCount();

            mapDocument->undoStack()->push(new AddLayer(mapDocument, index,
                                                        newLayer, parentLayer));

            layer = newLayer;
            existingByDefinitionId.insert(definition->id, layer);
        }

        if (definition->isGroup()) {
            if (auto *groupLayer = dynamic_cast<GroupLayer *>(layer)) {
                reconcileLevel(mapDocument, project, definition->id, groupLayer,
                              existingByDefinitionId);
            }
        }
    }
}

/**
 * Renames already-existing inherited layers whose name has drifted from
 * their LayerDefinition (i.e. the definition was renamed after the layer
 * was created). Pushed through the undo stack, same as any other layer
 * edit, so it can be undone like everything else in that map.
 */
static void syncNames(MapDocument *mapDocument, const Project &project)
{
    LayerIterator it(mapDocument->map());
    while (Layer *layer = it.next()) {
        if (layer->definitionId().isEmpty())
            continue;

        const LayerDefinition *definition = project.layerDefinitions().findById(layer->definitionId());
        if (definition && layer->name() != definition->name)
            mapDocument->undoStack()->push(new SetLayerName(mapDocument, { layer }, definition->name));
    }
}

void reconcile(MapDocument *mapDocument, const Project &project)
{
    if (!mapDocument || !mapDocument->map())
        return;

    syncNames(mapDocument, project);

    const int definitionCount = project.layerDefinitions().count();
    if (definitionCount == 0)
        return;

    QHash<QString, Layer *> existingByDefinitionId;

    LayerIterator it(mapDocument->map());
    while (Layer *layer = it.next()) {
        if (!layer->definitionId().isEmpty())
            existingByDefinitionId.insert(layer->definitionId(), layer);
    }

    // Skip touching the undo stack entirely when nothing is missing.
    bool anyMissing = false;
    for (int i = 0; i < definitionCount; ++i) {
        if (!existingByDefinitionId.contains(project.layerDefinitions().at(i)->id)) {
            anyMissing = true;
            break;
        }
    }
    if (!anyMissing)
        return;

    mapDocument->undoStack()->beginMacro(
                QCoreApplication::translate("Tiled::LayerDefinitionInstantiator",
                                            "Add Inherited Layers"));

    reconcileLevel(mapDocument, project, QString(), nullptr, existingByDefinitionId);

    mapDocument->undoStack()->endMacro();
}

void syncAllOpenDocuments(const Project &project)
{
    const auto &documents = DocumentManager::instance()->documents();
    for (const DocumentPtr &document : documents) {
        if (auto *mapDocument = qobject_cast<MapDocument *>(document.data()))
            reconcile(mapDocument, project);
    }
}

bool isLayerProtected(const Layer *layer)
{
    if (!layer || layer->definitionId().isEmpty())
        return false;

    return ProjectManager::instance()->project().layerDefinitions().findById(layer->definitionId()) != nullptr;
}

} // namespace LayerDefinitionInstantiator
} // namespace Tiled
