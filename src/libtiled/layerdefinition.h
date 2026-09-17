/*
 * layerdefinition.h
 * Copyright 2026
 *
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "properties.h"
#include "tiled_global.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <vector>

namespace Tiled {

class ExportContext;

/**
 * Describes a layer that is defined once at the Project level and
 * automatically instantiated on every Map that belongs to the project
 * (similar to LDtk's layer definitions).
 *
 * A LayerDefinition itself does not hold any map content (no tiles, no
 * objects). It only describes the *shape* of the inherited layer: its
 * type, its place in the layer hierarchy, and some defaults. The actual
 * per-map Layer instance still lives in the Map/TMX as usual, but carries
 * a matching definitionId so it can be recognized, protected from
 * deletion, and kept in sync with the definition (name, order, nesting).
 */
class TILEDSHARED_EXPORT LayerDefinition
{
public:
    enum Type {
        TileLayerDef,
        ObjectGroupDef,
        ImageLayerDef,
        GroupLayerDef
    };

    /**
     * Stable identifier, generated once and never changed. Used to match
     * this definition to its instance layer on each map, regardless of
     * renames or reordering.
     */
    QString id;

    QString name;
    Type type = ObjectGroupDef;

    /**
     * Tags assigned to this layer definition, used later for things like
     * filtering which tilesets are relevant for this layer (e.g. in a
     * tileset dock). Duplicates are never allowed within one definition
     * (enforced by whoever assigns tags — see addTag()/removeTag() on
     * LayerDefinitionsEditor and Project::removeTag()/renameTag() for the
     * registry-wide operations).
     */
    QStringList tags;

    /**
     * Id of the parent LayerDefinition (which must be of GroupLayerDef
     * type), or an empty string if this is a top-level definition.
     */
    QString parentId;

    /**
     * Order among sibling definitions (within the same parentId).
     */
    int order = 0;

    /**
     * Default properties applied to newly instantiated layers.
     */
    Properties defaultProperties;

    bool visibleByDefault = true;

    LayerDefinition() = default;
    LayerDefinition(const QString &name, Type type);

    bool isGroup() const { return type == GroupLayerDef; }

    /**
     * Adds \a tag to this definition's tags, unless it's already present
     * (or empty). Returns whether it was actually added.
     */
    bool addTag(const QString &tag);

    /**
     * Removes \a tag from this definition's tags, if present. This is a
     * purely local operation — it does NOT touch the project's tag
     * registry, and does not affect any other layer definition.
     */
    void removeTag(const QString &tag);

    QJsonObject toJson(const ExportContext &context) const;
    static LayerDefinition fromJson(const QJsonObject &json, const ExportContext &context);

    static Type typeFromString(const QString &string);
    static QString typeToString(Type type);

    static QString generateId();
};

/**
 * An ordered collection of LayerDefinition, owned by a Project.
 *
 * Definitions are stored behind std::unique_ptr so that pointers handed
 * out by findById()/at()/childrenOf() remain valid even when the
 * collection is modified elsewhere (adding/removing other definitions
 * does not reallocate existing LayerDefinition instances).
 *
 * NOTE: uses std::vector rather than QVector/QList, because Qt6's
 * containers require a copyable value_type for some internal operations
 * (e.g. reserve()), which std::unique_ptr is not. std::vector fully
 * supports move-only element types.
 */
class TILEDSHARED_EXPORT LayerDefinitions
{
public:
    LayerDefinitions() = default;
    LayerDefinitions(const LayerDefinitions &other);
    LayerDefinitions &operator=(const LayerDefinitions &other);
    LayerDefinitions(LayerDefinitions &&other) = default;
    LayerDefinitions &operator=(LayerDefinitions &&other) = default;

    int count() const { return static_cast<int>(mDefinitions.size()); }

    LayerDefinition *at(int index) { return mDefinitions.at(size_t(index)).get(); }
    const LayerDefinition *at(int index) const { return mDefinitions.at(size_t(index)).get(); }

    int indexOf(const LayerDefinition *definition) const;

    LayerDefinition *findById(const QString &id);
    const LayerDefinition *findById(const QString &id) const;

    /**
     * Returns the direct children of \a parentId, already sorted by
     * their `order` field. Pass an empty string to get the top-level
     * definitions.
     */
    QVector<LayerDefinition *> childrenOf(const QString &parentId);
    QVector<const LayerDefinition *> childrenOf(const QString &parentId) const;

    LayerDefinition *add(LayerDefinition definition);
    void removeAt(int index);
    void moveDefinition(int from, int to);

    void clear() { mDefinitions.clear(); }

    void loadFromJson(const QJsonArray &array, const ExportContext &context);
    QJsonArray toJson(const ExportContext &context) const;

private:
    std::vector<std::unique_ptr<LayerDefinition>> mDefinitions;
};

} // namespace Tiled
