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

#include "playlistwidget.h"

#include "gui/guisettings.h"
#include "gui/trackselectioncontroller.h"
#include "playlistcontroller.h"
#include "playlistdelegate.h"
#include "playlistmodel.h"
#include "playlistview.h"
#include "presetregistry.h"

#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/library/sortingregistry.h>
#include <core/library/tracksort.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>

#include <utils/actions/actioncontainer.h>
#include <utils/headerview.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistWidget::Private : QObject
{
    PlaylistWidget* widget;
    Core::Player::PlayerManager* playerManager;
    TrackSelectionController* selectionController;
    Utils::SettingsManager* settings;
    Utils::SettingsDialogController* settingsDialog;

    PlaylistController* controller;

    QHBoxLayout* layout;
    PlaylistModel* model;
    PlaylistView* playlistView;
    Utils::HeaderView* header;
    bool changingSelection{false};

    PlaylistPreset currentPreset;

    Private(PlaylistWidget* widget, Core::Player::PlayerManager* playerManager, PlaylistController* playlistController,
            TrackSelectionController* selectionController, Utils::SettingsManager* settings)
        : widget{widget}
        , playerManager{playerManager}
        , selectionController{selectionController}
        , settings{settings}
        , settingsDialog{settings->settingsDialog()}
        , controller{playlistController}
        , layout{new QHBoxLayout(widget)}
        , model{new PlaylistModel(playerManager, settings, widget)}
        , playlistView{new PlaylistView(widget)}
        , header{new Utils::HeaderView(Qt::Horizontal, widget)}
    {
        layout->setContentsMargins(0, 0, 0, 0);

        header->setStretchLastSection(true);
        playlistView->setHeader(header);
        header->setContextMenuPolicy(Qt::CustomContextMenu);

        playlistView->setModel(model);
        playlistView->setItemDelegate(new PlaylistDelegate(widget));

        layout->addWidget(playlistView);

        setHeaderHidden(settings->value<Settings::PlaylistHeader>());
        setScrollbarHidden(settings->value<Settings::PlaylistScrollBar>());

        connect(playlistView->header(), &QHeaderView::customContextMenuRequested, this,
                &PlaylistWidget::Private::customHeaderMenuRequested);
        connect(playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                &PlaylistWidget::Private::selectionChanged);

        connect(playlistView, &PlaylistView::playlistChanged, this, &PlaylistWidget::Private::playlistTracksChanged);
        connect(playlistView, &QAbstractItemView::doubleClicked, this, &PlaylistWidget::Private::doubleClicked);
        connect(playerManager, &Core::Player::PlayerManager::playStateChanged, this,
                &PlaylistWidget::Private::changeState);

        connect(model, &QAbstractItemModel::rowsInserted, this, &PlaylistWidget::Private::expandPlaylist);
        connect(header, &Utils::HeaderView::leftClicked, this, &PlaylistWidget::Private::switchContextMenu);

        connect(playerManager, &Core::Player::PlayerManager::currentTrackChanged, model,
                &PlaylistModel::changeTrackState);

        connect(model, &QAbstractItemModel::modelReset, this, &PlaylistWidget::Private::reset);
        connect(model, &QAbstractItemModel::modelAboutToBeReset, playlistView, &QAbstractItemView::clearSelection);

        connect(playlistController, &PlaylistController::currentPlaylistChanged, this,
                &PlaylistWidget::Private::changePlaylist);
        connect(playlistController, &PlaylistController::refreshPlaylist, this,
                &PlaylistWidget::Private::changePlaylist);

        //    connect(p->library, &Core::Library::MusicLibrary::tracksSorted, p->model, &PlaylistModel::reset);
        //    connect(p->library, &Core::Library::MusicLibrary::tracksDeleted, p->model, &PlaylistModel::reset);
        //    connect(p->library, &Core::Library::MusicLibrary::tracksUpdated, p->model, &PlaylistModel::reset);
        //    connect(p->library, &Core::Library::MusicLibrary::tracksAdded, p->model, &PlaylistModel::setupModelData);
        //    connect(p->library, &Core::Library::MusicLibrary::libraryRemoved, p->model, &PlaylistModel::reset);
        //    connect(p->library, &Core::Library::MusicLibrary::libraryChanged, p->model, &PlaylistModel::reset);

        QObject::connect(controller->presetRegistry(), &PresetRegistry::presetChanged, this,
                         &PlaylistWidget::Private::onPresetChanged);

        settings->subscribe<Settings::PlaylistHeader>(this, &PlaylistWidget::Private::setHeaderHidden);
        settings->subscribe<Settings::PlaylistScrollBar>(this, &PlaylistWidget::Private::setScrollbarHidden);

        settings->subscribe<Settings::CurrentPreset>(this, [this](const QString& presetName) {
            const auto preset = controller->presetRegistry()->itemByName(presetName);
            changePreset(preset);
        });

        changePreset(controller->presetRegistry()->itemByName(settings->value<Settings::CurrentPreset>()));
    }

    void onPresetChanged(const PlaylistPreset& preset)
    {
        if(currentPreset.id == preset.id) {
            changePreset(preset);
        }
    }

    void changePreset(const PlaylistPreset& preset)
    {
        currentPreset = preset;
        model->changePreset(currentPreset);
        if(auto playlist = controller->currentPlaylist()) {
            model->reset(*playlist);
        }
    }

    void changePlaylist(const Core::Playlist::Playlist& playlist) const
    {
        model->reset(playlist);
    }

    void reset() const
    {
        playlistView->expandAll();
        playlistView->scrollToTop();
    }

    bool isHeaderHidden() const
    {
        return playlistView->isHeaderHidden();
    }

    void setHeaderHidden(bool showHeader) const
    {
        playlistView->setHeaderHidden(!showHeader);
    }

    bool isScrollbarHidden() const
    {
        return playlistView->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
    }

    void setScrollbarHidden(bool showScrollBar) const
    {
        playlistView->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    }

    void selectionChanged()
    {
        if(changingSelection) {
            return;
        }

        changingSelection = true;

        QItemSelectionModel* selectionModel = playlistView->selectionModel();

        QModelIndexList indexes = selectionModel->selectedIndexes();
        QItemSelection itemsToSelect;
        QItemSelection itemsToDeselect;

        Core::TrackList tracks;

        while(!indexes.empty()) {
            const QModelIndex& index = indexes.front();
            indexes.pop_front();

            if(!index.isValid()) {
                continue;
            }

            const auto type = index.data(PlaylistItem::Type).toInt();

            if(type != PlaylistItem::Track) {
                itemsToDeselect.emplace_back(index);

                const QItemSelection children{model->index(0, 0, index),
                                              model->index(model->rowCount(index) - 1, 0, index)};
                const auto childIndexes = children.indexes();

                auto childrenToSelect = std::views::filter(childIndexes, [&](const QModelIndex& childIndex) {
                    return !selectionModel->isSelected(childIndex);
                });

                std::ranges::copy(childrenToSelect, std::back_inserter(indexes));
            }
            else {
                itemsToSelect.emplace_back(index);
                tracks.push_back(index.data(PlaylistItem::Role::ItemData).value<Core::Track>());
            }
        }
        selectionModel->select(itemsToDeselect, QItemSelectionModel::Deselect);
        selectionModel->select(itemsToSelect, QItemSelectionModel::Select);

        if(!tracks.empty()) {
            selectionController->changeSelectedTracks(tracks);
        }
        changingSelection = false;
    }

    void customHeaderMenuRequested(QPoint pos)
    {
        auto* menu = new QMenu(widget);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        auto* presetsMenu = new QMenu("Presets", menu);

        const auto& presets = controller->presetRegistry()->items();

        for(const auto& [index, preset] : presets) {
            const QString name = preset.name;
            auto* switchPreset = new QAction(name, presetsMenu);
            if(preset == currentPreset) {
                presetsMenu->setDefaultAction(switchPreset);
            }
            QObject::connect(switchPreset, &QAction::triggered, widget, [this, name]() {
                settings->set<Settings::CurrentPreset>(name);
            });
            presetsMenu->addAction(switchPreset);
            //            }
        }
        menu->addMenu(presetsMenu);

        menu->popup(widget->mapToGlobal(pos));
    }

    void changeState(Core::Player::PlayState state) const
    {
        switch(state) {
            case(Core::Player::Playing):
                model->changeTrackState();
                return findCurrent();
            case(Core::Player::Stopped):
            case(Core::Player::Paused):
                return model->changeTrackState();
        }
    }

    void playlistTracksChanged()
    {
        QModelIndexList indexes{{}};

        Core::TrackList tracks;

        while(!indexes.empty()) {
            const QModelIndex& index = indexes.front();
            indexes.pop_front();

            const auto type = index.data(PlaylistItem::Type).toInt();

            if(type != PlaylistItem::Track) {
                const QItemSelection children{model->index(0, 0, index),
                                              model->index(model->rowCount(index) - 1, 0, index)};
                const auto childIndexes = children.indexes();
                std::ranges::copy(childIndexes, std::back_inserter(indexes));
            }
            else {
                tracks.push_back(index.data(PlaylistItem::Role::ItemData).value<Core::Track>());
            }
        }
        if(!tracks.empty()) {
            if(auto playlist = controller->currentPlaylist()) {
                controller->playlistHandler()->replacePlaylistTracks(playlist->id(), tracks);
            }
        }
    }

    void doubleClicked(const QModelIndex& index) const
    {
        const auto type = index.data(PlaylistItem::Type);

        if(type != PlaylistItem::Track) {
            selectionController->executeAction(TrackAction::Play);
        }
        else {
            const auto track = index.data(PlaylistItem::Role::ItemData).value<Core::Track>();
            controller->startPlayback(track);
        }
        model->changeTrackState();
        playlistView->clearSelection();
    }

    void findCurrent() const
    {
        const Core::Track track = playerManager->currentTrack();
        //        const QModelIndex index = model->indexForTrack(track);
        //        if(index.isValid()) {
        //            playlistView->scrollTo(index);
        //            playlistView->setCurrentIndex(index);
        //        }
    }

    void expandPlaylist(const QModelIndex& parent, int first, int last) const
    {
        while(first <= last) {
            const QModelIndex child = model->index(first, 0, parent);
            playlistView->expand(child);
            ++first;
        }
    }

    void switchContextMenu(int section, QPoint pos)
    {
        Q_UNUSED(section)
        auto* menu = new QMenu(widget);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        //        const auto currentPlaylist = playlistController->currentPlaylist();
        const auto& playlists = controller->playlists();

        for(const auto& playlist : playlists) {
            auto* switchPl = new QAction(playlist.name(), menu);
            const int id   = playlist.id();
            QObject::connect(switchPl, &QAction::triggered, widget, [this, id]() {
                controller->changeCurrentPlaylist(id);
            });
            menu->addAction(switchPl);
        }
        menu->popup(widget->mapToGlobal(pos));
    }

    void changeSort(const QString& script)
    {
        /* if(playlistView->selectionModel()->hasSelection()) { }
         else*/
        if(auto playlist = controller->currentPlaylist()) {
            const auto sortedTracks = Core::Library::Sorting::calcSortTracks(script, playlist->tracks());
            playlist->replaceTracks(sortedTracks);
            controller->playlistHandler()->replacePlaylistTracks(playlist->id(), sortedTracks);
            changePlaylist(*playlist);
        }
    }

    void addSortMenu(QMenu* parent)
    {
        auto* sortMenu = new QMenu("Sort", parent);

        const auto& groups = controller->sortRegistry()->items();
        for(const auto& [index, script] : groups) {
            auto* switchSort = new QAction(script.name, sortMenu);
            QObject::connect(switchSort, &QAction::triggered, widget, [this, script]() {
                changeSort(script.script);
            });
            sortMenu->addAction(switchSort);
        }
        parent->addMenu(sortMenu);
    }
};

PlaylistWidget::PlaylistWidget(Core::Player::PlayerManager* playerManager, PlaylistController* playlistController,
                               TrackSelectionController* selectionController, Utils::SettingsManager* settings,
                               QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerManager, playlistController, selectionController, settings)}
{
    setObjectName("Playlist");

    if(auto playlist = p->controller->currentPlaylist()) {
        p->changePlaylist(*playlist);
    }
}

PlaylistWidget::~PlaylistWidget() = default;

QString PlaylistWidget::name() const
{
    return "Playlist";
}

void PlaylistWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* removeRows = new QAction("Remove", menu);
    QObject::connect(removeRows, &QAction::triggered, this, [this]() {
        QModelIndexList trackSelection;
        const auto selected = p->playlistView->selectionModel()->selectedIndexes();
        std::ranges::copy_if(std::as_const(selected), std::back_inserter(trackSelection), [](const QModelIndex& index) {
            return index.isValid() && index.data(PlaylistItem::Type).toInt() == PlaylistItem::Track;
        });

        p->model->removeTracks(trackSelection);
        p->playlistTracksChanged();
    });
    menu->addAction(removeRows);

    menu->addSeparator();

    p->addSortMenu(menu);
    p->selectionController->addTrackContextMenu(menu);

    menu->popup(mapToGlobal(event->pos()));
}

void PlaylistWidget::keyPressEvent(QKeyEvent* e)
{
    const auto key = e->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        if(p->selectionController->hasTracks()) {
            p->selectionController->executeAction(TrackAction::Play);

            p->model->changeTrackState();
            p->playlistView->clearSelection();
        }
    }
    QWidget::keyPressEvent(e);
}

// void PlaylistWidget::spanHeaders(QModelIndex parent)
//{
//     for (int i = 0; i < p->model->rowCount(parent); i++)
//     {
//         auto idx = p->model->index(i, 0, parent);
//         if (p->model->hasChildren(idx))
//         {
//             p->playlist->setFirstColumnSpanned(i, parent, true);
//             spanHeaders(idx);
//         }
//     }
// }
} // namespace Fy::Gui::Widgets::Playlist
