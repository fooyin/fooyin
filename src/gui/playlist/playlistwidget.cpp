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
#include "playlistcontroller.h"
#include "playlistdelegate.h"
#include "playlistmodel.h"
#include "playlistview.h"
#include "presetregistry.h"

#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
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
    Core::Library::MusicLibrary* library;
    Core::Player::PlayerManager* playerManager;
    PresetRegistry* presetRegistry;
    Utils::SettingsManager* settings;
    Utils::SettingsDialogController* settingsDialog;

    PlaylistController* playlistController;
    Core::TrackList selectedTracks;

    QHBoxLayout* layout;
    PlaylistModel* model;
    PlaylistView* playlistView;
    Utils::HeaderView* header;
    bool changingSelection{false};

    Private(PlaylistWidget* widget, Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
            PlaylistController* playlistController, PresetRegistry* presetRegistry, Utils::SettingsManager* settings)
        : widget{widget}
        , library{library}
        , playerManager{playerManager}
        , presetRegistry{presetRegistry}
        , settings{settings}
        , settingsDialog{settings->settingsDialog()}
        , playlistController{playlistController}
        , layout{new QHBoxLayout(widget)}
        , model{new PlaylistModel(playerManager, settings, widget)}
        , playlistView{new PlaylistView(widget)}
        , header{new Utils::HeaderView(Qt::Horizontal, widget)}
    {
        layout->setContentsMargins(0, 0, 0, 0);

        header->setStretchLastSection(true);
        playlistView->setHeader(header);
        header->setContextMenuPolicy(Qt::CustomContextMenu);

        model->changePreset(presetRegistry->presetByName(settings->value<Settings::CurrentPreset>()));

        playlistView->setModel(model);
        playlistView->setItemDelegate(new PlaylistDelegate(this));

        layout->addWidget(playlistView);

        setHeaderHidden(settings->value<Settings::PlaylistHeader>());
        setScrollbarHidden(settings->value<Settings::PlaylistScrollBar>());

        connect(playlistView->header(), &QHeaderView::customContextMenuRequested, this,
                &PlaylistWidget::Private::customHeaderMenuRequested);
        connect(playlistView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                &PlaylistWidget::Private::selectionChanged);

        connect(playlistView, &PlaylistView::doubleClicked, this, &PlaylistWidget::Private::playTrack);
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

        QObject::connect(presetRegistry, &PresetRegistry::presetChanged, this,
                         &PlaylistWidget::Private::onPresetChanged);

        settings->subscribe<Settings::PlaylistHeader>(this, &PlaylistWidget::Private::setHeaderHidden);
        settings->subscribe<Settings::PlaylistScrollBar>(this, &PlaylistWidget::Private::setScrollbarHidden);

        settings->subscribe<Settings::CurrentPreset>(this, &PlaylistWidget::Private::changePreset);
    }

    void onPresetChanged(const PlaylistPreset& preset)
    {
        if(preset.name == settings->value<Settings::CurrentPreset>()) {
            changePreset(preset.name);
        }
    }

    void changePreset(const QString& name)
    {
        model->changePreset(presetRegistry->presetByName(name));
        model->reset(playlistController->currentPlaylist());
    }

    void changePlaylist(Core::Playlist::Playlist* playlist)
    {
        if(playlist) {
            model->reset(playlist);
        }
    }

    void reset()
    {
        playlistView->expandAll();
        playlistView->scrollToTop();
    }

    bool isHeaderHidden()
    {
        return playlistView->isHeaderHidden();
    }

    void setHeaderHidden(bool showHeader)
    {
        playlistView->setHeaderHidden(!showHeader);
    }

    bool isScrollbarHidden()
    {
        return playlistView->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
    }

    void setScrollbarHidden(bool showScrollBar)
    {
        playlistView->setVerticalScrollBarPolicy(!showScrollBar ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    }

    void selectionChanged()
    {
        if(changingSelection) {
            return;
        }
        changingSelection = true;
        clearSelection();

        const auto selectedIndexes = playlistView->selectionModel()->selectedIndexes();
        std::vector<QModelIndex> indexes;

        indexes.insert(indexes.cend(), selectedIndexes.cbegin(), selectedIndexes.cend());

        while(!indexes.empty()) {
            const auto index = indexes.front();
            indexes.erase(indexes.cbegin());
            if(index.isValid()) {
                const auto type = index.data(PlaylistItem::Type);
                if(type == PlaylistItem::Track) {
                    selectedTracks.emplace_back(index.data(PlaylistItem::Role::ItemData).value<Core::Track>());
                }
                else {
                    const QItemSelection selectedChildren{model->index(0, 0, index),
                                                          model->index(model->rowCount(index) - 1, 0, index)};
                    const auto childIndexes = selectedChildren.indexes();
                    indexes.insert(indexes.end(), childIndexes.begin(), childIndexes.end());
                    playlistView->selectionModel()->select(selectedChildren, QItemSelectionModel::Select);
                }
            }
        }
        emit widget->selectionWasChanged(selectedTracks);
        changingSelection = false;
    }

    void customHeaderMenuRequested(QPoint pos)
    {
        auto* menu = new QMenu(widget);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        const auto& currentPreset = presetRegistry->presetByName(settings->value<Settings::CurrentPreset>());
        const auto& presets       = presetRegistry->presets();

        for(const auto& [index, preset] : presets) {
            //            if(preset.name != currentPreset.name) {
            const QString name = preset.name;
            auto* switchPreset = new QAction(name, menu);
            QObject::connect(switchPreset, &QAction::triggered, this, [this, name]() {
                settings->set<Settings::CurrentPreset>(name);
            });
            menu->addAction(switchPreset);
            //            }
        }
        menu->popup(widget->mapToGlobal(pos));
    }

    void changeState(Core::Player::PlayState state)
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

    void playTrack(const QModelIndex& index)
    {
        const auto type = index.data(PlaylistItem::Type);
        Core::Track track;

        if(type != PlaylistItem::Track && !selectedTracks.empty()) {
            track = selectedTracks.front();
        }
        else {
            track = index.data(PlaylistItem::Role::ItemData).value<Core::Track>();
        }

        playlistController->startPlayback(track);
        model->changeTrackState();
        clearSelection(true);
    }

    void findCurrent()
    {
        const Core::Track track = playerManager->currentTrack();
        const QModelIndex index = model->indexForTrack(track);
        if(index.isValid()) {
            playlistView->scrollTo(index);
            playlistView->setCurrentIndex(index);
        }
    }

    void expandPlaylist(const QModelIndex& parent, int first, int last)
    {
        while(first <= last) {
            playlistView->expand(model->index(first, 0, parent));
            ++first;
        }
    }

    void clearSelection(bool clearView = false)
    {
        if(clearView) {
            playlistView->clearSelection();
        }
        selectedTracks.clear();
    }

    void switchContextMenu(int section, QPoint pos)
    {
        Q_UNUSED(section)
        auto* menu = new QMenu(widget);
        menu->setAttribute(Qt::WA_DeleteOnClose);

        const auto* currentPlaylist = playlistController->currentPlaylist();
        const auto& playlists       = playlistController->playlists();

        for(const auto& playlist : playlists) {
            if(playlist.get() != currentPlaylist) {
                auto* switchPl = new QAction(playlist->name(), menu);
                const int id   = playlist->id();
                QObject::connect(switchPl, &QAction::triggered, this, [this, id]() {
                    playlistController->changeCurrentPlaylist(id);
                });
                menu->addAction(switchPl);
            }
        }
        menu->popup(widget->mapToGlobal(pos));
    }
};

PlaylistWidget::PlaylistWidget(Core::Library::MusicLibrary* library, Core::Player::PlayerManager* playerManager,
                               PlaylistController* playlistController, PresetRegistry* presetRegistry,
                               Utils::SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, library, playerManager, playlistController, presetRegistry, settings)}
{
    setObjectName("Playlist");

    p->changePlaylist(p->playlistController->currentPlaylist());
}

PlaylistWidget::~PlaylistWidget() = default;

QString PlaylistWidget::name() const
{
    return "Playlist";
}

void PlaylistWidget::keyPressEvent(QKeyEvent* e)
{
    const auto key = e->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        if(!p->selectedTracks.empty()) {
            p->playlistController->startPlayback(p->selectedTracks.front());

            p->model->changeTrackState();
            p->playlistView->clearSelection();
            p->selectedTracks.clear();
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
