/*
 * entitywindow.h
 * A standalone window (not a dock) for browsing and quickly editing "Entity"
 * definitions, inspired by LDtk's "Project Entities" window.
 *
 * Stage 2 (revised): an "Entity" is simply an Object Template (.tx file) -
 * its icon and size come from the template's tile object, and its
 * schema/fields come from the object's Class and custom properties.
 *
 * Rather than opening Tiled's separate Template Editor dock, this window
 * embeds a PropertiesWidget directly in its right-hand panel, bound to a
 * small "dummy" MapDocument that wraps a clone of the selected template's
 * object - the exact same technique Tiled's own TemplatesDock uses
 * internally. Edits are written straight back to the .tx file on disk.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#pragma once

#include "mapdocument.h"

#include <QDialog>

class QListView;
class QSplitter;
class QLineEdit;

namespace Tiled {

class TemplatesListModel;
class PropertiesWidget;
class ObjectTemplate;

/**
 * Standalone "Entity Window", listing every Object Template in the current
 * project. Selecting one loads it into an embedded PropertiesWidget for
 * quick editing (name, class, size, color, custom properties, ...),
 * without needing to open the separate Template Editor dock.
 */
class EntityWindow : public QDialog
{
    Q_OBJECT

public:
    explicit EntityWindow(QWidget *parent = nullptr);
    ~EntityWindow() override;

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void refresh();
    void selectionChanged();
    void applyChanges();
    MapObject *dummyObject() const;
    void retranslateUi();

    TemplatesListModel *mTemplatesModel;
    ObjectTemplate *mCurrentObjectTemplate = nullptr;
    MapDocumentPtr mDummyMapDocument;

    QSplitter *mSplitter;
    QLineEdit *mFilterEdit;
    QListView *mEntityListView;
    PropertiesWidget *mPropertiesWidget;
};

} // namespace Tiled
