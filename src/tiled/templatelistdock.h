/*
 * templatelistdock.h
 * Custom dock that lists every object template (.tx file) found in the
 * current project's folders as a vertical list. Selecting an entry sets it
 * as the "current object template", the same state used by CreateTemplateTool
 * (the "Insert Template" tool) to know which template to stamp onto the map.
 *
 * This file is written to sit alongside Tiled's existing sources in
 * src/tiled/, and reuses Tiled's own classes: Tiled::ObjectTemplate,
 * Tiled::TemplateManager and Tiled::ProjectManager.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#pragma once

#include <QDockWidget>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QStandardItemModel>

class QListView;
class QLineEdit;
class QSortFilterProxyModel;

namespace Tiled {

class ObjectTemplate;

/**
 * Model holding one row per discovered object template file.
 *
 * Item data:
 *  - Qt::DisplayRole      -> template display name (object name or file base name)
 *  - Qt::ToolTipRole      -> full path of the .tx file
 *  - Qt::DecorationRole   -> a small preview icon, when available
 *  - TemplateFilePathRole -> absolute file path (used to (re)load the template)
 */
class TemplatesListModel : public QStandardItemModel
{
    Q_OBJECT

public:
    enum Roles {
        TemplateFilePathRole = Qt::UserRole + 1
    };

    explicit TemplatesListModel(QObject *parent = nullptr);

    /**
     * Rescans the current project's folders for ".tx" template files and
     * rebuilds the list. Safe to call repeatedly (e.g. after the project
     * folders change).
     *
     * Also (re)configures an internal QFileSystemWatcher covering every
     * folder scanned, so newly created ".tx" files (e.g. from "Save As
     * Template") are picked up automatically without needing to reopen
     * Tiled or manually refresh.
     */
    void refresh();

private:
    void addTemplateFile(const QString &filePath);

    QFileSystemWatcher mWatcher;
};

/**
 * Dock widget showing a vertical list of every object template available in
 * the currently open project. Selecting an item makes that template the
 * "current" one, ready to be placed on the map with the Insert Template tool.
 */
class TemplateListDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit TemplateListDock(QWidget *parent = nullptr);
    ~TemplateListDock() override;

public slots:
    /**
     * Rebuilds the list of templates from the current project's folders.
     */
    void refresh();

signals:
    /**
     * Emitted whenever the user selects a different template in the list.
     * Connect this to Tiled::ToolManager::setObjectTemplate() so that tools
     * such as CreateTemplateTool pick it up, the same way TilesetDock and
     * TemplatesDock already do for tiles.
     */
    void currentTemplateChanged(Tiled::ObjectTemplate *objectTemplate);

protected:
    void changeEvent(QEvent *event) override;

private:
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex &index);
    void retranslateUi();

    TemplatesListModel *mModel;
    QSortFilterProxyModel *mFilterModel;
    QLineEdit *mFilterEdit;
    QListView *mListView;

    Tiled::ObjectTemplate *mCurrentTemplate = nullptr;
};

} // namespace Tiled
