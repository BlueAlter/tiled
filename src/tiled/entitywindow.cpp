/*
 * entitywindow.cpp
 * See entitywindow.h for a description of what this window does.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "entitywindow.h"

#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "objecttemplate.h"
#include "projectmanager.h"
#include "propertieswidget.h"
#include "templatelistdock.h"
#include "templatemanager.h"
#include "tileset.h"

#include <QCloseEvent>
#include <QEvent>
#include <QLineEdit>
#include <QListView>
#include <QSplitter>
#include <QUndoStack>
#include <QVBoxLayout>

using namespace Tiled;

EntityWindow::EntityWindow(QWidget *parent)
    : QDialog(parent)
    , mTemplatesModel(new TemplatesListModel(this))
{
    setObjectName(QLatin1String("EntityWindow"));
    resize(900, 520);

    mFilterEdit = new QLineEdit(this);
    mFilterEdit->setClearButtonEnabled(true);

    mEntityListView = new QListView(this);
    mEntityListView->setModel(mTemplatesModel);
    mEntityListView->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(mFilterEdit, &QLineEdit::textChanged,
            this, [this] (const QString &text) {
        for (int row = 0; row < mTemplatesModel->rowCount(); ++row) {
            const bool matches = mTemplatesModel->item(row)->text()
                    .contains(text, Qt::CaseInsensitive);
            mEntityListView->setRowHidden(row, !matches);
        }
    });

    connect(mEntityListView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this] (const QModelIndex &, const QModelIndex &) {
        selectionChanged();
    });

    // The actual editing surface: the same generic properties editor used
    // everywhere else in Tiled (Name, Class, Geometry, Color, Custom
    // Properties, ...), bound to a small dummy document further down.
    mPropertiesWidget = new PropertiesWidget(this);

    auto leftWidget = new QWidget(this);
    auto leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(mFilterEdit);
    leftLayout->addWidget(mEntityListView);

    mSplitter = new QSplitter(this);
    mSplitter->addWidget(leftWidget);
    mSplitter->addWidget(mPropertiesWidget);
    mSplitter->setStretchFactor(0, 1);
    mSplitter->setStretchFactor(1, 2);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(mSplitter);

    retranslateUi();
    refresh();

    // Keep the entity list in sync with the currently open project.
    if (ProjectManager *manager = ProjectManager::instance()) {
        connect(manager, &ProjectManager::projectChanged,
                this, &EntityWindow::refresh);
    }
}

EntityWindow::~EntityWindow() = default;

void EntityWindow::refresh()
{
    mTemplatesModel->refresh();
}

MapObject *EntityWindow::dummyObject() const
{
    if (!mDummyMapDocument)
        return nullptr;

    auto objectGroup = static_cast<ObjectGroup*>(mDummyMapDocument->map()->layerAt(0));
    return objectGroup->objects().isEmpty() ? nullptr : objectGroup->objects().constFirst();
}

void EntityWindow::selectionChanged()
{
    // Detach the properties widget from the previous dummy document before
    // replacing it, so the old document is properly released.
    mPropertiesWidget->setDocument(nullptr);
    mDummyMapDocument.reset();
    mCurrentObjectTemplate = nullptr;

    const QModelIndex index = mEntityListView->currentIndex();
    if (!index.isValid())
        return;

    const QString filePath = mTemplatesModel->data(index, TemplatesListModel::TemplateFilePathRole).toString();

    QString error;
    ObjectTemplate *objectTemplate =
            TemplateManager::instance()->loadObjectTemplate(filePath, &error);

    if (!objectTemplate || !objectTemplate->object())
        return;

    mCurrentObjectTemplate = objectTemplate;

    // Build a tiny one-object "dummy" map to host the template's object,
    // exactly like Tiled's own Template Editor does internally. This lets us
    // reuse the generic PropertiesWidget for editing, with full undo/redo.
    Map::Parameters mapParameters;
    mapParameters.width = 1;
    mapParameters.height = 1;
    mapParameters.tileWidth = 1;
    mapParameters.tileHeight = 1;

    auto map = std::make_unique<Map>(mapParameters);

    MapObject *dummy = objectTemplate->object()->clone();
    dummy->markAsTemplateBase();

    if (Tileset *tileset = dummy->cell().tileset())
        map->addTileset(tileset->sharedFromThis());

    auto objectGroup = new ObjectGroup;
    objectGroup->addObject(dummy);
    map->addLayer(objectGroup);

    mDummyMapDocument = MapDocumentPtr::create(std::move(map));
    mDummyMapDocument->setAllowHidingObjects(false);
    mDummyMapDocument->switchCurrentLayer(objectGroup);
    mDummyMapDocument->setCurrentObject(dummy);

    connect(mDummyMapDocument->undoStack(), &QUndoStack::indexChanged,
            this, &EntityWindow::applyChanges);

    mPropertiesWidget->setDocument(mDummyMapDocument.data());
}

void EntityWindow::applyChanges()
{
    if (!mCurrentObjectTemplate)
        return;

    MapObject *object = dummyObject();
    if (!object)
        return;

    mCurrentObjectTemplate->setObject(object);
    mCurrentObjectTemplate->save();

    emit TemplateManager::instance()->objectTemplateChanged(mCurrentObjectTemplate);
}

void EntityWindow::closeEvent(QCloseEvent *event)
{
    QDialog::closeEvent(event);
    if (event->isAccepted())
        emit closed();
}

void EntityWindow::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void EntityWindow::retranslateUi()
{
    setWindowTitle(tr("Entities"));
    mFilterEdit->setPlaceholderText(tr("Filter"));
}

#include "moc_entitywindow.cpp"
