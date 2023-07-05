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

#include "trackselectioncontroller.h"

#include "gui/guiconstants.h"
#include "gui/playlist/playlistcontroller.h"

#include <core/playlist/playlisthandler.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QFileInfo>
#include <QMenu>

#include <ranges>

namespace Fy::Gui {
enum PlaylistType
{
    Active,
    Current,
    New
};

struct TrackSelectionController::Private
{
    TrackSelectionController* controller;
    Utils::ActionManager* actionManager;
    Widgets::Playlist::PlaylistController* playlistController;
    Core::Playlist::PlaylistHandler* playlistHandler;

    QString selectionTitle;
    Core::TrackList tracks;

    Utils::ActionContainer* contextMenu{nullptr};
    std::vector<QAction*> trackPlaylistActions;
    QAction* openFolder;

    Private(TrackSelectionController* controller, Utils::ActionManager* actionManager,
            Widgets::Playlist::PlaylistController* playlistController)
        : controller{controller}
        , actionManager{actionManager}
        , playlistController{playlistController}
        , playlistHandler{playlistController->playlistHandler()}
        , openFolder{new QAction("Open Containing Folder", contextMenu)}
    {
        contextMenu = actionManager->createMenu(Constants::Menus::Context::TrackSelection);

        auto addPlaylistAction = [this](QAction* action) {
            trackPlaylistActions.push_back(action);
            contextMenu->addAction(action);
        };

        auto* addCurrent = new QAction("Add to current playlist", contextMenu);
        QObject::connect(addCurrent, &QAction::triggered, contextMenu, [this]() {
            addToPlaylist(Current);
        });
        addPlaylistAction(addCurrent);

        auto* addActive = new QAction("Add to active playlist", contextMenu);
        QObject::connect(addActive, &QAction::triggered, contextMenu, [this]() {
            addToPlaylist(Active);
        });
        addPlaylistAction(addActive);

        auto* sendCurrent = new QAction("Send to current playlist", contextMenu);
        QObject::connect(sendCurrent, &QAction::triggered, contextMenu, [this]() {
            sendToPlaylist(Current);
        });
        addPlaylistAction(sendCurrent);

        auto* sendNew = new QAction("Send to new playlist", contextMenu);
        QObject::connect(sendNew, &QAction::triggered, contextMenu, [this]() {
            sendToPlaylist(New, {}, selectionTitle);
        });
        addPlaylistAction(sendNew);

        contextMenu->addSeparator();

        QObject::connect(openFolder, &QAction::triggered, contextMenu, [this]() {
            if(!tracks.empty()) {
                const QString dir = QFileInfo{tracks.front().filepath()}.absolutePath();
                Utils::File::openDirectory(dir);
            }
        });
        contextMenu->addAction(openFolder);

        contextMenu->addSeparator();

        auto* openProperties = new QAction("Properties", contextMenu);
        QObject::connect(openProperties, &QAction::triggered, controller, [controller]() {
            emit controller->requestPropertiesDialog();
        });
        contextMenu->addAction(openProperties);

        contextMenu->addSeparator();
    }

    void sendToPlaylist(PlaylistType type, ActionOptions options = {}, const QString& playlistName = {})
    {
        if(type == Current) {
            if(auto currentPlaylist = playlistController->currentPlaylist()) {
                playlistHandler->createPlaylist(currentPlaylist->name(), tracks, options & Switch);
            }
        }
        else {
            playlistHandler->createPlaylist(playlistName, tracks, options & Switch);
        }
    }

    void addToPlaylist(PlaylistType type)
    {
        auto append = [this](auto playlistToAppend) {
            if(playlistToAppend) {
                playlistHandler->appendToPlaylist(playlistToAppend->id(), tracks);
            }
        };

        append(type == Active ? playlistHandler->activePlaylist() : playlistController->currentPlaylist());
    }

    void togglePlaylistActions(bool enable)
    {
        for(QAction* action : trackPlaylistActions) {
            action->setVisible(enable);
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
                                                   Widgets::Playlist::PlaylistController* playlistController)
    : p{std::make_unique<Private>(this, actionManager, playlistController)}
{ }

bool TrackSelectionController::hasTracks() const
{
    return !p->tracks.empty();
}

TrackSelectionController::~TrackSelectionController() = default;

const Core::TrackList& TrackSelectionController::selectedTracks() const
{
    return p->tracks;
}

void TrackSelectionController::changeSelectedTracks(const Core::TrackList& tracks, const QString& title)
{
    p->tracks         = tracks;
    p->selectionTitle = title;
    emit selectionChanged(p->tracks);
}

void TrackSelectionController::addTrackContextMenu(QMenu* menu, bool playlistActions) const
{
    p->togglePlaylistActions(playlistActions);
    p->updateActionState();
    Utils::cloneMenu(p->contextMenu->menu(), menu);
}

void TrackSelectionController::executeAction(TrackAction action, ActionOptions options, const QString& playlistName)
{
    switch(action) {
        case(TrackAction::SendCurrentPlaylist):
            return p->sendToPlaylist(Current, options, playlistName);
        case(TrackAction::SendNewPlaylist):
            return p->sendToPlaylist(New, options, playlistName);
        case(TrackAction::AddCurrentPlaylist):
            return p->addToPlaylist(Current);
        case(TrackAction::AddActivePlaylist):
            return p->addToPlaylist(Active);
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
