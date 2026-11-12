/*
 * tilesetregistry.cpp
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

#include "tilesetregistry.h"

#include <QJsonArray>
#include <QRandomGenerator>

namespace Tiled {

TilesetRegistryEntry::TilesetRegistryEntry(const QString &fileName)
    : id(generateId())
    , fileName(fileName)
{
}

bool TilesetRegistryEntry::addTag(const QString &tag)
{
    const QString trimmed = tag.trimmed();
    if (trimmed.isEmpty() || tags.contains(trimmed))
        return false;

    tags.append(trimmed);
    return true;
}

void TilesetRegistryEntry::removeTag(const QString &tag)
{
    tags.removeOne(tag);
}

QJsonObject TilesetRegistryEntry::toJson(const QDir &dir) const
{
    QJsonObject json {
        { QStringLiteral("id"), id },
        { QStringLiteral("fileName"), dir.relativeFilePath(fileName) },
    };

    if (!tags.isEmpty())
        json[QStringLiteral("tags")] = QJsonArray::fromStringList(tags);

    return json;
}

TilesetRegistryEntry TilesetRegistryEntry::fromJson(const QJsonObject &json, const QDir &dir)
{
    TilesetRegistryEntry entry;

    entry.id = json.value(QLatin1String("id")).toString();
    if (entry.id.isEmpty())
        entry.id = generateId();

    entry.fileName = QDir::cleanPath(dir.absoluteFilePath(json.value(QLatin1String("fileName")).toString()));

    for (const QJsonValue &value : json.value(QLatin1String("tags")).toArray())
        entry.addTag(value.toString());

    return entry;
}

QString TilesetRegistryEntry::generateId()
{
    return QStringLiteral("%1-%2")
            .arg(QRandomGenerator::global()->generate(), 0, 16)
            .arg(QRandomGenerator::global()->generate(), 0, 16);
}

TilesetRegistry::TilesetRegistry(const TilesetRegistry &other)
{
    mEntries.reserve(other.mEntries.size());
    for (const auto &entry : other.mEntries)
        mEntries.push_back(std::make_unique<TilesetRegistryEntry>(*entry));
}

TilesetRegistry &TilesetRegistry::operator=(const TilesetRegistry &other)
{
    if (this == &other)
        return *this;

    std::vector<std::unique_ptr<TilesetRegistryEntry>> copy;
    copy.reserve(other.mEntries.size());
    for (const auto &entry : other.mEntries)
        copy.push_back(std::make_unique<TilesetRegistryEntry>(*entry));

    mEntries = std::move(copy);
    return *this;
}

int TilesetRegistry::indexOf(const TilesetRegistryEntry *entry) const
{
    for (size_t i = 0; i < mEntries.size(); ++i)
        if (mEntries[i].get() == entry)
            return static_cast<int>(i);
    return -1;
}

TilesetRegistryEntry *TilesetRegistry::findById(const QString &id)
{
    for (auto &entry : mEntries)
        if (entry->id == id)
            return entry.get();
    return nullptr;
}

const TilesetRegistryEntry *TilesetRegistry::findById(const QString &id) const
{
    for (auto &entry : mEntries)
        if (entry->id == id)
            return entry.get();
    return nullptr;
}

TilesetRegistryEntry *TilesetRegistry::findByFileName(const QString &fileName)
{
    for (auto &entry : mEntries)
        if (entry->fileName == fileName)
            return entry.get();
    return nullptr;
}

TilesetRegistryEntry *TilesetRegistry::add(TilesetRegistryEntry entry)
{
    if (entry.id.isEmpty())
        entry.id = TilesetRegistryEntry::generateId();

    mEntries.push_back(std::make_unique<TilesetRegistryEntry>(std::move(entry)));
    return mEntries.back().get();
}

void TilesetRegistry::removeAt(int index)
{
    Q_ASSERT(index >= 0 && size_t(index) < mEntries.size());
    mEntries.erase(mEntries.begin() + index);
}

void TilesetRegistry::loadFromJson(const QJsonArray &array, const QDir &dir)
{
    mEntries.clear();
    mEntries.reserve(size_t(array.size()));

    for (const QJsonValue &value : array) {
        mEntries.push_back(std::make_unique<TilesetRegistryEntry>(
                                TilesetRegistryEntry::fromJson(value.toObject(), dir)));
    }
}

QJsonArray TilesetRegistry::toJson(const QDir &dir) const
{
    QJsonArray array;
    for (const auto &entry : mEntries)
        array.append(entry->toJson(dir));
    return array;
}

} // namespace Tiled
