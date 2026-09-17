/*
 * layerdefinitioninstantiator.h
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

#pragma once

namespace Tiled {

class Layer;
class MapDocument;
class Project;

/**
 * Responsible for keeping a Map in sync with the LayerDefinitions of the
 * Project it belongs to: creates any inherited layer that is missing from
 * the map (recursively, including nested groups), each addition pushed as
 * a normal undoable AddLayer command.
 *
 * Does nothing (and touches the undo stack not at all) if the map already
 * has a matching layer for every LayerDefinition in the project.
 */
namespace LayerDefinitionInstantiator {

void reconcile(MapDocument *mapDocument, const Project &project);

/**
 * Applies the current state of the project's LayerDefinitions to every
 * currently open MapDocument: creates any layer that's missing (same as
 * reconcile()) and renames already-existing inherited layers whose name
 * no longer matches their definition.
 *
 * Call this after adding, renaming or removing a LayerDefinition so open
 * maps don't require being closed and reopened to pick up the change.
 *
 * NOTE: does not yet re-order or re-parent already-existing layer
 * instances when a definition's order/parent changes; that only takes
 * effect for newly created instances.
 */
void syncAllOpenDocuments(const Project &project);

/**
 * Returns whether \a layer is currently protected from deletion, i.e. it
 * has a definitionId AND that definition still exists in the current
 * project. A layer whose definition was since removed from the project
 * is considered orphaned and is no longer protected.
 */
bool isLayerProtected(const Layer *layer);

} // namespace LayerDefinitionInstantiator
} // namespace Tiled
