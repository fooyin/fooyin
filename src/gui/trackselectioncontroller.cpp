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

#include <gui/trackselectioncontroller.h>

#include "playlist/playlistcontroller.h"

#include <core/coresettings.h>
#include <core/playlist/playlistmanager.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QFileInfo>
#include <QMenu>

#include <ranges>

namespace Fy::Gui {
struct TrackSelectionController::Private : QObject
{
    TrackSelectionController* controller;
    Utils::ActionManager* actionManager;
    Utils::SettingsManager* settings;
    Widgets::Playlist::PlaylistController* playlistController;
    Core::Playlist::PlaylistManager* playlistHandler;

    QString selectionTitle{"New playlist"};
    Core::TrackList tracks;

    Utils::ActionContainer* tracksMenu{nullptr};
    Utils::ActionContainer* tracksPlaylistMenu{nullptr};

    QAction* openFolder;

    Private(TrackSelectionController* controller, Utils::ActionManager* actionManager, Utils::SettingsManager* settings,
            Widgets::Playlist::PlaylistController* playlistController)
        : controller{controller}
        , actionManager{actionManager}
        , settings{settings}
        , playlistController{playlistController}
        , playlistHandler{playlistController->playlistHandler()}
        , openFolder{new QAction("Open Containing Folder", tracksMenu)}
    {
        tracksMenu         = actionManager->createMenu(Constants::Menus::Context::TrackSelection);
        tracksPlaylistMenu = actionManager->createMenu(Constants::Menus::Context::TracksPlaylist);

        tracksPlaylistMenu->addSeparator();

        auto* addCurrent = new QAction("Add to current playlist", tracksPlaylistMenu);
        QObject::connect(addCurrent, &QAction::triggered, tracksPlaylistMenu, [this]() {
            addToCurrentPlaylist();
        });
        tracksPlaylistMenu->addAction(addCurrent);

        auto* addActive = new QAction("Add to active playlist", tracksPlaylistMenu);
        QObject::connect(addActive, &QAction::triggered, tracksPlaylistMenu, [this]() {
            addToActivePlaylist();
        });
        tracksPlaylistMenu->addAction(addActive);

        auto* sendCurrent = new QAction("Send to current playlist", tracksPlaylistMenu);
        QObject::connect(sendCurrent, &QAction::triggered, tracksPlaylistMenu, [this]() {
            sendToCurrentPlaylist();
        });
        tracksPlaylistMenu->addAction(sendCurrent);

        auto* sendNew = new QAction("Send to new playlist", tracksPlaylistMenu);
        QObject::connect(sendNew, &QAction::triggered, tracksPlaylistMenu, [this]() {
            sendToNewPlaylist({}, selectionTitle);
        });
        tracksPlaylistMenu->addAction(sendNew);

        tracksPlaylistMenu->addSeparator();

        tracksMenu->addSeparator();

        QObject::connect(openFolder, &QAction::triggered, tracksMenu, [this]() {
            if(!tracks.empty()) {
                const QString dir = QFileInfo{tracks.front().filepath()}.absolutePath();
                Utils::File::openDirectory(dir);
            }
        });
        tracksMenu->addAction(openFolder);

        tracksMenu->addSeparator();

        auto* openProperties = new QAction("Properties", tracksMenu);
        QObject::connect(openProperties, &QAction::triggered, controller, [controller]() {
            emit controller->requestPropertiesDialog();
        });
        tracksMenu->addAction(openProperties);

        tracksMenu->addSeparator();
    }

    void sendToNewPlaylist(ActionOptions options = {}, const QString& playlistName = {})
    {
        if(options & KeepActive) {
            auto activePlaylist = playlistHandler->activePlaylist();
            if(!activePlaylist || activePlaylist->name() != playlistName) {
                playlistHandler->createPlaylist(playlistName, tracks, options & Switch);
                return;
            }
            const QString keepActiveName = playlistName + " (Playback)";
            if(auto keepActivePlaylist = playlistHandler->playlistByName(keepActiveName)) {
                playlistHandler->exchangePlaylist(*keepActivePlaylist, *activePlaylist);
                playlistHandler->changeActivePlaylist(keepActivePlaylist->id());
            }
            else {
                playlistHandler->renamePlaylist(activePlaylist->id(), keepActiveName);
            }
        }
        playlistHandler->createPlaylist(playlistName, tracks, options & Switch);
    }

    void sendToCurrentPlaylist(ActionOptions options = {})
    {
        if(auto currentPlaylist = playlistController->currentPlaylist()) {
            playlistHandler->createPlaylist(currentPlaylist->name(), tracks, options & Switch);
        }
    }

    void addToCurrentPlaylist()
    {
        if(auto playlist = playlistController->currentPlaylist()) {
            playlistHandler->appendToPlaylist(playlist->id(), tracks);
        }
    }

    void addToActivePlaylist()
    {
        if(auto playlist = playlistHandler->activePlaylist()) {
            playlistHandler->appendToPlaylist(playlist->id(), tracks);
        }
    }

    void updateActionState()
    {
        openFolder->setEnabled(allTracksInSameFolder());
    }

    bool allTracksInSameFolder()
    {
        if(tracks.empty()) {
            return false;
        }

        const QString firstPath = QFileInfo{tracks[0].filepath()}.absolutePath();

        return std::ranges::all_of(tracks | std::ranges::views::transform([](const Core::Track& track) {
                                       return QFileInfo{track.filepath()}.absolutePath();
                                   }),
                                   [&firstPath](const QString& folderPath) {
                                       return folderPath == firstPath;
                                   });
    }
};

TrackSelectionController::TrackSelectionController(Utils::ActionManager* actionManager,
                                                   Utils::SettingsManager* settings,
                                                   Widgets::Playlist::PlaylistController* playlistController)
    : p{std::make_unique<Private>(this, actionManager, settings, playlistController)}
{ }

bool TrackSelectionController::hasTracks() const
{
    return !p->tracks.empty();
}

TrackSelectionController::~TrackSelectionController() = default;

Core::TrackList TrackSelectionController::selectedTracks() const
{
    return p->tracks;
}

void TrackSelectionController::changeSelectedTracks(const Core::TrackList& tracks, const QString& title)
{
    p->tracks         = tracks;
    p->selectionTitle = title;
    emit selectionChanged(p->tracks);
}

void TrackSelectionController::addTrackContextMenu(QMenu* menu) const
{
    p->updateActionState();
    Utils::cloneMenu(p->tracksMenu->menu(), menu);
}

void TrackSelectionController::addTrackPlaylistContextMenu(QMenu* menu) const
{
    p->updateActionState();
    Utils::cloneMenu(p->tracksPlaylistMenu->menu(), menu);
}

void TrackSelectionController::executeAction(TrackAction action, ActionOptions options, const QString& playlistName)
{
    switch(action) {
        case(TrackAction::SendCurrentPlaylist):
            return p->sendToCurrentPlaylist(options);
        case(TrackAction::SendNewPlaylist):
            return p->sendToNewPlaylist(options, playlistName);
        case(TrackAction::AddCurrentPlaylist):
            return p->addToCurrentPlaylist();
        case(TrackAction::AddActivePlaylist):
            return p->addToActivePlaylist();
        case(TrackAction::Play):
            if(!p->tracks.empty()) {
                if(auto playlist = p->playlistController->currentPlaylist()) {
                    return p->playlistHandler->startPlayback(playlist->name(), p->tracks.front());
                }
            }
        case(TrackAction::Expand):
        case(TrackAction::None):
            break;
    }
}
} // namespace Fy::Gui
