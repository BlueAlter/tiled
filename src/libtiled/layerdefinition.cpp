/*
 * layerdefinition.cpp
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

#include "layerdefinition.h"

#include <QJsonArray>
#include <QRandomGenerator>

#include <algorithm>

namespace Tiled {

LayerDefinition::LayerDefinition(const QString &name, Type type)
    : id(generateId())
    , name(name)
    , type(type)
{
}

bool LayerDefinition::addTag(const QString &tag)
{
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty() || tags.contains(trimmed))
        return false;

    tags.append(trimmed);
    return true;
}

void LayerDefinition::removeTag(const QString &tag)
{
    tags.removeOne(tag);
}

QJsonObject LayerDefinition::toJson(const ExportContext &context) const
{
    QJsonObject json {
        { QStringLiteral("id"), id },
        { QStringLiteral("name"), name },
        { QStringLiteral("type"), typeToString(type) },
        { QStringLiteral("visible"), visibleByDefault },
        { QStringLiteral("order"), order },
    };

    if (!tags.isEmpty())
        json[QStringLiteral("tags")] = QJsonArray::fromStringList(tags);

    if (!parentId.isEmpty())
        json[QStringLiteral("parent")] = parentId;

    if (!defaultProperties.isEmpty())
        json[QStringLiteral("properties")] = propertiesToJson(defaultProperties, context);

    return json;
}

LayerDefinition LayerDefinition::fromJson(const QJsonObject &json, const ExportContext &context)
{
    LayerDefinition definition;

    definition.id = json.value(QLatin1String("id")).toString();
    if (definition.id.isEmpty())
        definition.id = generateId();

    definition.name = json.value(QLatin1String("name")).toString();
    definition.type = typeFromString(json.value(QLatin1String("type")).toString());
    definition.parentId = json.value(QLatin1String("parent")).toString();
    definition.order = json.value(QLatin1String("order")).toInt();
    definition.visibleByDefault = json.value(QLatin1String("visible")).toBool(true);
    definition.defaultProperties = propertiesFromJson(json.value(QLatin1String("properties")).toArray(), context);

    if (json.contains(QLatin1String("tags"))) {
        for (const QJsonValue &value : json.value(QLatin1String("tags")).toArray())
            definition.addTag(value.toString());
    } else if (json.contains(QLatin1String("tag"))) {
        // Backward compatibility with the earlier single-tag format.
        definition.addTag(json.value(QLatin1String("tag")).toString());
    }

    return definition;
}

LayerDefinition::Type LayerDefinition::typeFromString(const QString &string)
{
    if (string == QLatin1String("tilelayer"))
        return TileLayerDef;
    if (string == QLatin1String("imagelayer"))
        return ImageLayerDef;
    if (string == QLatin1String("group"))
        return GroupLayerDef;
    return ObjectGroupDef;
}

QString LayerDefinition::typeToString(Type type)
{
    switch (type) {
    case TileLayerDef:   return QStringLiteral("tilelayer");
    case ObjectGroupDef: return QStringLiteral("objectgroup");
    case ImageLayerDef:  return QStringLiteral("imagelayer");
    case GroupLayerDef:  return QStringLiteral("group");
    }
    return QStringLiteral("objectgroup");
}

QString LayerDefinition::generateId()
{
    // Simple, dependency-free unique id generator. Good enough since ids
    // only need to be unique within a single project file.
    return QStringLiteral("%1-%2")
            .arg(QRandomGenerator::global()->generate(), 0, 16)
            .arg(QRandomGenerator::global()->generate(), 0, 16);
}

LayerDefinitions::LayerDefinitions(const LayerDefinitions &other)
{
    mDefinitions.reserve(other.mDefinitions.size());
    for (const auto &definition : other.mDefinitions)
        mDefinitions.push_back(std::make_unique<LayerDefinition>(*definition));
}

LayerDefinitions &LayerDefinitions::operator=(const LayerDefinitions &other)
{
    if (this == &other)
        return *this;

    std::vector<std::unique_ptr<LayerDefinition>> copy;
    copy.reserve(other.mDefinitions.size());
    for (const auto &definition : other.mDefinitions)
        copy.push_back(std::make_unique<LayerDefinition>(*definition));

    mDefinitions = std::move(copy);
    return *this;
}

int LayerDefinitions::indexOf(const LayerDefinition *definition) const
{
    for (size_t i = 0; i < mDefinitions.size(); ++i)
        if (mDefinitions[i].get() == definition)
            return static_cast<int>(i);
    return -1;
}

LayerDefinition *LayerDefinitions::findById(const QString &id)
{
    for (auto &definition : mDefinitions)
        if (definition->id == id)
            return definition.get();
    return nullptr;
}

const LayerDefinition *LayerDefinitions::findById(const QString &id) const
{
    for (auto &definition : mDefinitions)
        if (definition->id == id)
            return definition.get();
    return nullptr;
}

QVector<LayerDefinition *> LayerDefinitions::childrenOf(const QString &parentId)
{
    QVector<LayerDefinition *> children;

    for (auto &definition : mDefinitions)
        if (definition->parentId == parentId)
            children.append(definition.get());

    std::sort(children.begin(), children.end(),
              [](const LayerDefinition *a, const LayerDefinition *b) {
        return a->order < b->order;
    });

    return children;
}

QVector<const LayerDefinition *> LayerDefinitions::childrenOf(const QString &parentId) const
{
    QVector<const LayerDefinition *> children;

    for (auto &definition : mDefinitions)
        if (definition->parentId == parentId)
            children.append(definition.get());

    std::sort(children.begin(), children.end(),
              [](const LayerDefinition *a, const LayerDefinition *b) {
        return a->order < b->order;
    });

    return children;
}

LayerDefinition *LayerDefinitions::add(LayerDefinition definition)
{
    if (definition.id.isEmpty())
        definition.id = LayerDefinition::generateId();

    mDefinitions.push_back(std::make_unique<LayerDefinition>(std::move(definition)));
    return mDefinitions.back().get();
}

void LayerDefinitions::removeAt(int index)
{
    Q_ASSERT(index >= 0 && size_t(index) < mDefinitions.size());
    mDefinitions.erase(mDefinitions.begin() + index);
}

void LayerDefinitions::moveDefinition(int from, int to)
{
    Q_ASSERT(from >= 0 && size_t(from) < mDefinitions.size());
    Q_ASSERT(to >= 0 && size_t(to) < mDefinitions.size());

    if (from == to)
        return;

    auto item = std::move(mDefinitions[size_t(from)]);
    mDefinitions.erase(mDefinitions.begin() + from);
    mDefinitions.insert(mDefinitions.begin() + to, std::move(item));
}

void LayerDefinitions::loadFromJson(const QJsonArray &array, const ExportContext &context)
{
    mDefinitions.clear();
    mDefinitions.reserve(size_t(array.size()));

    for (const QJsonValue &value : array) {
        mDefinitions.push_back(std::make_unique<LayerDefinition>(
                                    LayerDefinition::fromJson(value.toObject(), context)));
    }
}

QJsonArray LayerDefinitions::toJson(const ExportContext &context) const
{
    QJsonArray array;
    for (const auto &definition : mDefinitions)
        array.append(definition->toJson(context));
    return array;
}

} // namespace Tiled
