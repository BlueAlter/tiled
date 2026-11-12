/*
 * tilesetregistryeditor.h
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

#include "tilesetregistry.h"

#include <QDialog>
#include <QSharedPointer>

class QComboBox;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QShowEvent;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace Tiled {

class TilesetDocument;
class TilesetView;

/**
 * Non-modal dialog listing the tilesets registered with the current
 * Project ("Project Tilesets"). Mirrors LayerDefinitionsEditor's
 * direct-save behavior and its tag chip UI, applied to tilesets instead
 * of layers.
 *
 * The tileset's own data (image, tile size, spacing, margin, name) is
 * shown read-only here (loaded via TilesetManager) and previewed with
 * the same TilesetView widget the real Tileset Editor uses. Actually
 * editing that data happens in the real Tileset Editor, opened via the
 * "Edit in Tileset Editor" button — this dialog does not duplicate that
 * editing UI, only registration and tagging.
 */
class TilesetRegistryEditor : public QDialog
{
    Q_OBJECT

public:
    explicit TilesetRegistryEditor(QWidget *parent = nullptr);

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void addNewTileset();
    void addExistingTileset();
    void scanProjectFolders();
    void removeSelectedEntry();
    void openInTilesetEditor();

    void refresh();
    void onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void updateInfoPanel();

    void onTagComboActivated();
    void openTagManager();
    void refreshTagCombo();
    void refreshTagChips();

    TilesetRegistryEntry *selectedEntry() const;
    QListWidgetItem *findItem(const QString &entryId) const;

    void saveProject();

    QListWidget *mListWidget;
    QToolButton *mAddButton;
    QToolButton *mRemoveButton;

    QLineEdit *mSourceEdit;
    QLineEdit *mIdentifierEdit;
    QLabel *mLayoutLabel;
    QToolButton *mEditButton;

    QComboBox *mTagCombo;
    QToolButton *mManageTagsButton;
    QWidget *mTagsContainer;
    QVBoxLayout *mTagsLayout;

    TilesetView *mPreviewView;
    QSharedPointer<TilesetDocument> mPreviewDocument;   // keeps the previewed tileset alive

    bool mSynchronizing = false;
};

} // namespace Tiled
