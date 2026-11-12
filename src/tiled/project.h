/*
 * project.h
 * Copyright 2019, Thorbjørn Lindeijer <bjorn@lindeijer.nl>
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

#include "command.h"
#include "layerdefinition.h"
#include "object.h"
#include "tiled.h"
#include "tilededitor_global.h"
#include "tilesetregistry.h"

#include <QDateTime>
#include <QStringList>
#include <QVector>

#include <memory>

namespace Tiled {

class TILED_EDITOR_EXPORT Project : public Object
{
public:
    Project();

    const QString &fileName() const;
    bool save();
    bool save(const QString &fileName);

    static std::unique_ptr<Project> load(const QString &fileName);

    void addFolder(const QString &folder);
    void removeFolder(int index);
    const QStringList &folders() const;

    const QDateTime &lastSaved() const;
    const SharedPropertyTypes &propertyTypes() const;

    LayerDefinitions &layerDefinitions() { return mLayerDefinitions; }
    const LayerDefinitions &layerDefinitions() const { return mLayerDefinitions; }

    TilesetRegistry &tilesetRegistry() { return mTilesetRegistry; }
    const TilesetRegistry &tilesetRegistry() const { return mTilesetRegistry; }

    /**
     * Global registry of tags available for use by LayerDefinitions (and,
     * later, other things like tilesets). Keeping this centralized means
     * a tag typed in one place is reliably the same tag used elsewhere,
     * and renaming/removing it here updates everything that refers to it.
     */
    const QStringList &tags() const { return mTags; }
    bool addTag(const QString &tag);
    bool renameTag(const QString &oldName, const QString &newName);
    void removeTag(const QString &tag);
    int tagUsageCount(const QString &tag) const;

    QString mExtensionsPath;
    QString mObjectTypesFile;
    QString mAutomappingRulesFile;
    QVector<Command> mCommands;
    CompatibilityVersion mCompatibilityVersion = Tiled_Current;

private:
    QDateTime mLastSaved;
    QString mFileName;
    QStringList mFolders;
    SharedPropertyTypes mPropertyTypes;
    LayerDefinitions mLayerDefinitions;
    TilesetRegistry mTilesetRegistry;
    QStringList mTags;
};


inline const QString &Project::fileName() const
{
    return mFileName;
}

inline const QStringList &Project::folders() const
{
    return mFolders;
}

inline const QDateTime &Project::lastSaved() const
{
    return mLastSaved;
}

inline const SharedPropertyTypes &Project::propertyTypes() const
{
    return mPropertyTypes;
}

} // namespace Tiled
