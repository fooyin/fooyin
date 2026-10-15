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

#include <core/scripting/scriptregistry.h>

#include <QCoreApplication>

#include <functional>

namespace Fooyin {
QString escapeRichText(const ScriptEvaluationOptions* options, QString value);
QString formattedRatingStars(const Track& track, const ScriptEvaluationOptions& options, bool includeEmptyStars);

/*!
 * Runtime evaluation state for the shared scripting catalogue.
 */
class ScriptEvaluator
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::ScriptRegistry)

public:
    using FuncRet = ScriptFieldValue;

    explicit ScriptEvaluator(ScriptRegistry* registry);
    ~ScriptEvaluator();

    [[nodiscard]] ScriptResult value(VariableKind kind, const QString& var, const ScriptSubject& subject) const;
    [[nodiscard]] ScriptResult function(ScriptFunctionId functionId, const QString& func, const ScriptValueList& args,
                                        const ScriptSubject& subject) const;

    template <typename Func>
    decltype(auto) withContext(const ScriptContext& context, Func&& func)
    {
        const auto* previous = m_context;
        m_context            = &context;
        clearTrackListCache();
        struct Restore
        {
            ScriptEvaluator* evaluator;
            const ScriptContext* context;
            ~Restore()
            {
                evaluator->m_context = context;
                evaluator->clearTrackListCache();
            }
        } restore{this, previous};
        return std::invoke(std::forward<Func>(func));
    }

    struct TrackListAggregateCache
    {
        const TrackList* tracks{nullptr};
        int trackCount{-1};
        int currentPosition{-1};
        int playlistIndex{-1};
        QString playtime;
        QString playlistSize;
        QString genres;
        QString playlistElapsed;
    };

    [[nodiscard]] const ScriptContext& currentContext() const;

    [[nodiscard]] ScriptResult valueForTrack(VariableKind kind, const QString& var, const Track& track,
                                             const Playlist* playlist) const;
    [[nodiscard]] ScriptResult valueForTrackList(VariableKind kind, const QString& var, const TrackList& tracks) const;
    [[nodiscard]] ScriptResult valueForPlaylist(VariableKind kind, const QString& var, const Playlist& playlist) const;

    [[nodiscard]] ScriptResult calculateResult(const FuncRet& funcRet) const;

    [[nodiscard]] FuncRet albumArtistMetadata(const Track& track) const;
    [[nodiscard]] FuncRet bitrateMetadata(const Track& track) const;
    [[nodiscard]] bool playbackValueAvailableForTrack(const Track& track = {}) const;
    [[nodiscard]] QString playbackTime() const;
    [[nodiscard]] QString playbackTimeSeconds() const;
    [[nodiscard]] QString playbackTimeRemaining() const;
    [[nodiscard]] QString playbackTimeRemainingSeconds() const;
    [[nodiscard]] QString decoder() const;
    [[nodiscard]] QString outputInfo(VariableKind kind) const;
    [[nodiscard]] QString isPlaying() const;
    [[nodiscard]] QString isPaused() const;
    [[nodiscard]] QString isStopped() const;
    [[nodiscard]] QString libraryNameVariable(const Track& track) const;
    [[nodiscard]] QString libraryPathVariable(const Track& track) const;
    [[nodiscard]] QString relativePathVariable(const Track& track) const;
    [[nodiscard]] QString getBitrate(const Track& track) const;
    [[nodiscard]] QString playlistDuration(const TrackList& tracks) const;

    void clearTrackListCache();
    [[nodiscard]] const TrackListAggregateCache& cachedTrackListValues(const TrackList& tracks) const;

    ScriptEvaluationOptions options;

private:
    ScriptContext m_emptyContext;
    const ScriptContext* m_context{&m_emptyContext};
    ScriptRegistry* m_registry;
    mutable TrackListAggregateCache m_trackListCache;
};
} // namespace Fooyin
