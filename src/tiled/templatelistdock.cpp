/*
 * templatelistdock.cpp
 * See templatelistdock.h for a description of what this dock does.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "templatelistdock.h"

#include "mapobject.h"
#include "objecttemplate.h"
#include "project.h"
#include "projectmanager.h"
#include "templatemanager.h"
#include "tile.h"
#include "tileset.h"
#include "utils.h"

#include <QDirIterator>
#include <QEvent>
#include <QFileInfo>
#include <QLineEdit>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

using namespace Tiled;

TemplatesListModel::TemplatesListModel(QObject *parent)
    : QStandardItemModel(parent)
{
    // Whenever a watched folder's contents change (a file or subfolder is
    // added/removed - e.g. "Save As Template" writing a new .tx file),
    // rescan everything. refresh() re-registers the watched paths itself,
    // so this stays correct even as folders are added/removed over time.
    connect(&mWatcher, &QFileSystemWatcher::directoryChanged,
            this, &TemplatesListModel::refresh);
}

void TemplatesListModel::refresh()
{
    clear();

    // Stop watching the previous set of folders before rebuilding it below,
    // to avoid "already watching path" warnings and stale watches.
    if (!mWatcher.directories().isEmpty())
        mWatcher.removePaths(mWatcher.directories());

    Project *project = nullptr;
    if (ProjectManager *manager = ProjectManager::instance())
        project = &manager->project();

    if (!project)
        return;

    // Walk every folder configured in the current project and pick up any
    // ".tx" file (Tiled's object template file extension) we can find,
    // recursively, so nested folders (e.g. "templates/enemies/") work too.
    //
    // Every folder visited - whether or not it currently contains a ".tx"
    // file - is also watched, so files created later (by "Save As
    // Template" or otherwise) are picked up automatically without needing
    // to reopen Tiled or manually refresh.
    QStringList watchedFolders;

    for (const QString &folder : project->folders()) {
        if (QFileInfo(folder).isDir())
            watchedFolders << folder;

        QDirIterator it(folder, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo info(path);

            if (info.isDir())
                watchedFolders << path;
            else if (info.fileName().endsWith(QLatin1String(".tx"), Qt::CaseInsensitive))
                addTemplateFile(path);
        }
    }

    watchedFolders.removeDuplicates();
    if (!watchedFolders.isEmpty())
        mWatcher.addPaths(watchedFolders);

    sort(0);
}

void TemplatesListModel::addTemplateFile(const QString &filePath)
{
    // Avoid adding the same file twice (can happen with overlapping folders).
    for (int row = 0; row < rowCount(); ++row) {
        if (item(row)->data(TemplateFilePathRole).toString() == filePath)
            return;
    }

    QString error;
    ObjectTemplate *objectTemplate =
            TemplateManager::instance()->loadObjectTemplate(filePath, &error);

    const QFileInfo fileInfo(filePath);

    QString displayName = fileInfo.completeBaseName();
    QIcon icon;

    if (objectTemplate) {
        if (const MapObject *object = objectTemplate->object()) {
            if (!object->name().isEmpty())
                displayName = object->name();

            // If the template refers to a tile, use it as the icon so the
            // list is easier to scan visually.
            if (Tile *tile = object->cell().tile())
                icon = QIcon(tile->image());
        }
    }

    auto listItem = new QStandardItem(icon, displayName);
    listItem->setData(filePath, TemplateFilePathRole);
    listItem->setToolTip(error.isEmpty() ? filePath
                                          : QStringLiteral("%1\n\n%2").arg(filePath, error));
    listItem->setEditable(false);

    if (!error.isEmpty())
        listItem->setForeground(Qt::red);

    appendRow(listItem);
}

TemplateListDock::TemplateListDock(QWidget *parent)
    : QDockWidget(parent)
    , mModel(new TemplatesListModel(this))
    , mFilterModel(new QSortFilterProxyModel(this))
    , mFilterEdit(new QLineEdit(this))
    , mListView(new QListView(this))
{
    setObjectName(QLatin1String("TemplateListDock"));

    mFilterModel->setSourceModel(mModel);
    mFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    mFilterEdit->setClearButtonEnabled(true);
    connect(mFilterEdit, &QLineEdit::textChanged,
            mFilterModel, &QSortFilterProxyModel::setFilterFixedString);

    mListView->setModel(mFilterModel);
    mListView->setViewMode(QListView::ListMode);   // vertical list, one item per row
    mListView->setFlow(QListView::TopToBottom);
    mListView->setWrapping(false);
    mListView->setResizeMode(QListView::Adjust);
    mListView->setUniformItemSizes(true);
    mListView->setSelectionMode(QAbstractItemView::SingleSelection);
    mListView->setIconSize(Utils::smallIconSize());

    connect(mListView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this] (const QModelIndex &, const QModelIndex &) {
        onSelectionChanged();
    });
    connect(mListView, &QListView::doubleClicked,
            this, &TemplateListDock::onDoubleClicked);

    auto widget = new QWidget(this);
    auto layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mFilterEdit);
    layout->addWidget(mListView);

    setWidget(widget);
    retranslateUi();

    refresh();

    // Keep the list in sync whenever the active project changes.
    if (ProjectManager *manager = ProjectManager::instance()) {
        connect(manager, &ProjectManager::projectChanged,
                this, &TemplateListDock::refresh);
    }
}

TemplateListDock::~TemplateListDock() = default;

void TemplateListDock::refresh()
{
    mModel->refresh();
}

void TemplateListDock::onSelectionChanged()
{
    const QModelIndex proxyIndex = mListView->currentIndex();
    if (!proxyIndex.isValid()) {
        mCurrentTemplate = nullptr;
        emit currentTemplateChanged(nullptr);
        return;
    }

    const QModelIndex sourceIndex = mFilterModel->mapToSource(proxyIndex);
    const QString filePath = mModel->data(sourceIndex, TemplatesListModel::TemplateFilePathRole).toString();

    // Loading is cheap once cached by TemplateManager: if it was already
    // loaded during refresh(), this just returns the cached instance. If the
    // file changed on disk in the meantime this also picks up the update.
    QString error;
    ObjectTemplate *objectTemplate =
            TemplateManager::instance()->loadObjectTemplate(filePath, &error);

    mCurrentTemplate = objectTemplate;
    emit currentTemplateChanged(objectTemplate);
}

void TemplateListDock::onDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index)
    // Selection already updates the current template via currentChanged, so
    // double-click currently just re-confirms the same selection. Hook this
    // up to DocumentManager::instance()->openTemplate(path) here if you also
    // want double-click to open the Template Editor (TemplatesDock).
    onSelectionChanged();
}

void TemplateListDock::changeEvent(QEvent *event)
{
    QDockWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void TemplateListDock::retranslateUi()
{
    setWindowTitle(tr("Templates"));
    mFilterEdit->setPlaceholderText(tr("Filter"));
}

#include "moc_templatelistdock.cpp"
