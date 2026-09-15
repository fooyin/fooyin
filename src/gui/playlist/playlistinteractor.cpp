/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/playlist/playlistinteractor.h>

#include "playlistcontroller.h"
#include "playlistuicontroller.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guiutils.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/datastream.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QIODevice>
#include <QMainWindow>
#include <QPointer>

#include <unordered_set>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
int indexOfFile(const TrackList& tracks, const QUrl& file)
{
    const QString filepath = file.toLocalFile();
    if(filepath.isEmpty()) {
        return -1;
    }

    for(int index{0}; const auto& track : tracks) {
        if(Utils::File::isSamePath(track.filepath(), filepath)
           || (track.hasCue() && Utils::File::isSamePath(track.cuePath(), filepath))) {
            return index;
        }
        ++index;
    }

    return -1;
}

class TrackScanController : public QObject
{
    Q_OBJECT

public:
    TrackScanController(MusicLibrary* library, const QString& labelText, ScanRequest request,
                        std::function<void(const TrackList&)> callback, QObject* parent = nullptr)
        : QObject{parent}
        , m_request{std::move(request)}
        , m_callback{std::move(callback)}
        , m_dialog{
              new ElapsedProgressDialog(labelText, PlaylistInteractor::tr("Abort"), 0, 100, Utils::getMainWindow())}
    {
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);
        m_dialog->setModal(true);
        m_dialog->setMinimumDuration(500ms);
        m_dialog->startTimer();

        QObject::connect(library, &MusicLibrary::scanProgress, this, &TrackScanController::handleProgress);
        QObject::connect(library, &MusicLibrary::tracksScanned, this, &TrackScanController::handleTracksScanned);
        QObject::connect(
            library, &MusicLibrary::scanFinished, this,
            [this](int id, ScanRequest::Type /*type*/, bool cancelled) { handleScanFinished(id, cancelled); });
    }

private:
    [[nodiscard]] bool checkScan(int id) const
    {
        return !m_completed && id == m_request.id;
    }

    void handleProgress(const ScanProgress& progress)
    {
        if(!checkScan(progress.id)) {
            return;
        }

        if(!m_dialog) {
            return;
        }

        if(m_dialog->wasCancelled()) {
            m_request.cancel();
            m_dialog->close();
            m_completed = true;
            deleteLater();
            return;
        }

        const bool isIndeterminate = progress.total <= 0;
        m_dialog->setBusy(isIndeterminate);

        if(!isIndeterminate) {
            m_dialog->setValue(progress.percentage());
        }

        if(!progress.file.isEmpty()) {
            m_dialog->setText(PlaylistInteractor::tr("Current file") + ":\n"_L1 + progress.file);
        }
    }

    void handleScanFinished(int id, bool cancelled)
    {
        if(!checkScan(id)) {
            return;
        }

        m_completed = true;

        if(m_dialog) {
            m_dialog->close();
        }

        if(!cancelled) {
            m_callback(m_tracks);
        }

        deleteLater();
    }

    void handleTracksScanned(int id, const TrackList& scannedTracks)
    {
        if(!checkScan(id)) {
            return;
        }

        m_tracks.insert(m_tracks.end(), scannedTracks.cbegin(), scannedTracks.cend());
    }

    ScanRequest m_request;
    std::function<void(const TrackList&)> m_callback;
    QPointer<ElapsedProgressDialog> m_dialog;
    TrackList m_tracks;
    bool m_completed{false};
};
} // namespace

class PlaylistInteractorPrivate
{
public:
    PlaylistInteractorPrivate(PlaylistInteractor* self, PlaylistHandler* handler, PlaylistController* controller,
                              MusicLibrary* library, SettingsManager* settings);

    [[nodiscard]] ScanRequest startFileScan(const QList<QUrl>& urls) const;
    [[nodiscard]] ScanRequest startTrackScan(const TrackList& tracks) const;
    [[nodiscard]] ScanRequest startPlaylistLoad(const QList<QUrl>& urls) const;

    void beginTrackScan(const QString& labelText, const ScanRequest& request,
                        std::function<void(const TrackList&)> func);

    void activatePlaylist(Playlist* playlist, bool play = false) const;
    void activatePlaylist(Playlist* playlist, int indexToPlay, bool play = false) const;
    void appendToPlaylist(Playlist* playlist, const TrackList& tracks) const;

    [[nodiscard]] TrackList filterDuplicateTracks(const Playlist* playlist, const TrackList& tracks) const;
    [[nodiscard]] Playlist* appendOrCreateNamedPlaylist(const QString& playlistName, const TrackList& tracks,
                                                        bool preventDuplicates = false) const;
    void tracksToNewPlaylist(const QString& playlistName, const TrackList& tracks, int indexToPlay, bool replace,
                             bool play = false);

    void scanTracks(const TrackList& tracks, std::function<void(const TrackList&)> func);
    void scanFiles(const QList<QUrl>& urls, std::function<void(const TrackList&)> func);

    void loadPlaylistTracks(const QList<QUrl>& urls, std::function<void(const TrackList&)> func);

    PlaylistInteractor* m_self;
    PlaylistHandler* m_handler;
    PlaylistController* m_controller;
    MusicLibrary* m_library;
    SettingsManager* m_settings;
};

PlaylistInteractorPrivate::PlaylistInteractorPrivate(PlaylistInteractor* self, PlaylistHandler* handler,
                                                     PlaylistController* controller, MusicLibrary* library,
                                                     SettingsManager* settings)
    : m_self{self}
    , m_handler{handler}
    , m_controller{controller}
    , m_library{library}
    , m_settings{settings}
{ }

ScanRequest PlaylistInteractorPrivate::startFileScan(const QList<QUrl>& urls) const
{
    return m_library->scanFiles(urls);
}

ScanRequest PlaylistInteractorPrivate::startTrackScan(const TrackList& tracks) const
{
    return m_library->scanTracks(tracks);
}

ScanRequest PlaylistInteractorPrivate::startPlaylistLoad(const QList<QUrl>& urls) const
{
    return m_library->loadPlaylist(urls);
}

void PlaylistInteractorPrivate::beginTrackScan(const QString& labelText, const ScanRequest& request,
                                               std::function<void(const TrackList&)> func)
{
    auto* controller = new TrackScanController(m_library, labelText, request, std::move(func), m_self);
    controller->setObjectName(u"TrackScanController"_s);
}

void PlaylistInteractorPrivate::activatePlaylist(Playlist* playlist, const bool play) const
{
    if(!playlist) {
        return;
    }

    m_controller->changeCurrentPlaylist(playlist);
    if(play) {
        m_controller->playerController()->startPlayback(playlist);
        m_controller->uiController()->showNowPlaying();
    }
}

void PlaylistInteractorPrivate::activatePlaylist(Playlist* playlist, const int indexToPlay, const bool play) const
{
    if(!playlist) {
        return;
    }

    playlist->changeCurrentIndex(indexToPlay);
    activatePlaylist(playlist, play);
}

void PlaylistInteractorPrivate::appendToPlaylist(Playlist* playlist, const TrackList& tracks) const
{
    if(!playlist || tracks.empty()) {
        return;
    }

    m_handler->appendToPlaylist(playlist->id(), tracks);
}

TrackList PlaylistInteractorPrivate::filterDuplicateTracks(const Playlist* playlist, const TrackList& tracks) const
{
    if(!playlist || !m_settings->value<Settings::Core::PlaylistPreventDuplicates>()) {
        return tracks;
    }

    std::unordered_set<QString> seenTracks;
    seenTracks.reserve(static_cast<size_t>(playlist->trackCount()) + tracks.size());

    const auto playlistTracks = playlist->playlistTracks();
    for(const PlaylistTrack& track : playlistTracks) {
        seenTracks.emplace(track.track.uniqueFilepath());
    }

    TrackList filteredTracks;
    filteredTracks.reserve(tracks.size());

    for(const Track& track : tracks) {
        if(seenTracks.emplace(track.uniqueFilepath()).second) {
            filteredTracks.push_back(track);
        }
    }

    return filteredTracks;
}

Playlist* PlaylistInteractorPrivate::appendOrCreateNamedPlaylist(const QString& playlistName, const TrackList& tracks,
                                                                 const bool preventDuplicates) const
{
    if(tracks.empty()) {
        return nullptr;
    }

    if(Playlist* playlist = m_handler->playlistByName(playlistName)) {
        const TrackList filteredTracks = preventDuplicates ? filterDuplicateTracks(playlist, tracks) : tracks;
        if(filteredTracks.empty()) {
            return playlist;
        }

        const int indexToPlay = playlist->trackCount();
        appendToPlaylist(playlist, filteredTracks);
        playlist->changeCurrentIndex(indexToPlay);
        return playlist;
    }

    return m_handler->createPlaylist(playlistName, tracks);
}

void PlaylistInteractorPrivate::tracksToNewPlaylist(const QString& playlistName, const TrackList& tracks,
                                                    int indexToPlay, bool replace, bool play)
{
    if(tracks.empty()) {
        return;
    }

    indexToPlay = std::max(indexToPlay, 0);

    if(replace) {
        activatePlaylist(m_handler->createPlaylist(playlistName, tracks), indexToPlay, play);
        return;
    }

    if(auto* playlist = m_handler->playlistByName(playlistName)) {
        const int firstAddedIndex = playlist->trackCount();
        appendToPlaylist(playlist, tracks);
        activatePlaylist(playlist, firstAddedIndex + indexToPlay, play);
        return;
    }

    activatePlaylist(m_handler->createPlaylist(playlistName, tracks), indexToPlay, play);
}

void PlaylistInteractorPrivate::scanTracks(const TrackList& tracks, std::function<void(const TrackList&)> func)
{
    if(!tracks.empty()) {
        beginTrackScan(PlaylistInteractor::tr("Reading tracks…"), startTrackScan(tracks), std::move(func));
    }
}

void PlaylistInteractorPrivate::scanFiles(const QList<QUrl>& urls, std::function<void(const TrackList&)> func)
{
    beginTrackScan(PlaylistInteractor::tr("Reading tracks…"), startFileScan(urls), std::move(func));
}

void PlaylistInteractorPrivate::loadPlaylistTracks(const QList<QUrl>& urls, std::function<void(const TrackList&)> func)
{
    beginTrackScan(PlaylistInteractor::tr("Loading playlist…"), startPlaylistLoad(urls), std::move(func));
}

PlaylistInteractor::PlaylistInteractor(PlaylistHandler* handler, PlaylistController* controller, MusicLibrary* library,
                                       SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<PlaylistInteractorPrivate>(this, handler, controller, library, settings)}
{ }

PlaylistInteractor::~PlaylistInteractor() = default;

PlaylistHandler* PlaylistInteractor::handler() const
{
    return p->m_handler;
}

PlaylistController* PlaylistInteractor::playlistController() const
{
    return p->m_controller;
}

MusicLibrary* PlaylistInteractor::library() const
{
    return p->m_library;
}

PlayerController* PlaylistInteractor::playerController() const
{
    return p->m_controller->playerController();
}

void PlaylistInteractor::filesToPlaylist(const QList<QUrl>& urls, const UId& id)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this, id](const TrackList& scannedTracks) { tracksToPlaylist(scannedTracks, id); });
}

void PlaylistInteractor::filesToCurrentPlaylist(const QList<QUrl>& urls)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this](const TrackList& scannedTracks) { tracksToCurrentPlaylist(scannedTracks); });
}

void PlaylistInteractor::filesToCurrentPlaylistAndPlayIfStopped(const QList<QUrl>& urls)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls,
                 [this](const TrackList& scannedTracks) { tracksToCurrentPlaylistAndPlayIfStopped(scannedTracks); });
}

void PlaylistInteractor::filesToCurrentPlaylistReplace(const QList<QUrl>& urls, bool play)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls,
                 [this, play](const TrackList& scannedTracks) { tracksToCurrentPlaylistReplace(scannedTracks, play); });
}

void PlaylistInteractor::filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, bool play)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this, playlistName, play](const TrackList& scannedTracks) {
        tracksToNewPlaylist(playlistName, scannedTracks, play);
    });
}

void PlaylistInteractor::filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls,
                                            const QUrl& fileToPlay, bool replace, bool play)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this, playlistName, fileToPlay, replace, play](const TrackList& scannedTracks) {
        p->tracksToNewPlaylist(playlistName, scannedTracks, indexOfFile(scannedTracks, fileToPlay), replace, play);
    });
}

void PlaylistInteractor::filesToNewPlaylistReplace(const QString& playlistName, const QList<QUrl>& urls, bool play)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this, playlistName, play](const TrackList& scannedTracks) {
        tracksToNewPlaylistReplace(playlistName, scannedTracks, play);
    });
}

void PlaylistInteractor::filesToActivePlaylist(const QList<QUrl>& urls)
{
    if(!p->m_handler->activePlaylist()) {
        return;
    }

    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this](const TrackList& scannedTracks) { tracksToActivePlaylist(scannedTracks); });
}

void PlaylistInteractor::loadPlaylist(const QList<QPair<QString, QUrl>>& playlistData, bool play)
{
    if(playlistData.empty()) {
        return;
    }

    for(const QPair<QString, QUrl>& item : playlistData) {
        auto [name, url]      = item;
        auto handleScanResult = [this, name, play](const TrackList& scannedTracks) {
            p->activatePlaylist(p->appendOrCreateNamedPlaylist(name, scannedTracks, true), play);
        };
        p->loadPlaylistTracks({url}, handleScanResult);
    }
}

void PlaylistInteractor::tracksToPlaylist(const TrackList& tracks, const UId& id)
{
    if(tracks.empty()) {
        return;
    }

    if(id.isValid()) {
        if(auto* playlist = p->m_handler->playlistById(id)) {
            p->appendToPlaylist(playlist, tracks);
            p->activatePlaylist(playlist);
        }
    }
    else {
        const QString playlistName = Track::findCommonField(tracks);
        p->activatePlaylist(p->m_handler->createNewPlaylist(playlistName, tracks));
    }
}

void PlaylistInteractor::tracksToCurrentPlaylist(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    p->appendToPlaylist(p->m_controller->currentPlaylist(), tracks);
}

void PlaylistInteractor::tracksToCurrentPlaylistAndPlayIfStopped(const TrackList& tracks)
{
    if(tracks.empty()) {
        return;
    }

    auto* playlist = p->m_controller->currentPlaylist();
    if(!playlist || playlist->isAutoPlaylist()) {
        return;
    }

    const int firstAddedIndex = playlist->trackCount();
    p->appendToPlaylist(playlist, tracks);

    if(playerController()->playState() == Player::PlayState::Stopped) {
        p->activatePlaylist(playlist, firstAddedIndex, true);
    }
}

void PlaylistInteractor::tracksToCurrentPlaylistReplace(const TrackList& tracks, bool play)
{
    if(tracks.empty()) {
        return;
    }

    if(auto* playlist = p->m_controller->currentPlaylist()) {
        p->m_handler->replacePlaylistTracks(playlist->id(), tracks);
        p->activatePlaylist(playlist, 0, play);
    }
}

void PlaylistInteractor::tracksToNewPlaylist(const QString& playlistName, const TrackList& tracks, bool play)
{
    if(tracks.empty()) {
        return;
    }

    p->activatePlaylist(p->appendOrCreateNamedPlaylist(playlistName, tracks), play);
}

void PlaylistInteractor::tracksToNewPlaylistReplace(const QString& playlistName, const TrackList& tracks, bool play)
{
    if(tracks.empty()) {
        return;
    }

    p->activatePlaylist(p->m_handler->createPlaylist(playlistName, tracks), play);
}

void PlaylistInteractor::tracksToActivePlaylist(const TrackList& tracks)
{
    if(!p->m_handler->activePlaylist()) {
        return;
    }

    if(tracks.empty()) {
        return;
    }

    p->appendToPlaylist(p->m_handler->activePlaylist(), tracks);
}

void PlaylistInteractor::filesToTracks(const QList<QUrl>& urls, std::function<void(const TrackList&)> func)
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, std::move(func));
}

void PlaylistInteractor::playlistFilesToTracks(const QList<QUrl>& urls, std::function<void(const TrackList&)> func)
{
    if(urls.empty()) {
        return;
    }

    p->loadPlaylistTracks(urls, std::move(func));
}

void PlaylistInteractor::trackIdsToPlaylist(const QByteArray& data, const UId& id)
{
    tracksToPlaylist(Gui::tracksFromMimeData(p->m_library, data), id);
}
} // namespace Fooyin

#include "gui/playlist/moc_playlistinteractor.cpp"
#include "playlistinteractor.moc"
