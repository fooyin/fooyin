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
    bool playbackOnSend{false};
};

struct TrackSelectionController::Private
{
    TrackSelectionController* m_self;

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    PlaylistController* m_playlistController;
    PlaylistHandler* m_playlistHandler;

    std::unordered_map<QWidget*, WidgetContext*> m_contextWidgets;
    std::unordered_map<WidgetContext*, WidgetSelection> m_contextSelection;
    WidgetContext* m_activeContext{nullptr};

    ActionContainer* m_tracksMenu{nullptr};
    ActionContainer* m_tracksQueueMenu{nullptr};
    ActionContainer* m_tracksPlaylistMenu{nullptr};

    QAction* m_addCurrent;
    QAction* m_addActive;
    QAction* m_sendCurrent;
    QAction* m_sendNew;
    QAction* m_addToQueue;
    QAction* m_removeFromQueue;
    QAction* m_openFolder;
    QAction* m_openProperties;

    Private(TrackSelectionController* self, ActionManager* actionManager, SettingsManager* settings,
            PlaylistController* playlistController)
        : m_self{self}
        , m_actionManager{actionManager}
        , m_settings{settings}
        , m_playlistController{playlistController}
        , m_playlistHandler{m_playlistController->playlistHandler()}
        , m_tracksMenu{m_actionManager->createMenu(Constants::Menus::Context::TrackSelection)}
        , m_tracksQueueMenu{m_actionManager->createMenu(Constants::Menus::Context::TrackSelection)}
        , m_tracksPlaylistMenu{m_actionManager->createMenu(Constants::Menus::Context::TracksPlaylist)}
        , m_addCurrent{new QAction(tr("Add to current playlist"), m_tracksPlaylistMenu)}
        , m_addActive{new QAction(tr("Add to active playlist"), m_tracksPlaylistMenu)}
        , m_sendCurrent{new QAction(tr("Send to current playlist"), m_tracksPlaylistMenu)}
        , m_sendNew{new QAction(tr("Send to new playlist"), m_tracksPlaylistMenu)}
        , m_addToQueue{new QAction(tr("Add to Playback Queue"), m_tracksMenu)}
        , m_removeFromQueue{new QAction(tr("Remove from Playback Queue"), m_tracksMenu)}
        , m_openFolder{new QAction(tr("Open Containing Folder"), m_tracksMenu)}
        , m_openProperties{new QAction(tr("Properties"), m_tracksMenu)}
    {
        // Playlist menu
        m_tracksPlaylistMenu->addSeparator();

        QObject::connect(m_addCurrent, &QAction::triggered, m_tracksPlaylistMenu, [this]() { addToCurrentPlaylist(); });
        m_tracksPlaylistMenu->addAction(
            m_actionManager->registerAction(m_addCurrent, "TrackSelection.AddCurrentPlaylist"));

        QObject::connect(m_addActive, &QAction::triggered, m_tracksPlaylistMenu, [this]() { addToActivePlaylist(); });
        m_tracksPlaylistMenu->addAction(
            m_actionManager->registerAction(m_addActive, "TrackSelection.AddActivePlaylist"));

        QObject::connect(m_sendCurrent, &QAction::triggered, m_tracksPlaylistMenu, [this]() {
            if(m_self->hasTracks()) {
                const auto& selection = m_contextSelection.at(m_activeContext);
                sendToCurrentPlaylist(selection.playbackOnSend ? PlaylistAction::StartPlayback
                                                               : PlaylistAction::Switch);
            }
        });
        m_tracksPlaylistMenu->addAction(
            m_actionManager->registerAction(m_sendCurrent, "TrackSelection.SendCurrentPlaylist"));

        QObject::connect(m_sendNew, &QAction::triggered, m_tracksPlaylistMenu, [this]() {
            if(m_self->hasTracks()) {
                const auto& selection = m_contextSelection.at(m_activeContext);
                const auto options
                    = PlaylistAction::Switch
                    | (selection.playbackOnSend ? PlaylistAction::StartPlayback : PlaylistAction::Switch);
                sendToNewPlaylist(options, selection.name);
            }
        });
        m_tracksPlaylistMenu->addAction(m_actionManager->registerAction(m_sendNew, "TrackSelection.SendNewPlaylist"));

        m_tracksPlaylistMenu->addSeparator();

        // Tracks menu
        m_tracksMenu->addSeparator();

        QObject::connect(m_addToQueue, &QAction::triggered, m_tracksQueueMenu, [this]() {
            if(m_self->hasTracks()) {
                const auto& selection = m_contextSelection.at(m_activeContext);
                m_playlistController->playerController()->queueTracks(selection.tracks);
            }
        });
        m_tracksQueueMenu->addAction(m_actionManager->registerAction(m_addToQueue, Constants::Actions::AddToQueue));

        QObject::connect(m_removeFromQueue, &QAction::triggered, m_tracksQueueMenu, [this]() {
            if(m_self->hasTracks()) {
                const auto& selection = m_contextSelection.at(m_activeContext);
                m_playlistController->playerController()->dequeueTracks(selection.tracks);
            }
        });
        m_tracksQueueMenu->addAction(
            m_actionManager->registerAction(m_removeFromQueue, Constants::Actions::RemoveFromQueue));

        QObject::connect(m_openFolder, &QAction::triggered, m_tracksMenu, [this]() {
            if(m_self->hasTracks()) {
                const auto& selection = m_contextSelection.at(m_activeContext);
                const QString dir     = QFileInfo{selection.tracks.front().filepath()}.absolutePath();
                Utils::File::openDirectory(dir);
            }
        });
        m_tracksMenu->addAction(m_actionManager->registerAction(m_openFolder, "TrackSelection.OpenFolder"));

        m_tracksMenu->addSeparator(Actions::Groups::Three);

        QObject::connect(m_openProperties, &QAction::triggered, m_self, [this]() {
            QMetaObject::invokeMethod(m_self, &TrackSelectionController::requestPropertiesDialog);
        });
        m_tracksMenu->addAction(m_actionManager->registerAction(m_openProperties, "TrackSelection.OpenProperties"),
                                Actions::Groups::Three);

        updateActionState();
    }

    WidgetContext* contextObject(QWidget* widget) const
    {
        if(m_contextWidgets.contains(widget)) {
            return m_contextWidgets.at(widget);
        }
        return nullptr;
    }

    bool addContextObject(WidgetContext* context)
    {
        if(!context) {
            return false;
        }

        QWidget* widget = context->widget();
        if(m_contextWidgets.contains(widget)) {
            return true;
        }

        m_contextWidgets.emplace(widget, context);
        QObject::connect(context, &QObject::destroyed, m_self, [this, context] { removeContextObject(context); });

        return true;
    }

    void removeContextObject(WidgetContext* context)
    {
        if(!context) {
            return;
        }

        QObject::disconnect(context, &QObject::destroyed, m_self, nullptr);

        if(!std::erase_if(m_contextWidgets, [context](const auto& v) { return v.second == context; })) {
            return;
        }

        m_contextSelection.erase(context);

        if(m_activeContext == context) {
            m_activeContext = nullptr;
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
                    m_activeContext = widgetContext;
                    updateActionState();
                    QMetaObject::invokeMethod(m_self, &TrackSelectionController::selectionChanged);
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
            m_playlistController->changeCurrentPlaylist(playlist);
        }
        if(options & PlaylistAction::StartPlayback) {
            m_playlistHandler->startPlayback(playlist);
        }
    }

    void sendToNewPlaylist(PlaylistAction::ActionOptions options = {}, const QString& playlistName = {}) const
    {
        if(!m_self->hasTracks()) {
            return;
        }

        QString newName{playlistName};

        const auto& selection = m_contextSelection.at(m_activeContext);

        if(newName.isEmpty()) {
            newName = selection.name;
        }

        if(options & PlaylistAction::KeepActive) {
            const auto* activePlaylist = m_playlistHandler->activePlaylist();

            if(!activePlaylist || activePlaylist->name() != newName) {
                auto* playlist = m_playlistHandler->createPlaylist(newName, selection.tracks);
                handleActions(playlist, options);
                return;
            }
            const QString keepActiveName = newName + QStringLiteral(" (") + tr("Playback") + QStringLiteral(")");

            if(auto* keepActivePlaylist = m_playlistHandler->playlistByName(keepActiveName)) {
                m_playlistHandler->movePlaylistTracks(activePlaylist->id(), keepActivePlaylist->id());
            }
            else {
                m_playlistHandler->renamePlaylist(activePlaylist->id(), keepActiveName);
            }
        }

        auto* playlist = m_playlistHandler->createPlaylist(newName, selection.tracks);
        playlist->changeCurrentIndex(-1);
        handleActions(playlist, options);
        emit m_self->actionExecuted(TrackAction::SendNewPlaylist);
    }

    void sendToCurrentPlaylist(PlaylistAction::ActionOptions options = {}) const
    {
        if(m_self->hasTracks()) {
            const auto& selection = m_contextSelection.at(m_activeContext);
            if(auto* currentPlaylist = m_playlistController->currentPlaylist()) {
                m_playlistHandler->createPlaylist(currentPlaylist->name(), selection.tracks);
                handleActions(currentPlaylist, options);
                emit m_self->actionExecuted(TrackAction::SendCurrentPlaylist);
            }
        }
    }

    void addToCurrentPlaylist() const
    {
        if(m_self->hasTracks()) {
            const auto& selection = m_contextSelection.at(m_activeContext);
            if(const auto* playlist = m_playlistController->currentPlaylist()) {
                m_playlistHandler->appendToPlaylist(playlist->id(), selection.tracks);
                emit m_self->actionExecuted(TrackAction::AddCurrentPlaylist);
            }
        }
    }

    void addToActivePlaylist() const
    {
        if(m_self->hasTracks()) {
            const auto& selection = m_contextSelection.at(m_activeContext);
            if(const auto* playlist = m_playlistHandler->activePlaylist()) {
                m_playlistHandler->appendToPlaylist(playlist->id(), selection.tracks);
                emit m_self->actionExecuted(TrackAction::AddActivePlaylist);
            }
        }
    }

    void updateActionState()
    {
        const bool haveTracks = m_activeContext && m_contextSelection.contains(m_activeContext)
                             && !m_contextSelection.at(m_activeContext).tracks.empty();

        auto allTracksInSameFolder = [this]() {
            const auto& selection   = m_contextSelection.at(m_activeContext);
            const QString firstPath = QFileInfo{selection.tracks.front().filepath()}.absolutePath();
            return std::ranges::all_of(selection.tracks | std::ranges::views::transform([](const Track& track) {
                                           return QFileInfo{track.filepath()}.absolutePath();
                                       }),
                                       [&firstPath](const QString& folderPath) { return folderPath == firstPath; });
        };

        m_addCurrent->setEnabled(haveTracks);
        m_addActive->setEnabled(haveTracks && m_playlistHandler->activePlaylist());
        m_sendCurrent->setEnabled(haveTracks);
        m_sendNew->setEnabled(haveTracks);
        m_openFolder->setEnabled(haveTracks && allTracksInSameFolder());
        m_openProperties->setEnabled(haveTracks);
        m_addToQueue->setEnabled(haveTracks);
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

TrackSelectionController::~TrackSelectionController() = default;

bool TrackSelectionController::hasTracks() const
{
    return p->m_activeContext && p->m_contextSelection.contains(p->m_activeContext)
        && !p->m_contextSelection.at(p->m_activeContext).tracks.empty();
}

Track TrackSelectionController::selectedTrack() const
{
    const auto selected = selectedTracks();
    if(!selected.empty()) {
        return selected.front();
    }

    return {};
}

TrackList TrackSelectionController::selectedTracks() const
{
    if(!p->m_activeContext || !p->m_contextSelection.contains(p->m_activeContext)) {
        return {};
    }

    return p->m_contextSelection.at(p->m_activeContext).tracks;
}

int TrackSelectionController::selectedTrackCount() const
{
    if(!p->m_activeContext || !p->m_contextSelection.contains(p->m_activeContext)) {
        return 0;
    }

    return static_cast<int>(p->m_contextSelection.at(p->m_activeContext).tracks.size());
}

void TrackSelectionController::changeSelectedTracks(WidgetContext* context, int index, const TrackList& tracks,
                                                    const QString& title)
{
    if(p->addContextObject(context)) {
        auto& selection      = p->m_contextSelection[context];
        selection.firstIndex = index;
        selection.name       = title;

        if(!tracks.empty()) {
            p->m_activeContext = context;
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

void TrackSelectionController::changePlaybackOnSend(WidgetContext* context, bool enabled)
{
    if(p->addContextObject(context)) {
        auto& selection          = p->m_contextSelection[context];
        selection.playbackOnSend = enabled;
    }
}

void TrackSelectionController::addTrackContextMenu(QMenu* menu) const
{
    Utils::appendMenuActions(p->m_tracksMenu->menu(), menu);
}

void TrackSelectionController::addTrackQueueContextMenu(QMenu* menu) const
{
    Utils::appendMenuActions(p->m_tracksQueueMenu->menu(), menu);
}

void TrackSelectionController::addTrackPlaylistContextMenu(QMenu* menu) const
{
    Utils::appendMenuActions(p->m_tracksPlaylistMenu->menu(), menu);
}

void TrackSelectionController::executeAction(TrackAction action, PlaylistAction::ActionOptions options,
                                             const QString& playlistName)
{
    switch(action) {
        case(TrackAction::SendCurrentPlaylist):
            p->sendToCurrentPlaylist(options);
            break;
        case(TrackAction::SendNewPlaylist):
            p->sendToNewPlaylist(options, playlistName);
            break;
        case(TrackAction::AddCurrentPlaylist):
            p->addToCurrentPlaylist();
            break;
        case(TrackAction::AddActivePlaylist):
            p->addToActivePlaylist();
            break;
        case(TrackAction::Play): {
            if(hasTracks()) {
                if(auto* playlist = p->m_playlistController->currentPlaylist()) {
                    const auto& selection = p->m_contextSelection.at(p->m_activeContext);
                    if(selection.firstIndex >= 0) {
                        playlist->changeCurrentIndex(selection.firstIndex);
                    }
                    p->m_playlistHandler->startPlayback(playlist->id());
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
