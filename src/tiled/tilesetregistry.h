/*
 * tilesetregistry.h
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

#include "tilededitor_global.h"

#include <QDir>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

class QJsonArray;

namespace Tiled {

/**
 * A tileset registered with a Project: a reference to a tileset file
 * (.tsx, or an image for an embedded-style entry), plus tags assigned at
 * the project level (see Project::tags()).
 *
 * The actual tileset data (image, tile size, spacing, margin, name) is
 * NOT duplicated here — it lives in the tileset file itself and is
 * loaded on demand (via TilesetManager) whenever it needs to be shown or
 * edited. This entry only tracks that the project "knows about" this
 * tileset and what tags it carries.
 */
class TILED_EDITOR_EXPORT TilesetRegistryEntry
{
public:
    /**
     * Stable identifier, generated once and never changed.
     */
    QString id;

    /**
     * Path to the tileset file, relative to the project's directory.
     */
    QString fileName;

    QStringList tags;

    TilesetRegistryEntry() = default;
    explicit TilesetRegistryEntry(const QString &fileName);

    /**
     * Adds \a tag, unless it's already present (or empty). Returns
     * whether it was actually added.
     */
    bool addTag(const QString &tag);

    /**
     * Removes \a tag if present. Purely local — does not touch the
     * project's tag registry (see Project::removeTag() for that).
     */
    void removeTag(const QString &tag);

    QJsonObject toJson(const QDir &dir) const;
    static TilesetRegistryEntry fromJson(const QJsonObject &json, const QDir &dir);

    static QString generateId();
};

/**
 * An ordered collection of TilesetRegistryEntry, owned by a Project.
 *
 * Uses std::vector<std::unique_ptr<...>> (rather than QVector) so that
 * pointers handed out by findById()/at() stay valid across insertions
 * and removals elsewhere in the collection, and supports both copy (deep)
 * and move, same as LayerDefinitions.
 */
class TILED_EDITOR_EXPORT TilesetRegistry
{
public:
    TilesetRegistry() = default;
    TilesetRegistry(const TilesetRegistry &other);
    TilesetRegistry &operator=(const TilesetRegistry &other);
    TilesetRegistry(TilesetRegistry &&other) = default;
    TilesetRegistry &operator=(TilesetRegistry &&other) = default;

    int count() const { return static_cast<int>(mEntries.size()); }

    TilesetRegistryEntry *at(int index) { return mEntries.at(size_t(index)).get(); }
    const TilesetRegistryEntry *at(int index) const { return mEntries.at(size_t(index)).get(); }

    int indexOf(const TilesetRegistryEntry *entry) const;

    TilesetRegistryEntry *findById(const QString &id);
    const TilesetRegistryEntry *findById(const QString &id) const;

    TilesetRegistryEntry *findByFileName(const QString &fileName);

    TilesetRegistryEntry *add(TilesetRegistryEntry entry);
    void removeAt(int index);

    void clear() { mEntries.clear(); }

    void loadFromJson(const QJsonArray &array, const QDir &dir);
    QJsonArray toJson(const QDir &dir) const;

private:
    std::vector<std::unique_ptr<TilesetRegistryEntry>> mEntries;
};

} // namespace Tiled
