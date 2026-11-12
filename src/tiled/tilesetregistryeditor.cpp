/*
 * tilesetregistryeditor.cpp
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

#include "tilesetregistryeditor.h"

#include "documentmanager.h"
#include "layerdefinitioninstantiator.h"
#include "newtilesetdialog.h"
#include "project.h"
#include "projectmanager.h"
#include "tileset.h"
#include "tilesetdocument.h"
#include "tilesetmanager.h"
#include "tilesetview.h"
#include "utils.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace Tiled {

static const int EntryIdRole = Qt::UserRole;

namespace {

/**
 * Minimal event filter that calls back on a double-click. Used to let a
 * read-only QLineEdit act like a label that becomes editable on
 * double-click. (Same helper as used in LayerDefinitionsEditor's tag
 * manager dialog.)
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

/**
 * Same registry-wide tag manager as used by LayerDefinitionsEditor. Kept
 * as a separate copy for now rather than shared, since the two dialogs
 * evolved independently; a follow-up could factor this out.
 */
static void showTagManagerDialog(QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(TilesetRegistryEditor::tr("Manage Tags"));

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
            deleteButton->setText(QStringLiteral("\U0001F5D1"));
            deleteButton->setStyleSheet(QStringLiteral("color: #d9534f; font-size: 11px;"));
            deleteButton->setToolTip(TilesetRegistryEditor::tr("Delete this tag everywhere"));
            deleteButton->setFixedSize(nameEdit->sizeHint().height(), nameEdit->sizeHint().height());
            deleteButton->setAutoRaise(true);
            rowLayout->addWidget(deleteButton);

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
                    QMessageBox::warning(nameEdit, TilesetRegistryEditor::tr("Rename Tag"),
                                         TilesetRegistryEditor::tr("A tag named \"%1\" already exists.")
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
                                TilesetRegistryEditor::tr("Delete Tag"),
                                TilesetRegistryEditor::tr(
                                    "The tag \"%1\" is used by %2 layer definition(s). "
                                    "Deleting it will remove it from all of them (and any "
                                    "tilesets using it). Continue?")
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

TilesetRegistryEditor::TilesetRegistryEditor(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Project Tilesets"));
    resize(Utils::dpiScaled(760), Utils::dpiScaled(480));

    mListWidget = new QListWidget(this);
    mListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *addMenu = new QMenu(this);
    addMenu->addAction(tr("New Tileset\u2026"), this, &TilesetRegistryEditor::addNewTileset);
    addMenu->addAction(tr("Add Existing Tileset\u2026"), this, &TilesetRegistryEditor::addExistingTileset);
    addMenu->addSeparator();
    addMenu->addAction(tr("Scan Project Folders for Tilesets\u2026"), this, &TilesetRegistryEditor::scanProjectFolders);

    mAddButton = new QToolButton(this);
    mAddButton->setText(QStringLiteral("+"));
    mAddButton->setPopupMode(QToolButton::InstantPopup);
    mAddButton->setMenu(addMenu);
    mAddButton->setToolTip(tr("Add a tileset to this project"));

    mRemoveButton = new QToolButton(this);
    mRemoveButton->setText(QStringLiteral("-"));
    mRemoveButton->setToolTip(tr("Remove the selected tileset from this project"));
    mRemoveButton->setEnabled(false);

    const int toolButtonSize = Utils::dpiScaled(24);
    mAddButton->setFixedSize(toolButtonSize, toolButtonSize);
    mRemoveButton->setFixedSize(toolButtonSize, toolButtonSize);

    auto *toolBarLayout = new QHBoxLayout;
    toolBarLayout->addWidget(mAddButton);
    toolBarLayout->addWidget(mRemoveButton);
    toolBarLayout->addStretch();

    auto *leftLayout = new QVBoxLayout;
    leftLayout->addLayout(toolBarLayout);
    leftLayout->addWidget(mListWidget);

    mSourceEdit = new QLineEdit(this);
    mSourceEdit->setReadOnly(true);

    mIdentifierEdit = new QLineEdit(this);
    mIdentifierEdit->setReadOnly(true);

    mLayoutLabel = new QLabel(this);

    mEditButton = new QToolButton(this);
    mEditButton->setText(tr("Edit in Tileset Editor\u2026"));
    mEditButton->setEnabled(false);

    mTagCombo = new QComboBox(this);
    mTagCombo->setEditable(true);
    mTagCombo->setInsertPolicy(QComboBox::NoInsert);
    mTagCombo->lineEdit()->setPlaceholderText(tr("Add a tag\u2026"));

    mManageTagsButton = new QToolButton(this);
    mManageTagsButton->setText(QStringLiteral("\u2699"));
    mManageTagsButton->setToolTip(tr("Manage Tags\u2026"));

    auto *tagAddLayout = new QHBoxLayout;
    tagAddLayout->addWidget(mTagCombo);
    tagAddLayout->addWidget(mManageTagsButton);

    mTagsContainer = new QWidget;
    mTagsLayout = new QVBoxLayout(mTagsContainer);
    mTagsLayout->setContentsMargins(0, 0, 0, 0);
    mTagsLayout->setSpacing(2);

    auto *tagsScrollArea = new QScrollArea(this);
    tagsScrollArea->setWidget(mTagsContainer);
    tagsScrollArea->setWidgetResizable(true);
    tagsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tagsScrollArea->setFrameShape(QFrame::NoFrame);
    tagsScrollArea->setMaximumHeight(Utils::dpiScaled(120));

    auto *tagFieldLayout = new QVBoxLayout;
    tagFieldLayout->addLayout(tagAddLayout);
    tagFieldLayout->addWidget(tagsScrollArea);

    auto *formLayout = new QFormLayout;
    formLayout->addRow(tr("Source image"), mSourceEdit);
    formLayout->addRow(tr("Identifier"), mIdentifierEdit);
    formLayout->addRow(tr("Layout"), mLayoutLabel);
    formLayout->addRow(QString(), mEditButton);
    formLayout->addRow(tr("Tags"), tagFieldLayout);

    mPreviewView = new TilesetView(this);
    mPreviewView->setMinimumHeight(Utils::dpiScaled(160));

    auto *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(formLayout);
    rightLayout->addWidget(mPreviewView, 1);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->addLayout(leftLayout, 1);
    mainLayout->addLayout(rightLayout, 1);

    connect(mListWidget, &QListWidget::currentItemChanged,
            this, &TilesetRegistryEditor::onCurrentItemChanged);

    connect(mAddButton, &QToolButton::clicked, mAddButton, &QToolButton::showMenu);
    connect(mRemoveButton, &QToolButton::clicked,
            this, &TilesetRegistryEditor::removeSelectedEntry);
    connect(mEditButton, &QToolButton::clicked,
            this, &TilesetRegistryEditor::openInTilesetEditor);

    connect(mTagCombo, &QComboBox::activated,
            this, &TilesetRegistryEditor::onTagComboActivated);
    connect(mTagCombo->lineEdit(), &QLineEdit::returnPressed,
            this, &TilesetRegistryEditor::onTagComboActivated);

    connect(mManageTagsButton, &QToolButton::clicked,
            this, &TilesetRegistryEditor::openTagManager);

    connect(ProjectManager::instance(), &ProjectManager::projectChanged, this, [this] {
        refreshTagCombo();
        refresh();
    });

    refreshTagCombo();
    refresh();
}

void TilesetRegistryEditor::refresh()
{
    const QString selectedId = selectedEntry() ? selectedEntry()->id : QString();

    mListWidget->clear();

    Project &project = ProjectManager::instance()->project();
    for (int i = 0; i < project.tilesetRegistry().count(); ++i) {
        TilesetRegistryEntry *entry = project.tilesetRegistry().at(i);

        auto *item = new QListWidgetItem(QFileInfo(entry->fileName).completeBaseName());
        item->setData(EntryIdRole, entry->id);
        mListWidget->addItem(item);
    }

    if (!selectedId.isEmpty()) {
        if (QListWidgetItem *item = findItem(selectedId))
            mListWidget->setCurrentItem(item);
    }
}

QListWidgetItem *TilesetRegistryEditor::findItem(const QString &entryId) const
{
    for (int i = 0; i < mListWidget->count(); ++i) {
        QListWidgetItem *item = mListWidget->item(i);
        if (item->data(EntryIdRole).toString() == entryId)
            return item;
    }
    return nullptr;
}

TilesetRegistryEntry *TilesetRegistryEditor::selectedEntry() const
{
    QListWidgetItem *item = mListWidget->currentItem();
    if (!item)
        return nullptr;

    const QString id = item->data(EntryIdRole).toString();
    return ProjectManager::instance()->project().tilesetRegistry().findById(id);
}

void TilesetRegistryEditor::onCurrentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(current)
    Q_UNUSED(previous)

    mRemoveButton->setEnabled(selectedEntry() != nullptr);
    mTagCombo->setEnabled(selectedEntry() != nullptr);

    updateInfoPanel();
    refreshTagChips();
}

void TilesetRegistryEditor::updateInfoPanel()
{
    TilesetRegistryEntry *entry = selectedEntry();

    mPreviewDocument.reset();
    mPreviewView->setTilesetDocument(nullptr);
    mEditButton->setEnabled(false);

    if (!entry) {
        mSourceEdit->clear();
        mIdentifierEdit->clear();
        mLayoutLabel->clear();
        return;
    }

    QString error;
    SharedTileset tileset = TilesetManager::instance()->loadTileset(entry->fileName, &error);

    if (!tileset) {
        mSourceEdit->setText(entry->fileName);
        mIdentifierEdit->clear();
        mLayoutLabel->setText(tr("Could not load this tileset: %1").arg(error));
        return;
    }

    mSourceEdit->setText(tileset->imageSource().isLocalFile()
                          ? tileset->imageSource().toLocalFile()
                          : tileset->imageSource().toString());
    mIdentifierEdit->setText(tileset->name());
    mLayoutLabel->setText(tr("%1x%2 px tiles, %3 columns, %4 tiles")
                           .arg(tileset->tileWidth())
                           .arg(tileset->tileHeight())
                           .arg(tileset->columnCount())
                           .arg(tileset->tileCount()));

    if (tileset->imageStatus() == LoadingError) {
        mLayoutLabel->setText(mLayoutLabel->text() + QLatin1Char('\n') +
                              tr("Warning: the tileset's image could not be loaded."));
    }

    // Only one TilesetDocument may wrap a given Tileset at a time (this is
    // enforced with an assert), so if this tileset is already open
    // elsewhere (e.g. just created via "New Tileset..." further down, or
    // open in the real Tileset Editor), reuse that document for the
    // preview instead of creating a second, conflicting one.
    if (TilesetDocument *existingDocument = TilesetDocument::findDocumentForTileset(tileset)) {
        mPreviewView->setTilesetDocument(existingDocument);
    } else {
        mPreviewDocument = TilesetDocumentPtr::create(tileset);
        mPreviewView->setTilesetDocument(mPreviewDocument.data());
    }

    mEditButton->setEnabled(true);
}

void TilesetRegistryEditor::refreshTagCombo()
{
    mSynchronizing = true;

    mTagCombo->clear();
    mTagCombo->addItems(ProjectManager::instance()->project().tags());
    mTagCombo->setCurrentIndex(-1);
    mTagCombo->clearEditText();

    mSynchronizing = false;
}

void TilesetRegistryEditor::refreshTagChips()
{
    QLayoutItem *child;
    while ((child = mTagsLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    TilesetRegistryEntry *entry = selectedEntry();
    if (!entry)
        return;

    for (const QString &tag : entry->tags) {
        auto *row = new QWidget(mTagsContainer);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        auto *label = new QLabel(tag, row);
        rowLayout->addWidget(label);
        rowLayout->addStretch();

        auto *removeButton = new QToolButton(row);
        removeButton->setText(QStringLiteral("\u00D7"));
        removeButton->setToolTip(tr("Remove this tag"));
        rowLayout->addWidget(removeButton);

        connect(removeButton, &QToolButton::clicked, this, [this, entry, tag] {
            entry->removeTag(tag);
            saveProject();
            refreshTagChips();
        });

        mTagsLayout->addWidget(row);
    }

    mTagsLayout->addStretch();
}

void TilesetRegistryEditor::onTagComboActivated()
{
    if (mSynchronizing)
        return;

    TilesetRegistryEntry *entry = selectedEntry();
    if (!entry)
        return;

    const QString text = mTagCombo->currentText().trimmed();
    if (text.isEmpty())
        return;

    Project &project = ProjectManager::instance()->project();
    if (!project.tags().contains(text))
        project.addTag(text);

    if (entry->addTag(text)) {
        saveProject();
        refreshTagChips();
    }

    mSynchronizing = true;
    mTagCombo->setCurrentIndex(-1);
    mTagCombo->clearEditText();
    mSynchronizing = false;

    refreshTagCombo();
}

void TilesetRegistryEditor::openTagManager()
{
    showTagManagerDialog(this);

    refreshTagCombo();
    refreshTagChips();
}

void TilesetRegistryEditor::addNewTileset()
{
    NewTilesetDialog newTilesetDialog(this);
    SharedTileset tileset = newTilesetDialog.createTileset();
    if (!tileset)
        return;

    // Project Tilesets entries always reference an external file — there's
    // no map context here to embed into, so we always save-as and open it
    // as a standalone tileset document, same as Tiled's own "New Tileset"
    // action does for external tilesets.
    auto tilesetDocument = TilesetDocumentPtr::create(tileset);
    DocumentManager *documentManager = DocumentManager::instance();
    emit documentManager->documentCreated(tilesetDocument.data());
    if (!documentManager->saveDocumentAs(tilesetDocument.data()))
        return;
    documentManager->addDocument(tilesetDocument);

    if (tileset->fileName().isEmpty())
        return;

    Project &project = ProjectManager::instance()->project();

    if (TilesetRegistryEntry *existing = project.tilesetRegistry().findByFileName(tileset->fileName())) {
        if (QListWidgetItem *item = findItem(existing->id))
            mListWidget->setCurrentItem(item);
        return;
    }

    TilesetRegistryEntry *added = project.tilesetRegistry().add(TilesetRegistryEntry(tileset->fileName()));
    saveProject();
    refresh();

    if (QListWidgetItem *item = findItem(added->id))
        mListWidget->setCurrentItem(item);
}

void TilesetRegistryEditor::scanProjectFolders()
{
    Project &project = ProjectManager::instance()->project();

    QStringList foundFiles;
    for (const QString &folder : project.folders()) {
        QDirIterator it(folder,
                        QStringList { QStringLiteral("*.tsx"), QStringLiteral("*.tsj") },
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            foundFiles.append(QDir::cleanPath(it.next()));
    }

    int addedCount = 0;
    for (const QString &file : std::as_const(foundFiles)) {
        if (!project.tilesetRegistry().findByFileName(file)) {
            project.tilesetRegistry().add(TilesetRegistryEntry(file));
            ++addedCount;
        }
    }

    if (addedCount > 0) {
        saveProject();
        refresh();
    }

    QMessageBox::information(this, tr("Scan Project Folders"),
                             addedCount > 0
                             ? tr("Added %1 tileset(s) found in the project's folders.").arg(addedCount)
                             : tr("No new tilesets found (either none exist, or they're already registered)."));
}

void TilesetRegistryEditor::addExistingTileset()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this, tr("Add Existing Tileset"), QString(),
                tr("Tiled tileset files (*.tsx *.tsj)"));
    if (fileName.isEmpty())
        return;

    Project &project = ProjectManager::instance()->project();

    if (TilesetRegistryEntry *existing = project.tilesetRegistry().findByFileName(fileName)) {
        if (QListWidgetItem *item = findItem(existing->id))
            mListWidget->setCurrentItem(item);
        return;
    }

    QString error;
    SharedTileset tileset = TilesetManager::instance()->loadTileset(fileName, &error);
    if (!tileset) {
        QMessageBox::warning(this, tr("Add Existing Tileset"),
                             tr("Could not load this tileset:\n%1").arg(error));
        return;
    }

    TilesetRegistryEntry *added = project.tilesetRegistry().add(TilesetRegistryEntry(fileName));
    saveProject();
    refresh();

    if (QListWidgetItem *item = findItem(added->id))
        mListWidget->setCurrentItem(item);
}

void TilesetRegistryEditor::removeSelectedEntry()
{
    Project &project = ProjectManager::instance()->project();
    TilesetRegistryEntry *entry = selectedEntry();
    if (!entry)
        return;

    // This only un-registers the tileset from the project; the tileset
    // file itself is never touched or deleted.
    const int index = project.tilesetRegistry().indexOf(entry);
    if (index < 0)
        return;

    project.tilesetRegistry().removeAt(index);

    saveProject();
    refresh();
}

void TilesetRegistryEditor::openInTilesetEditor()
{
    TilesetRegistryEntry *entry = selectedEntry();
    if (!entry)
        return;

    DocumentManager::instance()->openFile(entry->fileName);
}

void TilesetRegistryEditor::closeEvent(QCloseEvent *event)
{
    QDialog::closeEvent(event);
    if (event->isAccepted())
        emit closed();
}

void TilesetRegistryEditor::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    refreshTagCombo();
    refresh();
}

void TilesetRegistryEditor::saveProject()
{
    ProjectManager::instance()->project().save();
}

} // namespace Tiled
