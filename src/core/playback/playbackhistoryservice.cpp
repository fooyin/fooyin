/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <core/player/playbackhistoryservice.h>

#include "core/database/librarydatabase.h"
#include "core/database/playbackhistorydatabase.h"
#include "core/database/trackdatabase.h"

#include <core/constants.h>
#include <core/player/playbacklisteningtracker.h>
#include <core/player/playercontroller.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>
#include <utils/async.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/stringcollator.h>

#include <QDateTime>
#include <QTimerEvent>
#include <QUrl>

#include <map>
#include <ranges>

using namespace Qt::StringLiterals;

constexpr int CheckpointIntervalMs = 30000;

namespace Fooyin {
namespace {
qint64 currentTimeMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString rawStreamTitle(const Track& track)
{
    for(const auto& tag : {u"STREAMTITLE"_s, u"ICY_TITLE"_s}) {
        const QString value = track.extraTag(tag).join(QLatin1StringView{Constants::UnitSeparator});
        if(!value.isEmpty()) {
            return value;
        }
    }
    return {};
}
} // namespace

PlaybackHistoryService::PlaybackHistoryService(PlayerController* playerController, DbConnectionPoolPtr dbPool,
                                               QObject* parent)
    : QObject{parent}
    , m_playerController{playerController}
    , m_dbPool{std::move(dbPool)}
    , m_database{std::make_unique<PlaybackHistoryDatabase>()}
    , m_currentItemId{0}
    , m_trackingMode{ListeningHistoryMode::FullHistory}
    , m_totalsCheckpointMs{0}
    , m_totalsSessionId{-1}
    , m_shutdown{false}
{
    m_database->initialise(DbConnectionProvider{m_dbPool});
    recoverOpenEvents();

    const auto* tracker = m_playerController->listeningTracker();
    QObject::connect(tracker, &PlaybackListeningTracker::sessionStarted, this,
                     [this](const ListeningSession& session) { startEvent(session.itemId, session.track); });
    QObject::connect(tracker, &PlaybackListeningTracker::sessionEnded, this, &PlaybackHistoryService::finaliseEvent);
    QObject::connect(tracker, &PlaybackListeningTracker::listeningTimeChanged, this,
                     [this](uint64_t itemId, uint64_t deltaMs) {
                         if(m_currentEvent && itemId == m_currentItemId) {
                             m_currentEvent->listeningTimeMs += deltaMs;
                             m_currentEvent->lastCheckpointAt = currentTimeMs();
                         }
                     });
    QObject::connect(m_playerController, &PlayerController::trackPlayed, this, [this](const Track& track) {
        if(m_trackingMode == ListeningHistoryMode::FullHistory && m_currentEvent && !track.isRemote()
           && !m_currentEvent->playedAt) {
            m_currentEvent->playedAt = currentTimeMs();
            checkpointEvent();
        }
    });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     &PlaybackHistoryService::handlePlayState);
    QObject::connect(tracker, &PlaybackListeningTracker::metadataChanged, this,
                     [this](const ListeningSession& session) {
                         if(m_currentEvent && session.track.isRemote()) {
                             updateStreamMetadata(session.track, true);
                         }
                     });
}

PlaybackHistoryService::~PlaybackHistoryService()
{
    shutdown();
}

PlaybackSourceKind PlaybackHistoryService::currentSourceKind() const
{
    return m_playerController->currentTrack().isRemote() ? PlaybackSourceKind::RemoteStream
                                                         : PlaybackSourceKind::LocalTrack;
}

std::optional<PlaybackEvent> PlaybackHistoryService::currentEvent() const
{
    return m_currentEvent;
}

QFuture<PlaybackHistoryOverview> PlaybackHistoryService::queryOverview(PlaybackHistoryScope scope) const
{
    return queryOverview(scope, currentEvent());
}

QFuture<PlaybackHistoryOverview>
PlaybackHistoryService::queryOverview(PlaybackHistoryScope scope, const std::optional<PlaybackEvent>& liveEvent) const
{
    auto snapshot{liveEvent};
    if(snapshot && snapshot->id < -1) {
        snapshot->listeningTimeMs -= std::min(snapshot->listeningTimeMs, m_totalsCheckpointMs);
    }

    const qint64 generatedAt = currentTimeMs();
    return Utils::asyncExec([dbPool = m_dbPool, scope, generatedAt, liveEvent = std::move(snapshot)]() {
        const DbConnectionHandler connectionHandler{dbPool};
        PlaybackHistoryDatabase database;
        database.initialise(DbConnectionProvider{dbPool});
        return database.overview(scope, generatedAt, liveEvent);
    });
}

QFuture<PlaybackHistoryPage> PlaybackHistoryService::queryHistory(const PlaybackHistoryQuery& query) const
{
    return Utils::asyncExec([dbPool = m_dbPool, query, liveEvent = currentEvent()]() {
        const DbConnectionHandler connectionHandler{dbPool};
        PlaybackHistoryDatabase database;
        database.initialise(DbConnectionProvider{dbPool});
        return database.history(query, liveEvent);
    });
}

QFuture<std::vector<StreamMetadataSpan>> PlaybackHistoryService::queryMetadataSpans(qint64 eventId) const
{
    return Utils::asyncExec([dbPool = m_dbPool, eventId]() {
        const DbConnectionHandler connectionHandler{dbPool};
        PlaybackHistoryDatabase database;
        database.initialise(DbConnectionProvider{dbPool});
        return database.metadataSpans(eventId);
    });
}

QFuture<std::vector<PlaybackSourceRanking>>
PlaybackHistoryService::querySourceRankings(const PlaybackRankingQuery& query) const
{
    return querySourceRankings(query, currentEvent());
}

QFuture<std::vector<PlaybackSourceRanking>>
PlaybackHistoryService::querySourceRankings(const PlaybackRankingQuery& query,
                                            const std::optional<PlaybackEvent>& liveEvent) const
{
    return Utils::asyncExec([dbPool = m_dbPool, query, liveEvent]() {
        const DbConnectionHandler connectionHandler{dbPool};
        PlaybackHistoryDatabase database;
        database.initialise(DbConnectionProvider{dbPool});
        return database.sourceRankings(query, liveEvent);
    });
}

QFuture<LibraryStatistics> PlaybackHistoryService::queryLibraryStatistics() const
{
    return Utils::asyncExec([dbPool = m_dbPool]() {
        const DbConnectionHandler connectionHandler{dbPool};
        PlaybackHistoryDatabase database;
        database.initialise(DbConnectionProvider{dbPool});
        return database.libraryStatistics();
    });
}

QFuture<std::vector<LibraryGroupStatistics>> PlaybackHistoryService::queryLibraryBreakdown(const QString& script) const
{
    return Utils::asyncExec([dbPool = m_dbPool, script]() -> std::vector<LibraryGroupStatistics> {
        ScriptParser parser;
        const auto parsed = parser.parse(script);
        if(script.trimmed().isEmpty() || !parsed.isValid()) {
            return {};
        }

        const DbConnectionHandler connectionHandler{dbPool};
        const DbConnectionProvider provider{dbPool};

        TrackDatabase tracks;
        tracks.initialise(provider);
        LibraryDatabase libraryDatabase;
        libraryDatabase.initialise(provider);
        LibraryInfoMap libraries;
        libraryDatabase.getAllLibraries(libraries);

        const ScriptContext context{.libraries = &libraries};

        std::map<QString, LibraryGroupStatistics> grouped;
        for(const auto& track : tracks.getAllTracks()) {
            if(track.libraryId() < 0) {
                continue;
            }

            const QString name = parser.evaluate(parsed, track, context);
            auto& group        = grouped[name];
            group.name         = name;
            ++group.trackCount;
            group.totalDurationMs += track.duration();
            group.totalFileSize += track.fileSize();
            group.totalPlayCount += std::max(0, track.playCount());
        }

        std::vector<LibraryGroupStatistics> groups;
        groups.reserve(grouped.size());

        for(auto& group : grouped | std::views::values) {
            groups.push_back(std::move(group));
        }

        StringCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);

        std::ranges::sort(groups, [&collator](const auto& lhs, const auto& rhs) {
            return lhs.trackCount != rhs.trackCount ? lhs.trackCount > rhs.trackCount
                                                    : collator.compare(lhs.name, rhs.name) < 0;
        });

        return groups;
    });
}

void PlaybackHistoryService::setTrackingMode(int mode)
{
    const auto selected = static_cast<ListeningHistoryMode>(std::clamp(mode, 0, 2));
    if(m_shutdown || m_trackingMode == selected) {
        return;
    }

    finaliseEvent();
    m_trackingMode = selected;
    if(const auto session = m_playerController->listeningTracker()->currentSession()) {
        startEvent(session->itemId, session->track);
    }
}

bool PlaybackHistoryService::clearHistory()
{
    if(m_shutdown || !m_database->clearHistory()) {
        return false;
    }

    m_checkpointTimer.stop();
    m_listeningTimer.invalidate();
    m_currentEvent.reset();
    m_currentSpan.reset();
    m_currentItemId = 0;
    Q_EMIT historyCleared();

    if(const auto session = m_playerController->listeningTracker()->currentSession()) {
        startEvent(session->itemId, session->track);
    }

    return true;
}

void PlaybackHistoryService::playbackError()
{
    finaliseEvent();
}

void PlaybackHistoryService::shutdown()
{
    if(std::exchange(m_shutdown, true)) {
        return;
    }
    finaliseEvent();
}

void PlaybackHistoryService::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_checkpointTimer.timerId()) {
        checkpointEvent();
        return;
    }
    QObject::timerEvent(event);
}

void PlaybackHistoryService::recoverOpenEvents()
{
    const auto events = m_database->openEvents();
    for(PlaybackEvent event : events) {
        event.endedAt          = event.lastCheckpointAt > 0 ? event.lastCheckpointAt : event.startedAt;
        event.lastCheckpointAt = *event.endedAt;
        m_database->finaliseEvent(event);
    }
}

void PlaybackHistoryService::startEvent(uint64_t itemId, const Track& track)
{
    if(m_shutdown || m_trackingMode == ListeningHistoryMode::Off || !track.isValid()) {
        return;
    }

    const qint64 now{currentTimeMs()};
    if(m_trackingMode == ListeningHistoryMode::TotalsOnly) {
        PlaybackEvent event;
        // Keep a distinct live identity without writing a source or playback event
        event.id               = --m_totalsSessionId;
        event.startedAt        = now;
        event.lastCheckpointAt = now;
        event.filepath         = track.filepath();
        m_currentEvent         = std::move(event);
        m_currentItemId        = itemId;
        m_totalsCheckpointMs   = 0;
        m_checkpointTimer.start(CheckpointIntervalMs, this);
        m_listeningTimer.invalidate();
        Q_EMIT historyChanged(m_currentEvent->id);
        return;
    }

    const auto session = m_playerController->listeningTracker()->currentSession();
    if(!session) {
        return;
    }

    const qint64 sourceId = m_database->upsertSource(session->source);
    if(sourceId < 0) {
        return;
    }

    PlaybackEvent event;
    event.sourceId         = sourceId;
    event.startedAt        = now;
    event.lastCheckpointAt = now;
    event.title            = track.title();
    event.artists          = track.artists();
    event.album            = track.album();
    event.albumArtists     = track.albumArtists();
    event.genres           = track.genres();
    event.composers        = track.composers();
    event.date             = track.date();
    event.year             = track.year();
    event.trackNumber      = track.trackNumber();
    event.discNumber       = track.discNumber();
    event.filepath         = track.filepath();
    event.durationMs       = track.duration();
    event.codec            = track.codec();
    event.codecProfile     = track.codecProfile();
    event.encoding         = track.encoding();
    event.bitrate          = track.bitrate();
    event.bitDepth         = track.bitDepth();
    event.sampleRate       = track.sampleRate();
    event.id               = m_database->insertEvent(event);

    if(event.id < 0) {
        return;
    }

    m_currentEvent  = event;
    m_currentItemId = itemId;
    m_checkpointTimer.start(CheckpointIntervalMs, this);
    m_listeningTimer.invalidate();

    if(track.isRemote()) {
        updateStreamMetadata(track);
    }
    Q_EMIT historyChanged(m_currentEvent->id);
}

void PlaybackHistoryService::finaliseEvent()
{
    if(!m_currentEvent) {
        return;
    }

    updateListeningTime();
    m_listeningTimer.invalidate();
    m_checkpointTimer.stop();

    m_currentEvent->endedAt          = currentTimeMs();
    m_currentEvent->lastCheckpointAt = *m_currentEvent->endedAt;

    const auto eventId = m_currentEvent->id;
    const bool stored = m_trackingMode == ListeningHistoryMode::TotalsOnly ? checkpointEvent()
                                                                           : m_database->finaliseEvent(*m_currentEvent);

    m_currentEvent.reset();
    m_currentSpan.reset();
    m_currentItemId = 0;
    if(stored) {
        Q_EMIT historyChanged(eventId);
    }
}

bool PlaybackHistoryService::checkpointEvent()
{
    if(!m_currentEvent) {
        return false;
    }

    updateListeningTime();
    m_currentEvent->lastCheckpointAt = currentTimeMs();

    bool stored{false};
    if(m_trackingMode == ListeningHistoryMode::TotalsOnly) {
        const auto delta = m_currentEvent->listeningTimeMs - m_totalsCheckpointMs;
        const auto kind  = Track::isRemotePath(m_currentEvent->filepath) ? PlaybackSourceKind::RemoteStream
                                                                         : PlaybackSourceKind::LocalTrack;
        stored           = m_database->addListeningTime(kind, m_currentEvent->lastCheckpointAt, delta);
        if(stored) {
            m_totalsCheckpointMs = m_currentEvent->listeningTimeMs;
        }
    }
    else {
        stored = m_database->checkpointEvent(*m_currentEvent);
    }
    if(stored) {
        Q_EMIT historyChanged(m_currentEvent->id);
    }
    return stored;
}

void PlaybackHistoryService::updateListeningTime()
{
    if(m_currentEvent) {
        m_currentEvent->lastCheckpointAt = currentTimeMs();
    }
}

void PlaybackHistoryService::updateStreamMetadata(const Track& track, bool allowTitleOnly)
{
    if(m_trackingMode != ListeningHistoryMode::FullHistory || !m_currentEvent || !track.isRemote()) {
        return;
    }

    StreamMetadataSpan span;
    span.eventId        = m_currentEvent->id;
    span.rawStreamTitle = rawStreamTitle(track);
    span.title          = track.title();
    span.artists        = track.artists();
    span.album          = track.album();

    if(span.rawStreamTitle.isEmpty() && span.artists.isEmpty() && span.album.isEmpty() && !allowTitleOnly) {
        return;
    }
    if(m_currentSpan && m_currentSpan->rawStreamTitle == span.rawStreamTitle && m_currentSpan->title == span.title
       && m_currentSpan->artists == span.artists && m_currentSpan->album == span.album) {
        return;
    }

    // Persist progress before span boundaries so recovery can't close a span before its start
    if(!checkpointEvent() || !closeMetadataSpan()) {
        return;
    }
    span.startedOffsetMs = m_currentEvent->listeningTimeMs;
    span.id              = m_database->insertMetadataSpan(span);
    if(span.id >= 0) {
        m_currentSpan = std::move(span);
        Q_EMIT historyChanged(m_currentEvent->id);
    }
}

bool PlaybackHistoryService::closeMetadataSpan()
{
    if(!m_currentSpan) {
        return true;
    }
    if(!m_currentEvent || !m_database->closeMetadataSpan(m_currentSpan->id, m_currentEvent->listeningTimeMs)) {
        return false;
    }
    m_currentSpan.reset();
    return true;
}

void PlaybackHistoryService::handlePlayState(Player::PlayState state, Player::PlayState /*previous*/)
{
    if(state == Player::PlayState::Playing) {
        if(!m_currentEvent && m_playerController->currentTrack().isValid()) {
            startEvent(m_playerController->currentPlaybackItemId(), m_playerController->currentTrack());
        }
        return;
    }

    updateListeningTime();
    if(state == Player::PlayState::Paused) {
        checkpointEvent();
    }
    if(state == Player::PlayState::Stopped) {
        finaliseEvent();
    }
}
} // namespace Fooyin
