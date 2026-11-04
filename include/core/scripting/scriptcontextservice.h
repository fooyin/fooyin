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

#pragma once

#include "fycore_export.h"

#include <core/scripting/scripttypes.h>

#include <QObject>

#include <functional>
#include <memory>

namespace Fooyin {
class ScriptContextService;
class ScriptContextServicePrivate;
class ScriptSubscriptionPrivate;
class PlaylistHandler;

struct ScriptChange
{
    ScriptDataSourceId source{InvalidScriptDataSource};
    uint64_t revision{0};
    Track track;
    UId playlistId;
    PlaybackQueueItemId queueItemId{0};
};
using ScriptChanges = std::vector<ScriptChange>;

class FYCORE_EXPORT ScriptSubscription : public QObject
{
    Q_OBJECT

public:
    ~ScriptSubscription() override;

    void setScript(const QString& script);
    void setDependencies(ScriptDependencies dependencies);
    void setScope(const Track& track, const UId& playlistId = {});
    void refresh();

    [[nodiscard]] const ScriptDependencies& dependencies() const;

Q_SIGNALS:
    void changed(const Fooyin::ScriptChanges& changes);

private:
    friend class ScriptContextService;

    explicit ScriptSubscription(ScriptContextService* service, QObject* parent);

    std::unique_ptr<ScriptSubscriptionPrivate> p;
};

/*!
 * Publishes script data changes and captures context for script evaluation.
 *
 * The service and its subscriptions must be used on the main thread.
 * Call ScriptContextData::freeze() before passing captured data to a worker thread.
 */
class FYCORE_EXPORT ScriptContextService : public QObject
{
    Q_OBJECT

public:
    using Capture = std::function<void(ScriptContextData&)>;

    explicit ScriptContextService(PlayerController* player, PlaylistHandler* playlistsHandler,
                                  QObject* parent = nullptr);
    ~ScriptContextService() override;

    /** Creates a subscription parented to @p owner. */
    ScriptSubscription* subscribe(QObject* owner);
    /*!
     * Registers @p capture as the provider for @p source, tied to the lifetime of @p owner.
     *
     * Register providers before script evaluation starts. The callback runs on the main thread
     * and must only read provider state. It must not publish changes or start asynchronous work.
     */
    void registerProvider(ScriptDataSourceId source, QObject* owner, Capture capture);
    /*!
     * Publishes a change to @p source after its provider state has been updated.
     *
     * An invalid @p track or @p playlistId leaves that scope unrestricted.
     * A non-zero @p queueItemId restricts queue updates to that occurrence.
     */
    void publish(ScriptDataSourceId source, const Track& track = {}, const UId& playlistId = {},
                 PlaybackQueueItemId queueItemId = 0);
    /** Adds current provider values and source revisions to @p data. */
    void capture(ScriptContextData& data) const;

    /** Returns context containing current playback, @p playlist and provider data. */
    [[nodiscard]] ScriptContextData playbackContext(Playlist* playlist = nullptr) const;
    /** Returns @c true if no relevant changes have been published since @p data was captured. */
    [[nodiscard]] bool isCurrent(const ScriptContextData& data, const ScriptDependencies& dependencies,
                                 std::span<const Track> tracks = {}) const;
    /** Returns @c true if a subscription depends on @p source. */
    [[nodiscard]] bool isObserved(ScriptDataSourceId source) const;
    [[nodiscard]] uint64_t playbackGeneration() const;

    /** Stops change notifications and clears provider callbacks. */
    void shutdown();

Q_SIGNALS:
    void sourceDemandChanged(Fooyin::ScriptDataSourceId source, bool observed);

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    friend class ScriptSubscription;
    friend class ScriptSubscriptionPrivate;

    std::unique_ptr<ScriptContextServicePrivate> p;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::ScriptChanges)
