/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "librarytreewidget.h"

#include "gui/guisettings.h"
#include "gui/trackselectioncontroller.h"
#include "librarytreegroupregistry.h"
#include "librarytreemodel.h"
#include "librarytreeview.h"

#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>

#include <utils/async.h>

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

namespace Fy::Gui::Widgets {
void getLowestIndexes(const QTreeView* treeView, const QModelIndex& index, QModelIndexList& bottomIndexes)
{
    if(!index.isValid()) {
        return getLowestIndexes(treeView, treeView->rootIndex(), bottomIndexes);
    }

    const int rowCount = treeView->model()->rowCount(index);
    if(rowCount == 0) {
        bottomIndexes.append(index);
        return;
    }

    for(int row = 0; row < rowCount; ++row) {
        const QModelIndex childIndex = treeView->model()->index(row, 0, index);
        getLowestIndexes(treeView, childIndex, bottomIndexes);
    }
}

struct LibraryTreeWidget::Private
{
    LibraryTreeWidget* widget;

    Core::Library::MusicLibrary* library;
    LibraryTreeGroupRegistry* groupsRegistry;
    TrackSelectionController* trackSelection;
    Utils::SettingsManager* settings;

    LibraryTreeGrouping grouping;

    QVBoxLayout* layout;
    LibraryTreeView* libraryTree;
    LibraryTreeModel* model;

    TrackAction doubleClickAction;
    TrackAction middleClickAction;

    Private(LibraryTreeWidget* widget, Core::Library::MusicLibrary* library, LibraryTreeGroupRegistry* groupsRegistry,
            TrackSelectionController* trackSelection, Utils::SettingsManager* settings)
        : widget{widget}
        , library{library}
        , groupsRegistry{groupsRegistry}
        , trackSelection{trackSelection}
        , settings{settings}
        , layout{new QVBoxLayout(widget)}
        , libraryTree{new LibraryTreeView(widget)}
        , model{new LibraryTreeModel(widget)}
        , doubleClickAction{static_cast<TrackAction>(settings->value<Settings::LibraryTreeDoubleClick>())}
        , middleClickAction{static_cast<TrackAction>(settings->value<Settings::LibraryTreeMiddleClick>())}
    {
        layout->setContentsMargins(0, 0, 0, 0);

        libraryTree->setUniformRowHeights(true);
        libraryTree->setModel(model);
        libraryTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        libraryTree->setExpandsOnDoubleClick(doubleClickAction == TrackAction::Expand);
        layout->addWidget(libraryTree);

        libraryTree->header()->setContextMenuPolicy(Qt::CustomContextMenu);
        libraryTree->setWordWrap(true);
        libraryTree->setTextElideMode(Qt::ElideRight);

        changeGrouping(groupsRegistry->itemByName(""));

        QObject::connect(libraryTree, &LibraryTreeView::doubleClicked, widget, [this]() {
            handleDoubleClick();
        });
        QObject::connect(libraryTree, &LibraryTreeView::middleMouseClicked, widget, [this]() {
            handleMiddleClicked();
        });

        QObject::connect(libraryTree->selectionModel(), &QItemSelectionModel::selectionChanged, widget, [this]() {
            selectionChanged();
        });

        QObject::connect(libraryTree->header(), &QHeaderView::customContextMenuRequested, widget,
                         [this](const QPoint& pos) {
                             setupHeaderContextMenu(pos);
                         });

        QObject::connect(groupsRegistry, &LibraryTreeGroupRegistry::groupingChanged, widget,
                         [this](const LibraryTreeGrouping& changedGrouping) {
                             if(grouping.id == changedGrouping.id) {
                                 changeGrouping(changedGrouping);
                             }
                         });

        auto treeReset = [this]() {
            reset();
        };

        // TODO: Support row insertion/deletion
        QObject::connect(library, &Core::Library::MusicLibrary::tracksLoaded, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::tracksDeleted, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::tracksAdded, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::tracksSorted, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::libraryRemoved, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::libraryChanged, treeReset);

        settings->subscribe<Settings::LibraryTreeDoubleClick>(widget, [this](int action) {
            doubleClickAction = static_cast<TrackAction>(action);
            libraryTree->setExpandsOnDoubleClick(doubleClickAction == TrackAction::Expand);
        });
        settings->subscribe<Settings::LibraryTreeMiddleClick>(widget, [this](int action) {
            middleClickAction = static_cast<TrackAction>(action);
        });

        reset();
    }

    void reset()
    {
        model->reset(library->tracks());
    }

    void changeGrouping(const LibraryTreeGrouping& newGrouping)
    {
        grouping = newGrouping;
        model->changeGrouping(grouping);
        reset();
    }

    void addGroupMenu(QMenu* parent)
    {
        auto* groupMenu = new QMenu("Grouping", parent);

        const auto& groups = groupsRegistry->items();
        for(const auto& group : groups) {
            auto* switchGroup = new QAction(group.second.name, groupMenu);
            QObject::connect(switchGroup, &QAction::triggered, widget, [this, group]() {
                changeGrouping(group.second);
            });
            groupMenu->addAction(switchGroup);
        }
        parent->addMenu(groupMenu);
    }

    void setupHeaderContextMenu(const QPoint& pos)
    {
        auto* menu = new QMenu(widget);
        addGroupMenu(menu);
        menu->popup(widget->mapToGlobal(pos));
    }

    void selectionChanged()
    {
        const QModelIndexList selectedIndexes = libraryTree->selectionModel()->selectedIndexes();
        if(selectedIndexes.empty()) {
            return;
        }

        QModelIndexList trackIndexes;
        for(const QModelIndex& index : selectedIndexes) {
            getLowestIndexes(libraryTree, index, trackIndexes);
        }

        Core::TrackList tracks;
        for(const auto& index : trackIndexes) {
            const auto indexTracks = index.data(LibraryTreeRole::Tracks).value<Core::TrackList>();
            tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
        }

        const auto sortedTracks = Utils::asyncExec<Core::TrackList>([&tracks]() {
            return Core::Library::Sorting::sortTracks(tracks);
        });
        trackSelection->changeSelectedTracks(sortedTracks, playlistNameFromSelection());

        if(settings->value<Settings::LibraryTreePlaylistEnabled>()) {
            const QString playlistName = settings->value<Settings::LibraryTreeAutoPlaylist>();
            const bool autoSwitch      = settings->value<Settings::LibraryTreeAutoSwitch>();

            trackSelection->executeAction(TrackAction::SendNewPlaylist, autoSwitch ? Switch : None, playlistName);
        }
    }

    QString playlistNameFromSelection() const
    {
        QString title;
        const QModelIndexList selectedIndexes = libraryTree->selectionModel()->selectedIndexes();
        for(const auto& index : selectedIndexes) {
            if(!title.isEmpty()) {
                title.append(", ");
            }
            title.append(index.data().toString());
        }
        return title;
    }

    void handleDoubleClick() const
    {
        const bool autoSwitch = settings->value<Settings::LibraryTreeAutoSwitch>();
        trackSelection->executeAction(doubleClickAction, autoSwitch ? Switch : None, playlistNameFromSelection());
    }

    void handleMiddleClicked() const
    {
        const bool autoSwitch = settings->value<Settings::LibraryTreeAutoSwitch>();
        trackSelection->executeAction(middleClickAction, autoSwitch ? Switch : None, playlistNameFromSelection());
    }
};

LibraryTreeWidget::LibraryTreeWidget(Core::Library::MusicLibrary* library, LibraryTreeGroupRegistry* groupsRegistry,
                                     TrackSelectionController* trackSelection, Utils::SettingsManager* settings,
                                     QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, library, groupsRegistry, trackSelection, settings)}
{
    setObjectName(LibraryTreeWidget::name());
}

QString LibraryTreeWidget::name() const
{
    return "Library Tree";
}

QString LibraryTreeWidget::layoutName() const
{
    return "LibraryTree";
}

void LibraryTreeWidget::saveLayout(QJsonArray& array)
{
    QJsonObject options;
    options["Grouping"] = p->grouping.name;

    QJsonObject tree;
    tree[layoutName()] = options;
    array.append(tree);
}

void LibraryTreeWidget::loadLayout(const QJsonObject& object)
{
    const LibraryTreeGrouping grouping = p->groupsRegistry->itemByName(object["Grouping"].toString());
    if(grouping.isValid()) {
        p->changeGrouping(grouping);
    }
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    p->trackSelection->addTrackPlaylistContextMenu(menu);
    p->addGroupMenu(menu);
    p->trackSelection->addTrackContextMenu(menu);

    menu->popup(mapToGlobal(event->pos()));
}
} // namespace Fy::Gui::Widgets
