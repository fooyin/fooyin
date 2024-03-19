/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>

#include <ranges>

namespace Fooyin {
struct WidgetSelection
{
    TrackList tracks;
    int firstIndex{0};
    QString name{TrackSelectionController::tr("New playlist")};
};

struct TrackSelectionController::Private
{
    TrackSelectionController* self;

    ActionManager* actionManager;
    SettingsManager* settings;
    PlaylistController* playlistController;
    PlaylistHandler* playlistHandler;

    std::unordered_map<QWidget*, WidgetContext*> contextWidgets;
    std::unordered_map<WidgetContext*, WidgetSelection> contextSelection;
    WidgetContext* activeContext;

    ActionContainer* tracksMenu{nullptr};
    ActionContainer* tracksQueueMenu{nullptr};
    ActionContainer* tracksPlaylistMenu{nullptr};

    QAction* addCurrent;
    QAction* addActive;
    QAction* sendCurrent;
    QAction* sendNew;
    QAction* addToQueue;
    QAction* removeFromQueue;
    QAction* openFolder;
    QAction* openProperties;

    Private(TrackSelectionController* self_, ActionManager* actionManager_, SettingsManager* settings_,
            PlaylistController* playlistController_)
        : self{self_}
        , actionManager{actionManager_}
        , settings{settings_}
        , playlistController{playlistController_}
        , playlistHandler{playlistController->playlistHandler()}
        , tracksMenu{actionManager->createMenu(Constants::Menus::Context::TrackSelection)}
        , tracksQueueMenu{actionManager->createMenu(Constants::Menus::Context::TrackSelection)}
        , tracksPlaylistMenu{actionManager->createMenu(Constants::Menus::Context::TracksPlaylist)}
        , addCurrent{new QAction(tr("Add to current playlist"), tracksPlaylistMenu)}
        , addActive{new QAction(tr("Add to active playlist"), tracksPlaylistMenu)}
        , sendCurrent{new QAction(tr("Send to current playlist"), tracksPlaylistMenu)}
        , sendNew{new QAction(tr("Send to new playlist"), tracksPlaylistMenu)}
        , addToQueue{new QAction(tr("Add to Playback Queue"), tracksMenu)}
        , removeFromQueue{new QAction(tr("Remove from Playback Queue"), tracksMenu)}
        , openFolder{new QAction(tr("Open Containing Folder"), tracksMenu)}
        , openProperties{new QAction(tr("Properties"), tracksMenu)}
    {
        // Playlist menu
        tracksPlaylistMenu->addSeparator();

        QObject::connect(addCurrent, &QAction::triggered, tracksPlaylistMenu, [this]() { addToCurrentPlaylist(); });
        tracksPlaylistMenu->addAction(actionManager->registerAction(addCurrent, "TrackSelection.AddCurrentPlaylist"));

        QObject::connect(addActive, &QAction::triggered, tracksPlaylistMenu, [this]() { addToActivePlaylist(); });
        tracksPlaylistMenu->addAction(actionManager->registerAction(addActive, "TrackSelection.AddActivePlaylist"));

        QObject::connect(sendCurrent, &QAction::triggered, tracksPlaylistMenu, [this]() { sendToCurrentPlaylist(); });
        tracksPlaylistMenu->addAction(actionManager->registerAction(sendCurrent, "TrackSelection.SendCurrentPlaylist"));

        QObject::connect(sendNew, &QAction::triggered, tracksPlaylistMenu, [this]() {
            if(self->hasTracks()) {
                const auto& selection = contextSelection.at(activeContext);
                sendToNewPlaylist(PlaylistAction::Switch, selection.name);
            }
        });
        tracksPlaylistMenu->addAction(actionManager->registerAction(sendNew, "TrackSelection.SendNewPlaylist"));

        tracksPlaylistMenu->addSeparator();

        // Tracks menu
        tracksMenu->addSeparator();

        QObject::connect(addToQueue, &QAction::triggered, tracksQueueMenu, [this]() {
            if(self->hasTracks()) {
                const auto& selection = contextSelection.at(activeContext);
                playlistController->playerController()->queueTracks(selection.tracks);
            }
        });
        tracksQueueMenu->addAction(actionManager->registerAction(addToQueue, Constants::Actions::AddToQueue));

        QObject::connect(removeFromQueue, &QAction::triggered, tracksQueueMenu, [this]() {
            if(self->hasTracks()) {
                const auto& selection = contextSelection.at(activeContext);
                playlistController->playerController()->dequeueTracks(selection.tracks);
            }
        });
        tracksQueueMenu->addAction(actionManager->registerAction(removeFromQueue, Constants::Actions::RemoveFromQueue));

        QObject::connect(openFolder, &QAction::triggered, tracksMenu, [this]() {
            if(self->hasTracks()) {
                const auto& selection = contextSelection.at(activeContext);
                const QString dir     = QFileInfo{selection.tracks.front().filepath()}.absolutePath();
                Utils::File::openDirectory(dir);
            }
        });
        tracksMenu->addAction(actionManager->registerAction(openFolder, "TrackSelection.OpenFolder"));

        tracksMenu->addSeparator();

        QObject::connect(openProperties, &QAction::triggered, self, [this]() {
            QMetaObject::invokeMethod(self, &TrackSelectionController::requestPropertiesDialog);
        });
        tracksMenu->addAction(actionManager->registerAction(openProperties, "TrackSelection.OpenProperties"));

        tracksMenu->addSeparator();

        updateActionState();
    }

    WidgetContext* contextObject(QWidget* widget) const
    {
        if(contextWidgets.contains(widget)) {
            return contextWidgets.at(widget);
        }
        return nullptr;
    }

    bool addContextObject(WidgetContext* context)
    {
        if(!context) {
            return false;
        }

        QWidget* widget = context->widget();
        if(contextWidgets.contains(widget)) {
            return true;
        }

        contextWidgets.emplace(widget, context);
        QObject::connect(context, &QObject::destroyed, self, [this, context] { removeContextObject(context); });

        return true;
    }

    void removeContextObject(WidgetContext* context)
    {
        if(!context) {
            return;
        }

        QObject::disconnect(context, &QObject::destroyed, self, nullptr);

        if(!std::erase_if(contextWidgets, [context](const auto& v) { return v.second == context; })) {
            return;
        }

        contextSelection.erase(context);

        if(activeContext == context) {
            activeContext = nullptr;
        }
    }

    void updateActiveContext(QWidget* widget)
    {
        if(qobject_cast<QMenuBar*>(widget) || qobject_cast<QMenu*>(widget)) {
            return;
        }

        if(QWidget* focusedWidget = QApplication::focusWidget()) {
            WidgetContext* widgetContext{nullptr};
            while(focusedWidget) {
                widgetContext = contextObject(focusedWidget);
                if(widgetContext) {
                    activeContext = widgetContext;
                    updateActionState();
                    QMetaObject::invokeMethod(self, &TrackSelectionController::selectionChanged);
                    return;
                }
                focusedWidget = focusedWidget->parentWidget();
            }
        }
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
        if(!self->hasTracks()) {
            return;
        }

        QString newName{playlistName};

        const auto& selection = contextSelection.at(activeContext);

        if(newName.isEmpty()) {
            newName = selection.name;
        }

        if(options & PlaylistAction::KeepActive) {
            const auto* activePlaylist = playlistHandler->activePlaylist();

            if(!activePlaylist || activePlaylist->name() != newName) {
                auto* playlist = playlistHandler->createPlaylist(newName, selection.tracks);
                handleActions(playlist, options);
                return;
            }
            const QString keepActiveName = newName + QStringLiteral(" (") + tr("Playback") + QStringLiteral(")");

            if(auto* keepActivePlaylist = playlistHandler->playlistByName(keepActiveName)) {
                keepActivePlaylist->changeCurrentIndex(activePlaylist->currentTrackIndex());
                playlistHandler->createPlaylist(keepActivePlaylist->name(), activePlaylist->tracks());

                playlistHandler->changeActivePlaylist(keepActivePlaylist->id());
            }
            else {
                playlistHandler->renamePlaylist(activePlaylist->id(), keepActiveName);
            }
        }

        auto* playlist = playlistHandler->createPlaylist(newName, selection.tracks);
        handleActions(playlist, options);
        QMetaObject::invokeMethod(self, "actionExecuted", Q_ARG(TrackAction, TrackAction::SendNewPlaylist));
    }

    void sendToCurrentPlaylist(PlaylistAction::ActionOptions options = {}) const
    {
        if(self->hasTracks()) {
            const auto& selection = contextSelection.at(activeContext);
            if(auto* currentPlaylist = playlistController->currentPlaylist()) {
                playlistHandler->createPlaylist(currentPlaylist->name(), selection.tracks);
                handleActions(currentPlaylist, options);
                QMetaObject::invokeMethod(self, "actionExecuted", Q_ARG(TrackAction, TrackAction::SendCurrentPlaylist));
            }
        }
    }

    void addToCurrentPlaylist() const
    {
        if(self->hasTracks()) {
            const auto& selection = contextSelection.at(activeContext);
            if(const auto* playlist = playlistController->currentPlaylist()) {
                playlistHandler->appendToPlaylist(playlist->id(), selection.tracks);
                QMetaObject::invokeMethod(self, "actionExecuted", Q_ARG(TrackAction, TrackAction::AddCurrentPlaylist));
            }
        }
    }

    void addToActivePlaylist() const
    {
        if(self->hasTracks()) {
            const auto& selection = contextSelection.at(activeContext);
            if(const auto* playlist = playlistHandler->activePlaylist()) {
                playlistHandler->appendToPlaylist(playlist->id(), selection.tracks);
                QMetaObject::invokeMethod(self, "actionExecuted", Q_ARG(TrackAction, TrackAction::AddActivePlaylist));
            }
        }
    }

    void updateActionState()
    {
        const bool haveTracks = activeContext && contextSelection.contains(activeContext)
                             && !contextSelection.at(activeContext).tracks.empty();

        auto allTracksInSameFolder = [this]() {
            const auto& selection   = contextSelection.at(activeContext);
            const QString firstPath = QFileInfo{selection.tracks.front().filepath()}.absolutePath();
            return std::ranges::all_of(selection.tracks | std::ranges::views::transform([](const Track& track) {
                                           return QFileInfo{track.filepath()}.absolutePath();
                                       }),
                                       [&firstPath](const QString& folderPath) { return folderPath == firstPath; });
        };

        addCurrent->setEnabled(haveTracks);
        addActive->setEnabled(haveTracks && playlistHandler->activePlaylist());
        sendCurrent->setEnabled(haveTracks);
        sendNew->setEnabled(haveTracks);
        openFolder->setEnabled(haveTracks && allTracksInSameFolder());
        openProperties->setEnabled(haveTracks);
        addToQueue->setEnabled(haveTracks);
    }
};

TrackSelectionController::TrackSelectionController(ActionManager* actionManager, SettingsManager* settings,
                                                   PlaylistController* playlistController, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, actionManager, settings, playlistController)}
{
    QObject::connect(qApp, &QApplication::focusChanged, this,
                     [this](QWidget* /*old*/, QWidget* now) { p->updateActiveContext(now); });
}

bool TrackSelectionController::hasTracks() const
{
    return p->activeContext && p->contextSelection.contains(p->activeContext)
        && !p->contextSelection.at(p->activeContext).tracks.empty();
}

TrackSelectionController::~TrackSelectionController() = default;

TrackList TrackSelectionController::selectedTracks() const
{
    if(!p->activeContext || !p->contextSelection.contains(p->activeContext)) {
        return {};
    }

    return p->contextSelection.at(p->activeContext).tracks;
}

void TrackSelectionController::changeSelectedTracks(WidgetContext* context, int index, const TrackList& tracks,
                                                    const QString& title)
{
    if(p->addContextObject(context)) {
        auto& selection      = p->contextSelection[context];
        selection.firstIndex = index;
        selection.name       = title;

        if(!tracks.empty()) {
            p->activeContext = context;
        }

        if(std::exchange(selection.tracks, tracks) == tracks) {
            return;
        }

        p->updateActionState();
        emit selectionChanged();
    }
}

void TrackSelectionController::changeSelectedTracks(WidgetContext* context, const TrackList& tracks,
                                                    const QString& title)
{
    changeSelectedTracks(context, 0, tracks, title);
}

void TrackSelectionController::addTrackContextMenu(QMenu* menu) const
{
    Utils::appendMenuActions(p->tracksMenu->menu(), menu);
}

void TrackSelectionController::addTrackQueueContextMenu(QMenu* menu) const
{
    Utils::appendMenuActions(p->tracksQueueMenu->menu(), menu);
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
            if(hasTracks()) {
                if(auto* playlist = p->playlistController->currentPlaylist()) {
                    const auto& selection = p->contextSelection.at(p->activeContext);
                    if(selection.firstIndex >= 0) {
                        playlist->changeCurrentIndex(selection.firstIndex);
                    }
                    p->playlistHandler->startPlayback(playlist->id());
                }
            }
            break;
        }
        case(TrackAction::None):
            break;
    }
}
} // namespace Fooyin

#include "gui/moc_trackselectioncontroller.cpp"
