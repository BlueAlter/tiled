/*
 * layerdefinitionseditor.cpp
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

#include "layerdefinitionseditor.h"

#include "layerdefinitioninstantiator.h"
#include "project.h"
#include "projectmanager.h"
#include "utils.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>

namespace Tiled {

static const int DefinitionIdRole = Qt::UserRole;

namespace {

/**
 * Minimal event filter that calls back on a double-click. Used to let a
 * read-only QLineEdit act like a label that becomes editable on
 * double-click, without needing a dedicated widget subclass.
 */
class DoubleClickFilter : public QObject
{
public:
    DoubleClickFilter(std::function<void()> callback, QObject *parent)
        : QObject(parent)
        , mCallback(std::move(callback))
    {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonDblClick) {
            mCallback();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> mCallback;
};

} // anonymous namespace

static QString typeDisplayName(LayerDefinition::Type type)
{
    switch (type) {
    case LayerDefinition::TileLayerDef:   return LayerDefinitionsEditor::tr("Tile Layer");
    case LayerDefinition::ObjectGroupDef: return LayerDefinitionsEditor::tr("Object Layer");
    case LayerDefinition::ImageLayerDef:  return LayerDefinitionsEditor::tr("Image Layer");
    case LayerDefinition::GroupLayerDef:  return LayerDefinitionsEditor::tr("Group Layer");
    }
    return QString();
}

/**
 * Small dialog for renaming/removing tags from the project's global tag
 * registry (as opposed to just un-assigning a tag from one layer, which
 * is handled locally by the chip list in the main editor).
 *
 * Removing a tag here is registry-wide: it's a destructive action that
 * un-assigns the tag from every LayerDefinition that uses it, so the
 * user is asked to confirm when the tag is currently in use anywhere.
 *
 * Each row is a fully custom widget (QLineEdit + delete button) rather
 * than a QListWidgetItem relying on Qt's built-in double-click-to-edit,
 * since setItemWidget() (needed to embed the delete button) conflicts
 * with that built-in editing and made renaming unreliable.
 */
static void showTagManagerDialog(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(LayerDefinitionsEditor::tr("Manage Tags"));

    auto *listContainer = new QWidget;
    auto *listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(2);

    auto *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidget(listContainer);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);

    std::function<void()> reloadList;

    reloadList = [listLayout, scrollArea, &dialog, &reloadList] {
        // Clears everything, including the trailing stretch (re-added below).
        QLayoutItem *child;
        while ((child = listLayout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }

        Project &project = ProjectManager::instance()->project();

        for (const QString &tag : project.tags()) {
            auto *row = new QWidget;
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);

            auto *nameEdit = new QLineEdit(tag, row);
            nameEdit->setReadOnly(true);
            nameEdit->setFrame(false);
            rowLayout->addWidget(nameEdit);

            auto *deleteButton = new QToolButton(row);
            deleteButton->setText(QStringLiteral("\U0001F5D1"));   // wastebasket glyph
            deleteButton->setStyleSheet(QStringLiteral("color: #d9534f; font-size: 11px;"));
            deleteButton->setToolTip(LayerDefinitionsEditor::tr("Delete this tag everywhere"));
            deleteButton->setFixedSize(nameEdit->sizeHint().height(), nameEdit->sizeHint().height());
            deleteButton->setAutoRaise(true);
            rowLayout->addWidget(deleteButton);

            // Double-click the name to rename it in place.
            auto *doubleClickFilter = new DoubleClickFilter([nameEdit] {
                nameEdit->setReadOnly(false);
                nameEdit->selectAll();
                nameEdit->setFocus();
            }, nameEdit);
            nameEdit->installEventFilter(doubleClickFilter);
            nameEdit->setProperty("tagName", tag);
            QObject::connect(nameEdit, &QLineEdit::editingFinished, &dialog, [nameEdit, &reloadList] {
                if (nameEdit->isReadOnly())
                    return;

                const QString oldName = nameEdit->property("tagName").toString();
                const QString newName = nameEdit->text().trimmed();

                nameEdit->setReadOnly(true);

                if (newName.isEmpty() || newName == oldName)
                    return;

                Project &project = ProjectManager::instance()->project();
                if (project.renameTag(oldName, newName)) {
                    project.save();
                    LayerDefinitionInstantiator::syncAllOpenDocuments(project);
                } else {
                    QMessageBox::warning(nameEdit, LayerDefinitionsEditor::tr("Rename Tag"),
                                         LayerDefinitionsEditor::tr("A tag named \"%1\" already exists.")
                                             .arg(newName));
                }

                reloadList();
            });

            QObject::connect(deleteButton, &QToolButton::clicked, &dialog, [tag, &dialog, &reloadList] {
                Project &project = ProjectManager::instance()->project();
                const int usageCount = project.tagUsageCount(tag);

                if (usageCount > 0) {
                    const auto reply = QMessageBox::warning(
                                &dialog,
                                LayerDefinitionsEditor::tr("Delete Tag"),
                                LayerDefinitionsEditor::tr(
                                    "The tag \"%1\" is used by %2 layer definition(s). "
                                    "Deleting it will remove it from all of them. Continue?")
                                    .arg(tag).arg(usageCount),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);
                    if (reply != QMessageBox::Yes)
                        return;
                }

                project.removeTag(tag);
                project.save();
                LayerDefinitionInstantiator::syncAllOpenDocuments(project);

                reloadList();
            });

            listLayout->addWidget(row);
        }

        listLayout->addStretch();
        scrollArea->verticalScrollBar()->setValue(0);
    };
    reloadList();

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(scrollArea);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    dialog.resize(Utils::dpiScaled(320), Utils::dpiScaled(360));
    dialog.exec();
}

LayerDefinitionsEditor::LayerDefinitionsEditor(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Project Layers"));
    resize(Utils::dpiScaled(680), Utils::dpiScaled(420));

    mTreeWidget = new QTreeWidget(this);
    mTreeWidget->setHeaderHidden(true);
    mTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    mTreeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    mTreeWidget->setDefaultDropAction(Qt::MoveAction);

    auto *addMenu = new QMenu(this);
    addMenu->addAction(tr("Tile Layer"), this, [this] { addDefinition(LayerDefinition::TileLayerDef); });
    addMenu->addAction(tr("Object Layer"), this, [this] { addDefinition(LayerDefinition::ObjectGroupDef); });
    addMenu->addAction(tr("Image Layer"), this, [this] { addDefinition(LayerDefinition::ImageLayerDef); });
    addMenu->addAction(tr("Group Layer"), this, [this] { addDefinition(LayerDefinition::GroupLayerDef); });

    mAddButton = new QToolButton(this);
    mAddButton->setText(QStringLiteral("+"));
    mAddButton->setPopupMode(QToolButton::InstantPopup);
    mAddButton->setMenu(addMenu);
    mAddButton->setToolTip(tr("Add a new layer definition"));

    mRemoveButton = new QToolButton(this);
    mRemoveButton->setText(QStringLiteral("-"));
    mRemoveButton->setToolTip(tr("Remove the selected layer definition"));
    mRemoveButton->setEnabled(false);

    mMoveUpButton = new QToolButton(this);
    mMoveUpButton->setText(QStringLiteral("\u25B2"));
    mMoveUpButton->setToolTip(tr("Move up"));
    mMoveUpButton->setEnabled(false);

    mMoveDownButton = new QToolButton(this);
    mMoveDownButton->setText(QStringLiteral("\u25BC"));
    mMoveDownButton->setToolTip(tr("Move down"));
    mMoveDownButton->setEnabled(false);

    // Give all four toolbar buttons the same footprint. mAddButton would
    // otherwise render wider than the others because Qt draws an extra
    // dropdown-arrow indicator for buttons that have a menu attached.
    const int toolButtonSize = Utils::dpiScaled(24);
    for (QToolButton *button : { mAddButton, mRemoveButton, mMoveUpButton, mMoveDownButton })
        button->setFixedSize(toolButtonSize, toolButtonSize);

    auto *toolBarLayout = new QHBoxLayout;
    toolBarLayout->addWidget(mAddButton);
    toolBarLayout->addWidget(mRemoveButton);
    toolBarLayout->addStretch();
    toolBarLayout->addWidget(mMoveUpButton);
    toolBarLayout->addWidget(mMoveDownButton);

    auto *leftLayout = new QVBoxLayout;
    leftLayout->addLayout(toolBarLayout);
    leftLayout->addWidget(mTreeWidget);

    mNameEdit = new QLineEdit(this);
    mTypeCombo = new QComboBox(this);
    mTypeCombo->addItem(typeDisplayName(LayerDefinition::TileLayerDef));
    mTypeCombo->addItem(typeDisplayName(LayerDefinition::ObjectGroupDef));
    mTypeCombo->addItem(typeDisplayName(LayerDefinition::ImageLayerDef));
    mTypeCombo->addItem(typeDisplayName(LayerDefinition::GroupLayerDef));
    mTypeCombo->setEnabled(false);   // type can't be changed after creation

    // "Add a tag" combo: always starts empty/blank, used only to pick an
    // existing tag or type a new one to ADD to the current definition.
    // It never shows/represents "the" tag, since there can be several.
    mTagCombo = new QComboBox(this);
    mTagCombo->setEditable(true);
    mTagCombo->setInsertPolicy(QComboBox::NoInsert);
    mTagCombo->lineEdit()->setPlaceholderText(tr("Add a tag\u2026"));

    mManageTagsButton = new QToolButton(this);
    mManageTagsButton->setText(QStringLiteral("\u2699"));   // gear glyph
    mManageTagsButton->setToolTip(tr("Manage Tags\u2026"));

    auto *tagAddLayout = new QHBoxLayout;
    tagAddLayout->addWidget(mTagCombo);
    tagAddLayout->addWidget(mManageTagsButton);

    // Chip list of tags assigned to the current definition, one row per
    // tag, each with its own small remove ("x") button. This is a purely
    // local un-assignment — it never touches the tag registry itself.
    mTagsContainer = new QWidget;
    mTagsLayout = new QVBoxLayout(mTagsContainer);
    mTagsLayout->setContentsMargins(0, 0, 0, 0);
    mTagsLayout->setSpacing(2);

    auto *tagsScrollArea = new QScrollArea(this);
    tagsScrollArea->setWidget(mTagsContainer);
    tagsScrollArea->setWidgetResizable(true);
    tagsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tagsScrollArea->setFrameShape(QFrame::NoFrame);

    auto *tagFieldLayout = new QVBoxLayout;
    tagFieldLayout->addLayout(tagAddLayout);
    tagFieldLayout->addWidget(tagsScrollArea);

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("Name"), mNameEdit);
    formLayout->addRow(tr("Type"), mTypeCombo);
    formLayout->addRow(tr("Tags"), tagFieldLayout);

    auto *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(formLayout);
    rightLayout->addStretch();

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 1);

    connect(mTreeWidget, &QTreeWidget::currentItemChanged,
            this, &LayerDefinitionsEditor::onCurrentItemChanged);
    connect(mTreeWidget->model(), &QAbstractItemModel::rowsMoved,
            this, &LayerDefinitionsEditor::onRowsMoved);

    connect(mAddButton, &QToolButton::clicked, mAddButton, &QToolButton::showMenu);
    connect(mRemoveButton, &QToolButton::clicked,
            this, &LayerDefinitionsEditor::removeSelectedDefinition);
    connect(mMoveUpButton, &QToolButton::clicked,
            this, [this] { moveSelectedDefinition(-1); });
    connect(mMoveDownButton, &QToolButton::clicked,
            this, [this] { moveSelectedDefinition(1); });

    connect(mNameEdit, &QLineEdit::editingFinished,
            this, &LayerDefinitionsEditor::onNameEdited);

    // Fires both when picking an existing tag from the dropdown and when
    // typing a new one and pressing Enter (activated() covers both).
    connect(mTagCombo, &QComboBox::activated,
            this, &LayerDefinitionsEditor::onTagComboActivated);
    connect(mTagCombo->lineEdit(), &QLineEdit::returnPressed,
            this, &LayerDefinitionsEditor::onTagComboActivated);

    connect(mManageTagsButton, &QToolButton::clicked,
            this, &LayerDefinitionsEditor::openTagManager);

    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, [this] {
        refreshTagCombo();
        refresh();
    });

    refreshTagCombo();
    refresh();
}

void LayerDefinitionsEditor::refresh()
{
    const QString selectedId = selectedDefinition() ? selectedDefinition()->id : QString();

    mTreeWidget->clear();
    populateChildren(nullptr, QString());
    mTreeWidget->expandAll();

    if (!selectedId.isEmpty()) {
        if (QTreeWidgetItem *item = findItem(selectedId))
            mTreeWidget->setCurrentItem(item);
    }
}

void LayerDefinitionsEditor::populateChildren(QTreeWidgetItem *parentItem, const QString &parentId)
{
    Project &project = ProjectManager::instance()->project();
    const auto children = project.layerDefinitions().childrenOf(parentId);

    for (const LayerDefinition *definition : children) {
        auto *item = new QTreeWidgetItem;
        item->setText(0, definition->name);
        item->setData(0, DefinitionIdRole, definition->id);

        Qt::ItemFlags itemFlags = item->flags() | Qt::ItemIsDragEnabled;
        if (definition->isGroup())
            itemFlags |= Qt::ItemIsDropEnabled;
        else
            itemFlags &= ~Qt::ItemIsDropEnabled;
        item->setFlags(itemFlags);

        if (parentItem)
            parentItem->addChild(item);
        else
            mTreeWidget->addTopLevelItem(item);

        if (definition->isGroup())
            populateChildren(item, definition->id);
    }
}

QTreeWidgetItem *LayerDefinitionsEditor::findItem(const QString &definitionId) const
{
    const auto items = mTreeWidget->findItems(QString(), Qt::MatchContains | Qt::MatchRecursive);
    for (QTreeWidgetItem *item : items) {
        if (item->data(0, DefinitionIdRole).toString() == definitionId)
            return item;
    }
    return nullptr;
}

LayerDefinition *LayerDefinitionsEditor::selectedDefinition() const
{
    QTreeWidgetItem *item = mTreeWidget->currentItem();
    if (!item)
        return nullptr;

    const QString id = item->data(0, DefinitionIdRole).toString();
    return ProjectManager::instance()->project().layerDefinitions().findById(id);
}

void LayerDefinitionsEditor::refreshTagCombo()
{
    mSynchronizing = true;

    mTagCombo->clear();
    mTagCombo->addItems(ProjectManager::instance()->project().tags());
    mTagCombo->setCurrentIndex(-1);
    mTagCombo->clearEditText();

    mSynchronizing = false;
}

void LayerDefinitionsEditor::refreshTagChips()
{
    // Clear any existing chip rows.
    QLayoutItem *child;
    while ((child = mTagsLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    LayerDefinition *definition = selectedDefinition();
    if (!definition)
        return;

    for (const QString &tag : definition->tags) {
        auto *row = new QWidget(mTagsContainer);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        auto *label = new QLabel(tag, row);
        rowLayout->addWidget(label);
        rowLayout->addStretch();

        auto *removeButton = new QToolButton(row);
        removeButton->setText(QStringLiteral("\u00D7"));   // multiplication sign, used as "x"
        removeButton->setToolTip(tr("Remove this tag from \"%1\"").arg(definition->name));
        rowLayout->addWidget(removeButton);

        // Purely local un-assignment: only touches this one definition,
        // never the project-wide tag registry.
        connect(removeButton, &QToolButton::clicked, this, [this, definition, tag] {
            definition->removeTag(tag);
            saveProject();
            LayerDefinitionInstantiator::syncAllOpenDocuments(ProjectManager::instance()->project());
            refreshTagChips();
        });

        mTagsLayout->addWidget(row);
    }

    mTagsLayout->addStretch();
}

void LayerDefinitionsEditor::onCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    Q_UNUSED(previous)

    mSynchronizing = true;

    LayerDefinition *definition = selectedDefinition();

    mRemoveButton->setEnabled(definition != nullptr);
    mMoveUpButton->setEnabled(definition != nullptr);
    mMoveDownButton->setEnabled(definition != nullptr);
    mTagCombo->setEnabled(definition != nullptr);

    if (definition) {
        mNameEdit->setEnabled(true);
        mNameEdit->setText(definition->name);
        mTypeCombo->setCurrentText(typeDisplayName(definition->type));
    } else {
        mNameEdit->setEnabled(false);
        mNameEdit->clear();
    }

    mTagCombo->setCurrentIndex(-1);
    mTagCombo->clearEditText();

    mSynchronizing = false;

    refreshTagChips();

    Q_UNUSED(current)
}

void LayerDefinitionsEditor::onNameEdited()
{
    if (mSynchronizing)
        return;

    if (LayerDefinition *definition = selectedDefinition()) {
        definition->name = mNameEdit->text();
        if (QTreeWidgetItem *item = mTreeWidget->currentItem())
            item->setText(0, definition->name);
        saveProject();
        LayerDefinitionInstantiator::syncAllOpenDocuments(ProjectManager::instance()->project());
    }
}

void LayerDefinitionsEditor::onTagComboActivated()
{
    if (mSynchronizing)
        return;

    LayerDefinition *definition = selectedDefinition();
    if (!definition)
        return;

    const QString text = mTagCombo->currentText().trimmed();
    if (text.isEmpty())
        return;

    // Typing a brand new tag and confirming it (Enter) registers it in
    // the project's tag registry right away, so it becomes available to
    // every other layer definition immediately. Picking an existing tag
    // from the dropdown just reuses it. Either way, addTag() below
    // refuses to create a duplicate within this one definition.
    Project &project = ProjectManager::instance()->project();
    if (!project.tags().contains(text))
        project.addTag(text);

    if (definition->addTag(text)) {
        saveProject();
        LayerDefinitionInstantiator::syncAllOpenDocuments(project);
        refreshTagChips();
    }

    mSynchronizing = true;
    mTagCombo->setCurrentIndex(-1);
    mTagCombo->clearEditText();
    mSynchronizing = false;

    refreshTagCombo();
}

void LayerDefinitionsEditor::openTagManager()
{
    showTagManagerDialog(this);

    refreshTagCombo();
    refreshTagChips();
}

void LayerDefinitionsEditor::onRowsMoved()
{
    if (mSynchronizing)
        return;

    Project &project = ProjectManager::instance()->project();

    // Recompute parentId/order for every definition based on the tree's
    // final shape, rather than trying to patch just the moved item — this
    // is simpler and self-correcting, at the cost of always rewriting
    // every definition's order (harmless, since order is only meaningful
    // relative to siblings).
    std::function<void(QTreeWidgetItem *, const QString &)> walk =
            [&] (QTreeWidgetItem *parentItem, const QString &parentId) {
        const int count = parentItem ? parentItem->childCount() : mTreeWidget->topLevelItemCount();

        for (int i = 0; i < count; ++i) {
            QTreeWidgetItem *item = parentItem ? parentItem->child(i) : mTreeWidget->topLevelItem(i);
            const QString id = item->data(0, DefinitionIdRole).toString();

            if (LayerDefinition *definition = project.layerDefinitions().findById(id)) {
                definition->parentId = parentId;
                definition->order = i;
            }

            walk(item, id);
        }
    };

    walk(nullptr, QString());

    saveProject();
    LayerDefinitionInstantiator::syncAllOpenDocuments(project);

    // The tree's own item objects are still valid and correctly nested
    // after an internal move, so a full refresh() isn't needed here.
}

void LayerDefinitionsEditor::addDefinition(LayerDefinition::Type type)
{
    Project &project = ProjectManager::instance()->project();

    // New definitions are added as a child of the currently selected
    // group, or at the top level otherwise.
    QString parentId;
    if (LayerDefinition *current = selectedDefinition()) {
        if (current->isGroup())
            parentId = current->id;
        else
            parentId = current->parentId;
    }

    const int siblingCount = project.layerDefinitions().childrenOf(parentId).size();

    LayerDefinition definition(tr("New Layer"), type);
    definition.parentId = parentId;
    definition.order = siblingCount;

    LayerDefinition *added = project.layerDefinitions().add(std::move(definition));

    saveProject();
    LayerDefinitionInstantiator::syncAllOpenDocuments(project);
    refresh();

    if (QTreeWidgetItem *item = findItem(added->id))
        mTreeWidget->setCurrentItem(item);
}

void LayerDefinitionsEditor::removeSelectedDefinition()
{
    Project &project = ProjectManager::instance()->project();
    LayerDefinition *definition = selectedDefinition();
    if (!definition)
        return;

    if (definition->isGroup() && !project.layerDefinitions().childrenOf(definition->id).isEmpty()) {
        QMessageBox::warning(this, tr("Remove Layer"),
                             tr("This group still contains other layer definitions. "
                                "Remove or move those out first."));
        return;
    }

    const int index = project.layerDefinitions().indexOf(definition);
    if (index < 0)
        return;

    project.layerDefinitions().removeAt(index);

    saveProject();
    refresh();
}

void LayerDefinitionsEditor::moveSelectedDefinition(int direction)
{
    Project &project = ProjectManager::instance()->project();
    LayerDefinition *definition = selectedDefinition();
    if (!definition)
        return;

    auto siblings = project.layerDefinitions().childrenOf(definition->parentId);

    const int currentIndex = siblings.indexOf(definition);
    const int swapIndex = currentIndex + direction;

    if (currentIndex < 0 || swapIndex < 0 || swapIndex >= siblings.size())
        return;

    // Sibling order values are kept as consecutive integers starting at
    // 0, so swapping the order of two neighbors is enough to swap their
    // visual position.
    std::swap(siblings[currentIndex]->order, siblings[swapIndex]->order);

    saveProject();
    refresh();
}

void LayerDefinitionsEditor::closeEvent(QCloseEvent *event)
{
    QDialog::closeEvent(event);
    if (event->isAccepted())
        emit closed();
}

void LayerDefinitionsEditor::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    refreshTagCombo();
    refresh();
}

void LayerDefinitionsEditor::saveProject()
{
    ProjectManager::instance()->project().save();
}

} // namespace Tiled
