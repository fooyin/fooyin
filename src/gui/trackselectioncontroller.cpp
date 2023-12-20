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

#include <core/playlist/playlistmanager.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QFileInfo>
#include <QMenu>

#include <ranges>

namespace Fooyin {
struct TrackSelectionController::Private
{
    TrackSelectionController* self;

    ActionManager* actionManager;
    SettingsManager* settings;
    PlaylistController* playlistController;
    PlaylistManager* playlistHandler;

    QString selectionTitle{tr("New playlist")};
    TrackList tracks;
    int firstIndex{-1};

    ActionContainer* tracksMenu{nullptr};
    ActionContainer* tracksPlaylistMenu{nullptr};

    QAction* addCurrent;
    QAction* addActive;
    QAction* sendCurrent;
    QAction* sendNew;
    QAction* openFolder;
    QAction* openProperties;

    Private(TrackSelectionController* self, ActionManager* actionManager, SettingsManager* settings,
            PlaylistController* playlistController)
        : self{self}
        , actionManager{actionManager}
        , settings{settings}
        , playlistController{playlistController}
        , playlistHandler{playlistController->playlistHandler()}
        , tracksMenu{actionManager->createMenu(Constants::Menus::Context::TrackSelection)}
        , tracksPlaylistMenu{actionManager->createMenu(Constants::Menus::Context::TracksPlaylist)}
        , openFolder{new QAction(tr("Open Containing Folder"), tracksMenu)}
    {
        // Playlist menu
        tracksPlaylistMenu->addSeparator();

        addCurrent = new QAction(tr("Add to current playlist"), tracksPlaylistMenu);
        QObject::connect(addCurrent, &QAction::triggered, tracksPlaylistMenu, [this]() { addToCurrentPlaylist(); });
        QObject::connect(addCurrent, &QAction::triggered, tracksPlaylistMenu, [this]() { addToCurrentPlaylist(); });
        tracksPlaylistMenu->addAction(actionManager->registerAction(addCurrent, "TrackSelection.AddCurrentPlaylist"));

        addActive = new QAction(tr("Add to active playlist"), tracksPlaylistMenu);
        QObject::connect(addActive, &QAction::triggered, tracksPlaylistMenu, [this]() { addToActivePlaylist(); });
        tracksPlaylistMenu->addAction(actionManager->registerAction(addActive, "TrackSelection.AddActivePlaylist"));

        sendCurrent = new QAction(tr("Send to current playlist"), tracksPlaylistMenu);
        QObject::connect(sendCurrent, &QAction::triggered, tracksPlaylistMenu, [this]() { sendToCurrentPlaylist(); });
        tracksPlaylistMenu->addAction(actionManager->registerAction(sendCurrent, "TrackSelection.SendCurrentPlaylist"));

        sendNew = new QAction(tr("Send to new playlist"), tracksPlaylistMenu);
        QObject::connect(sendNew, &QAction::triggered, tracksPlaylistMenu,
                         [this]() { sendToNewPlaylist({}, selectionTitle); });
        tracksPlaylistMenu->addAction(actionManager->registerAction(sendNew, "TrackSelection.SendNewPlaylist"));

        tracksPlaylistMenu->addSeparator();

        // Tracks menu
        tracksMenu->addSeparator();

        QObject::connect(openFolder, &QAction::triggered, tracksMenu, [this]() {
            if(!tracks.empty()) {
                const QString dir = QFileInfo{tracks.front().filepath()}.absolutePath();
                Utils::File::openDirectory(dir);
            }
        });
        tracksMenu->addAction(actionManager->registerAction(openFolder, "TrackSelection.OpenFolder"));

        tracksMenu->addSeparator();

        openProperties = new QAction(tr("Properties"), tracksMenu);
        QObject::connect(openProperties, &QAction::triggered, self, [self]() {
            QMetaObject::invokeMethod(self, &TrackSelectionController::requestPropertiesDialog);
        });
        tracksMenu->addAction(actionManager->registerAction(openProperties, "TrackSelection.OpenProperties"));

        tracksMenu->addSeparator();

        updateActionState();
    }

    void handleActions(Playlist* playlist, PlaylistAction::ActionOptions options) const
    {
        if(!playlist) {
            return;
        }

        if(options & PlaylistAction::Switch) {
            playlistController->changeCurrentPlaylist(playlist);
        }
    }

    void sendToNewPlaylist(PlaylistAction::ActionOptions options = {}, const QString& playlistName = {}) const
    {
        QString newName{playlistName};
        if(newName.isEmpty()) {
            newName = selectionTitle;
        }

        if(options & PlaylistAction::KeepActive) {
            auto* activePlaylist = playlistHandler->activePlaylist();

            if(!activePlaylist || activePlaylist->name() != newName) {
                auto* playlist = playlistHandler->createPlaylist(newName, tracks);
                handleActions(playlist, options);
                return;
            }
            const QString keepActiveName = newName + tr(" (Playback)");

            if(auto* keepActivePlaylist = playlistHandler->playlistByName(keepActiveName)) {
                keepActivePlaylist->changeCurrentTrack(activePlaylist->currentTrackIndex());
                keepActivePlaylist->replaceTracks(activePlaylist->tracks());

                playlistHandler->changeActivePlaylist(keepActivePlaylist->id());
            }
            else {
                playlistHandler->renamePlaylist(activePlaylist->id(), keepActiveName);
            }
        }

        auto* playlist = playlistHandler->createPlaylist(newName, tracks);
        handleActions(playlist, options);
    }

    void sendToCurrentPlaylist(PlaylistAction::ActionOptions options = {}) const
    {
        if(auto* currentPlaylist = playlistController->currentPlaylist()) {
            playlistHandler->createPlaylist(currentPlaylist->name(), tracks);
            handleActions(currentPlaylist, options);
        }
    }

    void addToCurrentPlaylist() const
    {
        if(auto* playlist = playlistController->currentPlaylist()) {
            playlistHandler->appendToPlaylist(playlist->id(), tracks);
        }
    }

    void addToActivePlaylist() const
    {
        if(auto* playlist = playlistHandler->activePlaylist()) {
            playlistHandler->appendToPlaylist(playlist->id(), tracks);
        }
    }

    void updateActionState()
    {
        addCurrent->setEnabled(!tracks.empty());
        addActive->setEnabled(!tracks.empty());
        sendCurrent->setEnabled(!tracks.empty());
        sendNew->setEnabled(!tracks.empty());
        openFolder->setEnabled(allTracksInSameFolder() && !tracks.empty());
        openProperties->setEnabled(!tracks.empty());
    }

    bool allTracksInSameFolder()
    {
        if(tracks.empty()) {
            return false;
        }

        const QString firstPath = QFileInfo{tracks[0].filepath()}.absolutePath();

        return std::ranges::all_of(tracks | std::ranges::views::transform([](const Track& track) {
                                       return QFileInfo{track.filepath()}.absolutePath();
                                   }),
                                   [&firstPath](const QString& folderPath) { return folderPath == firstPath; });
    }
};

TrackSelectionController::TrackSelectionController(ActionManager* actionManager, SettingsManager* settings,
                                                   PlaylistController* playlistController, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, actionManager, settings, playlistController)}
{ }

bool TrackSelectionController::hasTracks() const
{
    return !p->tracks.empty();
}

TrackSelectionController::~TrackSelectionController() = default;

TrackList TrackSelectionController::selectedTracks() const
{
    return p->tracks;
}

void TrackSelectionController::changeSelectedTracks(int index, const TrackList& tracks, const QString& title)
{
    p->firstIndex     = index;
    p->selectionTitle = title;
    if(std::exchange(p->tracks, tracks) == tracks) {
        return;
    }

    p->updateActionState();
    emit selectionChanged(p->tracks);
}

void TrackSelectionController::changeSelectedTracks(const TrackList& tracks, const QString& title)
{
    changeSelectedTracks(-1, tracks, title);
}

void TrackSelectionController::addTrackContextMenu(QMenu* menu) const
{
    Utils::appendMenuActions(p->tracksMenu->menu(), menu);
}

void TrackSelectionController::addTrackPlaylistContextMenu(QMenu* menu) const
{
    Utils::appendMenuActions(p->tracksPlaylistMenu->menu(), menu);
}

void TrackSelectionController::executeAction(TrackAction action, PlaylistAction::ActionOptions options,
                                             const QString& playlistName)
{
    switch(action) {
        case(TrackAction::SendCurrentPlaylist): {
            p->sendToCurrentPlaylist(options);
            break;
        }
        case(TrackAction::SendNewPlaylist): {
            p->sendToNewPlaylist(options, playlistName);
            break;
        }
        case(TrackAction::AddCurrentPlaylist): {
            p->addToCurrentPlaylist();
            break;
        }
        case(TrackAction::AddActivePlaylist): {
            p->addToActivePlaylist();
            break;
        }
        case(TrackAction::Play): {
            if(!p->tracks.empty()) {
                if(auto* playlist = p->playlistController->currentPlaylist()) {
                    if(p->firstIndex >= 0) {
                        playlist->changeCurrentTrack(p->firstIndex);
                    }
                    p->playlistHandler->startPlayback(playlist->id());
                }
            }
            break;
        }
        case(TrackAction::Expand):
        case(TrackAction::None):
            break;
    }
}
} // namespace Fooyin

#include "gui/moc_trackselectioncontroller.cpp"
