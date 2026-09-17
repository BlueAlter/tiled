/*
 * project.cpp
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

#include "project.h"
#include "properties.h"
#include "savefile.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Tiled {

static QString relative(const QDir &dir, const QString &fileName)
{
    QString rel = dir.relativeFilePath(fileName);
    return rel.isEmpty() ? QStringLiteral(".") : rel;
}

static QString absolute(const QDir &dir, const QString &fileName)
{
    if (fileName.isEmpty())
        return QString();

    return QDir::cleanPath(dir.absoluteFilePath(fileName));
}

Project::Project()
    : Object(Object::ProjectType)
    , mPropertyTypes(SharedPropertyTypes::create())
{
}

bool Project::save()
{
    if (!mFileName.isEmpty())
        return save(mFileName);
    return false;
}

bool Project::save(const QString &fileName)
{
    QString extensionsPath = mExtensionsPath;

    // Initialize extensions path to its default value
    if (mFileName.isEmpty() && extensionsPath.isEmpty())
        extensionsPath = QFileInfo(fileName).dir().filePath(QLatin1String("extensions"));

    const QDir dir = QFileInfo(fileName).dir();

    QJsonArray folders;
    for (auto &folder : std::as_const(mFolders))
        folders.append(relative(dir, folder));

    QJsonArray commands;
    for (const Command &command : std::as_const(mCommands))
        commands.append(QJsonObject::fromVariantHash(command.toVariant()));

    const QJsonArray propertyTypes = mPropertyTypes->toJson(dir.path());
    ExportContext context(*mPropertyTypes, dir.path());
    context.setRecursiveBehavior(ExportContext::RecursiveBehavior::TypedListValues);
    const QJsonArray projectProperties = propertiesToJson(properties(), context);
    const QJsonArray layerDefinitions = mLayerDefinitions.toJson(context);
    const QJsonArray tags = QJsonArray::fromStringList(mTags);
    QJsonObject project {
        { QStringLiteral("propertyTypes"), propertyTypes },
        { QStringLiteral("folders"), folders },
        { QStringLiteral("extensionsPath"), relative(dir, extensionsPath) },
        { QStringLiteral("automappingRulesFile"), dir.relativeFilePath(mAutomappingRulesFile) },
        { QStringLiteral("commands"), commands },
        { QStringLiteral("properties"),  projectProperties },
        { QStringLiteral("layerDefinitions"), layerDefinitions },
        { QStringLiteral("tags"), tags },
    };

    if (mCompatibilityVersion != Tiled_Latest)
        project[QStringLiteral("compatibilityVersion")] = mCompatibilityVersion;

    const QJsonDocument document(project);

    SaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.device()->write(document.toJson());
    if (!file.commit())
        return false;

    mLastSaved = QFileInfo(fileName).lastModified();
    mFileName = fileName;
    mExtensionsPath = extensionsPath;
    return true;
}

std::unique_ptr<Project> Project::load(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return nullptr;

    QJsonParseError error;
    const QByteArray json = file.readAll();
    const QJsonDocument document(QJsonDocument::fromJson(json, &error));
    if (error.error != QJsonParseError::NoError)
        return nullptr;

    auto project = std::make_unique<Project>();
    project->mFileName = fileName;

    const QDir dir = QFileInfo(fileName).dir();
    const QJsonObject projectJson = document.object();

    project->mExtensionsPath = absolute(dir, projectJson.value(QLatin1String("extensionsPath")).toString(QStringLiteral("extensions")));
    project->mObjectTypesFile = absolute(dir, projectJson.value(QLatin1String("objectTypesFile")).toString());
    project->mAutomappingRulesFile = absolute(dir, projectJson.value(QLatin1String("automappingRulesFile")).toString());

    project->mPropertyTypes->loadFromJson(projectJson.value(QLatin1String("propertyTypes")).toArray(), dir.path());

    const QString projectPropertiesKey = QLatin1String("properties");
    if (projectJson.contains(projectPropertiesKey)) {
        ExportContext context(*project->mPropertyTypes, dir.path());
        context.setRecursiveBehavior(ExportContext::RecursiveBehavior::TypedListValues);
        const Properties loadedProperties = propertiesFromJson(projectJson.value(projectPropertiesKey).toArray(), context);
        project->setProperties(loadedProperties);
    }

    if (projectJson.contains(QLatin1String("layerDefinitions"))) {
        ExportContext context(*project->mPropertyTypes, dir.path());
        context.setRecursiveBehavior(ExportContext::RecursiveBehavior::TypedListValues);
        project->mLayerDefinitions.loadFromJson(projectJson.value(QLatin1String("layerDefinitions")).toArray(), context);
    }

    for (const QJsonValue &value : projectJson.value(QLatin1String("tags")).toArray())
        project->mTags.append(value.toString());

    const QJsonArray folders = projectJson.value(QLatin1String("folders")).toArray();
    for (const QJsonValue &folderValue : folders)
        project->mFolders.append(QDir::cleanPath(dir.absoluteFilePath(folderValue.toString())));

    const QJsonArray commands = projectJson.value(QLatin1String("commands")).toArray();
    for (const QJsonValue &commandValue : commands)
        project->mCommands.append(Command::fromVariant(commandValue.toVariant()));

    project->mCompatibilityVersion = static_cast<CompatibilityVersion>(projectJson.value(QLatin1String("compatibilityVersion")).toInt(Tiled_Latest));

    return project;
}

void Project::addFolder(const QString &folder)
{
    mFolders.append(folder);
}

void Project::removeFolder(int index)
{
    Q_ASSERT(index >= 0 && index < mFolders.size());
    mFolders.removeAt(index);
}

bool Project::addTag(const QString &tag)
{
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty() || mTags.contains(trimmed))
        return false;

    mTags.append(trimmed);
    return true;
}

bool Project::renameTag(const QString &oldName, const QString &newName)
{
    const QString trimmed = newName.trimmed();
    if (trimmed.isEmpty() || oldName == trimmed)
        return false;

    const int index = mTags.indexOf(oldName);
    if (index < 0)
        return false;

    // Don't allow renaming onto an already-existing tag; that would
    // silently merge two tags together, which the user should do
    // explicitly (remove the duplicate first) rather than by accident.
    if (mTags.contains(trimmed))
        return false;

    mTags[index] = trimmed;

    for (int i = 0; i < mLayerDefinitions.count(); ++i) {
        LayerDefinition *definition = mLayerDefinitions.at(i);
        const int tagIndex = definition->tags.indexOf(oldName);
        if (tagIndex < 0)
            continue;

        // Guard against a layer somehow already having both names
        // assigned (shouldn't normally happen, but would otherwise
        // produce a duplicate in that one definition's tag list).
        if (definition->tags.contains(trimmed))
            definition->tags.removeAt(tagIndex);
        else
            definition->tags[tagIndex] = trimmed;
    }

    return true;
}

void Project::removeTag(const QString &tag)
{
    mTags.removeAll(tag);

    for (int i = 0; i < mLayerDefinitions.count(); ++i)
        mLayerDefinitions.at(i)->tags.removeAll(tag);
}

int Project::tagUsageCount(const QString &tag) const
{
    int count = 0;
    for (int i = 0; i < mLayerDefinitions.count(); ++i)
        if (mLayerDefinitions.at(i)->tags.contains(tag))
            ++count;
    return count;
}

} // namespace Tiled
