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
#include "gui/playlist/playlistcontroller.h"
#include "librarytreegroupregistry.h"
#include "librarytreemodel.h"
#include "librarytreeview.h"

#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>

#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QJsonObject>
#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

namespace Fy::Gui::Widgets {
constexpr auto AutoPlaylist = "Library Selection";

struct LibraryTreeWidget::Private
{
    LibraryTreeWidget* widget;

    Core::Library::MusicLibrary* library;
    LibraryTreeGroupRegistry* groupsRegistry;
    Playlist::PlaylistController* playlistController;
    Core::Playlist::PlaylistHandler* playlistHandler;
    Utils::SettingsManager* settings;

    LibraryTreeGrouping grouping;

    QVBoxLayout* layout;
    LibraryTreeView* libraryTree;
    LibraryTreeModel* model;

    Core::TrackList selectedTracks;

    ClickAction doubleClickAction;
    ClickAction middleClickAction;

    bool autoplay;
    bool autoSend;

    Private(LibraryTreeWidget* widget, Core::Library::MusicLibrary* library, LibraryTreeGroupRegistry* groupsRegistry,
            Playlist::PlaylistController* playlistController, Utils::SettingsManager* settings)
        : widget{widget}
        , library{library}
        , groupsRegistry{groupsRegistry}
        , playlistController{playlistController}
        , playlistHandler{playlistController->playlistHandler()}
        , settings{settings}
        , layout{new QVBoxLayout(widget)}
        , libraryTree{new LibraryTreeView(widget)}
        , model{new LibraryTreeModel(widget)}
        , doubleClickAction{static_cast<ClickAction>(settings->value<Settings::LibraryTreeDoubleClick>())}
        , middleClickAction{static_cast<ClickAction>(settings->value<Settings::LibraryTreeMiddleClick>())}
        , autoplay{settings->value<Settings::LibraryTreeAutoplay>()}
        , autoSend{settings->value<Settings::LibraryTreeAutoSend>()}
    {
        layout->setContentsMargins(0, 0, 0, 0);

        libraryTree->setUniformRowHeights(true);
        libraryTree->setModel(model);
        libraryTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
        libraryTree->setExpandsOnDoubleClick(doubleClickAction == Expand);
        layout->addWidget(libraryTree);

        libraryTree->header()->setContextMenuPolicy(Qt::CustomContextMenu);
        libraryTree->setWordWrap(true);
        libraryTree->setTextElideMode(Qt::ElideRight);

        changeGrouping(groupsRegistry->groupingByName(""));

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
                         [this](const LibraryTreeGrouping& oldGroup, const LibraryTreeGrouping& newGroup) {
                             if(grouping == oldGroup) {
                                 changeGrouping(newGroup);
                             }
                         });

        auto treeReset = [this]() {
            reset();
        };

        // TODO: Support row insertion/deletion
        QObject::connect(library, &Core::Library::MusicLibrary::tracksLoaded, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::tracksSorted, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::tracksDeleted, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::tracksAdded, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::libraryRemoved, treeReset);
        QObject::connect(library, &Core::Library::MusicLibrary::libraryChanged, treeReset);

        settings->subscribe<Settings::LibraryTreeDoubleClick>(widget, [this](int action) {
            doubleClickAction = static_cast<ClickAction>(action);
            libraryTree->setExpandsOnDoubleClick(doubleClickAction == Expand);
        });
        settings->subscribe<Settings::LibraryTreeMiddleClick>(widget, [this](int action) {
            middleClickAction = static_cast<ClickAction>(action);
        });
        settings->subscribe<Settings::LibraryTreeAutoplay>(widget, [this](bool checked) {
            autoplay = checked;
        });
        settings->subscribe<Settings::LibraryTreeAutoSend>(widget, [this](bool checked) {
            autoSend = checked;
        });

        reset();
    }

    void reset()
    {
        model->reload(library->tracks());
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

        const auto& groups = groupsRegistry->groupings();
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

        selectedTracks.clear();
        for(const auto& index : selectedIndexes) {
            const auto indexTracks = index.data(LibraryTreeRole::Tracks).value<Core::TrackList>();
            selectedTracks.insert(selectedTracks.end(), indexTracks.cbegin(), indexTracks.cend());
        }
        if(autoSend) {
            sendToPlaylist(selectedTracks, false);
        }
    }

    void sendToPlaylist(const Core::TrackList& tracks, bool current)
    {
        std::optional<Core::Playlist::Playlist> playlist;

        if(current) {
            if(auto currentPlaylist = playlistController->currentPlaylist()) {
                playlist = playlistHandler->createPlaylist(currentPlaylist->name(), tracks);
            }
        }
        else {
            playlist = playlistHandler->createPlaylist(AutoPlaylist, tracks);
        }

        if(playlist && autoplay) {
            playlistHandler->startPlayback(playlist->name());
        }
    }

    void addToPlaylist(const Core::TrackList& tracks)
    {
        if(auto playlist = playlistController->currentPlaylist()) {
            playlistHandler->appendToPlaylist(playlist->id(), tracks);
        }
    }

    void handleDoubleClick()
    {
        switch(doubleClickAction) {
            case(SendCurrentPlaylist):
                return sendToPlaylist(selectedTracks, true);
            case(SendNewPlaylist):
                return sendToPlaylist(selectedTracks, false);
            case(AddCurrentPlaylist):
                return addToPlaylist(selectedTracks);
            case(Expand):
            case(None):
                break;
        }
    }

    void handleMiddleClicked()
    {
        switch(middleClickAction) {
            case(SendCurrentPlaylist):
                return sendToPlaylist(selectedTracks, true);
            case(SendNewPlaylist):
                return sendToPlaylist(selectedTracks, false);
            case(AddCurrentPlaylist):
                return addToPlaylist(selectedTracks);
            case(Expand):
            case(None):
                break;
        }
    }
};

LibraryTreeWidget::LibraryTreeWidget(Core::Library::MusicLibrary* library, LibraryTreeGroupRegistry* groupsRegistry,
                                     Playlist::PlaylistController* playlistController, Utils::SettingsManager* settings,
                                     QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, library, groupsRegistry, playlistController, settings)}
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
    const LibraryTreeGrouping grouping = p->groupsRegistry->groupingByName(object["Grouping"].toString());
    if(grouping.isValid()) {
        p->changeGrouping(grouping);
    }
}

void LibraryTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QModelIndexList selected = p->libraryTree->selectionModel()->selection().indexes();
    if(selected.empty()) {
        return;
    }

    QStringList playlistName;
    Core::TrackList tracks;
    for(const auto& index : selected) {
        playlistName.emplace_back(index.data().toString());
        const auto indexTracks = index.data(LibraryTreeRole::Tracks).value<Core::TrackList>();
        tracks.insert(tracks.end(), indexTracks.cbegin(), indexTracks.cend());
    }

    auto* createPlaylist = new QAction("Send to playlist", menu);
    QObject::connect(createPlaylist, &QAction::triggered, this, [this, playlistName, tracks]() {
        p->playlistHandler->createPlaylist(playlistName.join(", "), tracks);
    });
    menu->addAction(createPlaylist);

    auto* addToPlaylist = new QAction("Add to current playlist", menu);
    QObject::connect(addToPlaylist, &QAction::triggered, this, [this, tracks]() {
        p->playlistHandler->appendToPlaylist(p->playlistController->currentPlaylist()->id(), tracks);
    });
    menu->addAction(addToPlaylist);

    menu->addSeparator();
    p->addGroupMenu(menu);

    menu->popup(mapToGlobal(event->pos()));
}
} // namespace Fy::Gui::Widgets
