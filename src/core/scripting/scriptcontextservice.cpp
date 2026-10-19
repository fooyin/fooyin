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

#include <core/scripting/scriptcontextservice.h>

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>

#include <QBasicTimer>
#include <QDateTime>
#include <QPointer>
#include <QThread>
#include <QTimerEvent>

#include <deque>
#include <unordered_map>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
bool affectsSubjects(const ScriptChange& change, std::span<const Track> tracks, const UId& playlistId)
{
    if(change.playlistId.isValid() && playlistId.isValid() && change.playlistId != playlistId) {
        return false;
    }
    return !change.track.isValid() || tracks.empty()
        || std::ranges::any_of(tracks, [&change](const Track& track) { return track.sameIdentityAs(change.track); });
}
} // namespace

class ScriptContextServicePrivate
{
public:
    ScriptContextServicePrivate(ScriptContextService* self, PlayerController* player)
        : m_self{self}
        , m_player{player}
        , m_playbackTrack{player->currentPlaylistTrack()}
        , m_queueIndex{player->playbackQueue().currentIndex()}
    { }

    void updateDemand()
    {
        if(m_shutdown) {
            return;
        }

        for(ScriptDataSourceId id{1}; id <= ScriptRegistry::instance().dataSourceCount(); ++id) {
            const bool observed = std::ranges::any_of(
                m_subscriptions, [id](const auto* subscription) { return subscription->dependencies().contains(id); });
            if(std::exchange(m_sources[id].observed, observed) != observed) {
                Q_EMIT m_self->sourceDemandChanged(id, observed);
            }
        }

        if(m_self->isObserved(ScriptSources::Clock)) {
            if(!m_clock.isActive()) {
                m_clock.start(1000, m_self);
            }
        }
        else {
            m_clock.stop();
        }
    }

    void publishPlaybackChange(ScriptDataSourceId source)
    {
        const auto track = m_player->currentTrack();
        if(m_playbackTrack.track.isValid()) {
            m_self->publish(source, m_playbackTrack.track);
        }
        if(track.isValid() && !track.sameIdentityAs(m_playbackTrack.track)) {
            m_self->publish(source, track);
        }
    }

    void publishPlaylistPlaybackChange()
    {
        const auto playlistId = m_player->currentPlaylistTrack().playlistId;
        if(m_playbackTrack.playlistId.isValid()) {
            m_self->publish(ScriptSources::PlaylistPlayback, {}, m_playbackTrack.playlistId);
        }
        if(playlistId.isValid() && playlistId != m_playbackTrack.playlistId) {
            m_self->publish(ScriptSources::PlaylistPlayback, {}, playlistId);
        }
    }

    void publishQueueCursorChange()
    {
        const auto& queue  = m_player->playbackQueue();
        const int current  = queue.currentIndex();
        const int previous = std::exchange(m_queueIndex, current);
        const int first    = std::max(0, std::min(previous, current));
        const int last     = std::min(queue.trackCount() - 1, std::max(previous, current));

        for(int index{first}; index <= last; ++index) {
            if(const auto* item = queue.item(index)) {
                m_self->publish(ScriptSources::QueueCursor, item->track.track, {}, item->id);
            }
        }
    }

    struct Source
    {
        uint64_t revision{0};
        bool observed{false};
        QPointer<QObject> owner;
        ScriptContextService::Capture capture;
        std::deque<ScriptChange> changes;
    };

    ScriptContextService* m_self;
    PlayerController* m_player;
    PlaylistTrack m_playbackTrack;
    int m_queueIndex;
    std::unordered_map<ScriptDataSourceId, Source> m_sources;
    std::vector<ScriptSubscription*> m_subscriptions;
    QBasicTimer m_clock;
    uint64_t m_playbackGeneration{0};
    bool m_shutdown{false};
};

class ScriptSubscriptionPrivate
{
public:
    ScriptSubscriptionPrivate(ScriptSubscription* self, ScriptContextService* service)
        : m_self{self}
        , m_service{service}
    { }

    void invalidate(const ScriptChange& change)
    {
        if(change.source != InvalidScriptDataSource && !m_dependencies.unclassified
           && !m_dependencies.contains(change.source)) {
            return;
        }

        const auto tracks = m_track.isValid() ? std::span{&m_track, 1} : std::span<const Track>{};
        if(!affectsSubjects(change, tracks, m_playlistId)) {
            return;
        }

        const auto pending = change.queueItemId != 0
                               ? m_pending.end()
                               : std::ranges::find_if(m_pending, [&change](const ScriptChange& item) {
                                     return item.source == change.source && item.playlistId == change.playlistId
                                         && item.queueItemId == change.queueItemId
                                         && ((!item.track.isValid() && !change.track.isValid())
                                             || item.track.sameIdentityAs(change.track));
                                 });
        if(pending != m_pending.end()) {
            *pending = change;
        }
        else {
            m_pending.push_back(change);
        }

        if(!std::exchange(m_scheduled, true)) {
            QMetaObject::invokeMethod(
                m_self,
                [this]() {
                    m_scheduled        = false;
                    const auto changes = std::exchange(m_pending, {});
                    if(m_service && !m_service->p->m_shutdown) {
                        Q_EMIT m_self->changed(changes);
                    }
                },
                Qt::QueuedConnection);
        }
    }

    ScriptSubscription* m_self;
    QPointer<ScriptContextService> m_service;
    ScriptDependencies m_dependencies{.unclassified = false};
    Track m_track;
    UId m_playlistId;
    ScriptChanges m_pending;
    bool m_scheduled{false};
    QString m_script;
    bool m_analysed{false};
};

void ScriptSubscription::setScript(const QString& script)
{
    if(p->m_analysed && p->m_script == script) {
        return;
    }

    p->m_script = script;

    if(!ScriptRegistry::instance().isFinalised()) {
        QMetaObject::invokeMethod(
            this,
            [this]() {
                if(ScriptRegistry::instance().isFinalised()) {
                    setScript(p->m_script);
                    refresh();
                }
            },
            Qt::QueuedConnection);
        return;
    }

    ScriptParser parser;
    setDependencies(parser.dependencies(parser.parse(script)));
    p->m_analysed = true;
}

void ScriptSubscription::setDependencies(ScriptDependencies dependencies)
{
    Q_ASSERT(QThread::currentThread() == thread());

    p->m_dependencies = std::move(dependencies);
    p->m_analysed     = false;
    if(p->m_service) {
        p->m_service->p->updateDemand();
    }
}

void ScriptSubscription::setScope(const Track& track, const UId& playlistId)
{
    p->m_track      = track;
    p->m_playlistId = playlistId;
}

void ScriptSubscription::refresh()
{
    p->invalidate({});
}

const ScriptDependencies& ScriptSubscription::dependencies() const
{
    return p->m_dependencies;
}

ScriptSubscription::ScriptSubscription(ScriptContextService* service, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<ScriptSubscriptionPrivate>(this, service)}
{ }

ScriptSubscription::~ScriptSubscription() = default;

ScriptContextService::ScriptContextService(PlayerController* player, PlaylistHandler* playlistsHandler, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<ScriptContextServicePrivate>(this, player)}
{
    QObject::connect(player, &PlayerController::currentTrackChanged, this, [this]() {
        ++p->m_playbackGeneration;
        p->publishPlaybackChange(ScriptSources::PlaybackTrack);
        p->publishPlaybackChange(ScriptSources::PlaybackTime);
        p->publishPlaybackChange(ScriptSources::PlaybackOutput);
        p->publishPlaylistPlaybackChange();
        p->m_playbackTrack = p->m_player->currentPlaylistTrack();
    });
    QObject::connect(player, &PlayerController::currentTrackUpdated, this, [this](const Track& track) {
        publish(ScriptSources::TrackMetadata, track);
        if(track.duration() != p->m_playbackTrack.track.duration()) {
            p->publishPlaylistPlaybackChange();
        }
        p->m_playbackTrack.track = track;
    });
    QObject::connect(player, &PlayerController::playStateChanged, this,
                     [this](Player::PlayState state, Player::PlayState previous) {
                         if(state == previous) {
                             return;
                         }
                         publish(ScriptSources::PlaybackState);
                         p->publishPlaybackChange(ScriptSources::PlaybackTrack);
                         if((state == Player::PlayState::Stopped) != (previous == Player::PlayState::Stopped)) {
                             publish(ScriptSources::PlaybackStopped);
                             p->publishPlaybackChange(ScriptSources::PlaybackTime);
                             p->publishPlaybackChange(ScriptSources::PlaybackOutput);
                             p->publishPlaylistPlaybackChange();
                         }
                     });

    const auto positionChanged = [this]() {
        p->publishPlaybackChange(ScriptSources::PlaybackTime);
        p->publishPlaylistPlaybackChange();
    };
    QObject::connect(player, &PlayerController::positionChangedSeconds, this, positionChanged);
    QObject::connect(player, &PlayerController::positionMoved, this, positionChanged);

    QObject::connect(player, &PlayerController::bitrateChanged, this,
                     [this]() { p->publishPlaybackChange(ScriptSources::PlaybackOutput); });
    QObject::connect(player, &PlayerController::playbackOutputInfoChanged, this,
                     [this]() { p->publishPlaybackChange(ScriptSources::PlaybackOutput); });
    QObject::connect(player, &PlayerController::playlistTrackUpdated, this, [this](const PlaylistTrack& track) {
        if(track.playlistId == p->m_playbackTrack.playlistId && track.entryId == p->m_playbackTrack.entryId
           && track.indexInPlaylist == p->m_playbackTrack.indexInPlaylist
           && track.track.sameIdentityAs(p->m_playbackTrack.track)) {
            return;
        }
        p->publishPlaybackChange(ScriptSources::PlaybackTrack);
        p->publishPlaybackChange(ScriptSources::PlaybackTime);
        p->publishPlaybackChange(ScriptSources::PlaybackOutput);
        p->publishPlaylistPlaybackChange();
        p->m_playbackTrack = track;
    });

    const auto queueChanged = [this]() {
        p->m_queueIndex = p->m_player->playbackQueue().currentIndex();
        publish(ScriptSources::Queue);
    };
    QObject::connect(player, &PlayerController::tracksQueued, this, queueChanged);
    QObject::connect(player, &PlayerController::tracksDequeued, this, queueChanged);
    QObject::connect(player, &PlayerController::trackIndexesDequeued, this, queueChanged);
    QObject::connect(player, &PlayerController::trackQueueChanged, this, queueChanged);
    QObject::connect(player, &PlayerController::playbackQueueModeChanged, this, queueChanged);

    QObject::connect(player, &PlayerController::playbackQueuePositionChanged, this,
                     [this]() { p->publishQueueCursorChange(); });

    const auto playlistChanged = [this](Playlist* playlist) {
        publish(ScriptSources::Playlist, {}, playlist ? playlist->id() : UId{});
    };
    QObject::connect(playlistsHandler, &PlaylistHandler::playlistRenamed, this, playlistChanged);
    QObject::connect(playlistsHandler, &PlaylistHandler::playlistUpdated, this, playlistChanged);
    QObject::connect(playlistsHandler, &PlaylistHandler::tracksAdded, this, playlistChanged);
    QObject::connect(playlistsHandler, &PlaylistHandler::tracksPatched, this, playlistChanged);
    QObject::connect(playlistsHandler, &PlaylistHandler::tracksChanged, this, playlistChanged);
    QObject::connect(playlistsHandler, &PlaylistHandler::tracksUpdated, this, playlistChanged);
    QObject::connect(playlistsHandler, &PlaylistHandler::tracksRemoved, this, playlistChanged);
}

ScriptContextService::~ScriptContextService() = default;

ScriptSubscription* ScriptContextService::subscribe(QObject* owner)
{
    Q_ASSERT(QThread::currentThread() == thread());
    Q_ASSERT(owner && owner->thread() == thread());

    auto* subscription = new ScriptSubscription(this, owner);
    p->m_subscriptions.push_back(subscription);
    QObject::connect(subscription, &QObject::destroyed, this, [this, subscription]() {
        std::erase(p->m_subscriptions, subscription);
        p->updateDemand();
    });
    return subscription;
}

void ScriptContextService::registerProvider(ScriptDataSourceId source, QObject* owner, Capture capture)
{
    Q_ASSERT(QThread::currentThread() == thread());
    Q_ASSERT(source != InvalidScriptDataSource && source <= ScriptRegistry::instance().dataSourceCount());

    auto& entry = p->m_sources[source];
    Q_ASSERT(!entry.capture);
    entry.owner   = owner;
    entry.capture = std::move(capture);
    QObject::connect(owner, &QObject::destroyed, this, [this, source]() {
        p->m_sources[source].capture = {};
        publish(source);
    });
}

void ScriptContextService::publish(ScriptDataSourceId source, const Track& track, const UId& playlistId,
                                   PlaybackQueueItemId queueItemId)
{
    Q_ASSERT(QThread::currentThread() == thread());

    if(p->m_shutdown) {
        return;
    }

    Q_ASSERT(source != InvalidScriptDataSource && source <= ScriptRegistry::instance().dataSourceCount());

    auto& entry = p->m_sources[source];
    const ScriptChange change{.source      = source,
                              .revision    = ++entry.revision,
                              .track       = track,
                              .playlistId  = playlistId,
                              .queueItemId = queueItemId};
    entry.changes.push_back(change);
    // Older captures fall back to conservative invalidation
    if(entry.changes.size() > 256) {
        entry.changes.pop_front();
    }

    for(auto* subscription : p->m_subscriptions) {
        subscription->p->invalidate(change);
    }
}

void ScriptContextService::capture(ScriptContextData& data) const
{
    Q_ASSERT(QThread::currentThread() == thread());

    if(p->m_shutdown) {
        return;
    }

    data.playback.generation = p->m_playbackGeneration;
    const QString date       = QDateTime::currentDateTime().toString(u"yyyy-MM-dd hh:mm:ss"_s);
    data.values.push_back({.name = u"DATETIME"_s, .result = {.value = date, .cond = true}, .track = {}});

    for(ScriptDataSourceId id{1}; id <= ScriptRegistry::instance().dataSourceCount(); ++id) {
        const auto it = p->m_sources.find(id);
        data.revisions.push_back({.source = id, .revision = it == p->m_sources.end() ? 0 : it->second.revision});
        if(it != p->m_sources.end() && it->second.owner && it->second.capture) {
            it->second.capture(data);
        }
    }
}

ScriptContextData ScriptContextService::playbackContext(Playlist* playlist) const
{
    Q_ASSERT(QThread::currentThread() == thread());

    auto data = makePlaybackScriptData(p->m_player, playlist);
    capture(data);
    return data;
}

bool ScriptContextService::isCurrent(const ScriptContextData& data, const ScriptDependencies& dependencies,
                                     std::span<const Track> tracks) const
{
    Q_ASSERT(QThread::currentThread() == thread());

    if(p->m_shutdown) {
        return false;
    }

    return std::ranges::all_of(data.revisions, [this, &data, &dependencies,
                                                tracks](const ScriptSourceRevision& revision) {
        if(!dependencies.unclassified && !dependencies.contains(revision.source)) {
            return true;
        }

        const auto it = p->m_sources.find(revision.source);
        if(it == p->m_sources.end() || it->second.revision == revision.revision) {
            return true;
        }

        const auto& changes = it->second.changes;
        if(changes.empty() || revision.revision < changes.front().revision - 1) {
            return false;
        }

        return std::ranges::none_of(changes, [&data, &revision, tracks](const ScriptChange& change) {
            return change.revision > revision.revision && affectsSubjects(change, tracks, data.playlistData.playlistId);
        });
    });
}

bool ScriptContextService::isObserved(ScriptDataSourceId source) const
{
    const auto it = p->m_sources.find(source);
    return !p->m_shutdown && it != p->m_sources.end() && it->second.observed;
}

uint64_t ScriptContextService::playbackGeneration() const
{
    return p->m_playbackGeneration;
}

void ScriptContextService::shutdown()
{
    p->m_shutdown = true;
    p->m_clock.stop();
    for(auto& [id, source] : p->m_sources) {
        source.capture = {};
    }
}

void ScriptContextService::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_clock.timerId()) {
        publish(ScriptSources::Clock);
    }
    QObject::timerEvent(event);
}

} // namespace Fooyin
