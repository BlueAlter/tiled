/*
 * layerdefinitionseditor.h
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

#include "layerdefinition.h"

#include <QDialog>

class QComboBox;
class QCloseEvent;
class QLineEdit;
class QShowEvent;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QWidget;

namespace Tiled {

/**
 * Non-modal dialog for managing a Project's LayerDefinitions: the layers
 * that get automatically instantiated (and protected from deletion) on
 * every Map belonging to the project.
 *
 * Mirrors the direct-save behavior of PropertyTypesEditor: edits are
 * applied straight to ProjectManager::instance()->project() and the
 * project file is saved right away (no undo stack involved, consistent
 * with how project-level settings work elsewhere in Tiled).
 */
class LayerDefinitionsEditor : public QDialog
{
    Q_OBJECT

public:
    explicit LayerDefinitionsEditor(QWidget *parent = nullptr);

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void addDefinition(LayerDefinition::Type type);
    void removeSelectedDefinition();
    void moveSelectedDefinition(int direction);   // -1 = up, +1 = down

    void refresh();
    void populateChildren(QTreeWidgetItem *parentItem, const QString &parentId);
    void onRowsMoved();

    void onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void onNameEdited();
    void onTagComboActivated();     // adds the entered/selected tag to the current definition
    void openTagManager();
    void refreshTagCombo();         // repopulates the "add a tag" dropdown from the registry
    void refreshTagChips();         // rebuilds the assigned-tags chip list for the current definition

    LayerDefinition *selectedDefinition() const;
    QTreeWidgetItem *findItem(const QString &definitionId) const;

    void saveProject();

    QTreeWidget *mTreeWidget;
    QToolButton *mAddButton;
    QToolButton *mRemoveButton;
    QToolButton *mMoveUpButton;
    QToolButton *mMoveDownButton;

    QLineEdit *mNameEdit;
    QComboBox *mTypeCombo;
    QComboBox *mTagCombo;
    QToolButton *mManageTagsButton;
    QWidget *mTagsContainer;        // holds one row per tag assigned to the current definition
    QVBoxLayout *mTagsLayout;

    bool mSynchronizing = false;
};

} // namespace Tiled
