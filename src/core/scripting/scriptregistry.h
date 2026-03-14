/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <core/scripting/scriptproviders.h>

#include <functional>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Fooyin {
/*!
 * Central runtime registry for built-in and provider-defined script variables and functions.
 */
class ScriptRegistry
{
public:
    using FuncRet         = ScriptFieldValue;
    using VariableInvoker = ScriptVariableInvoker;
    using FunctionInvoker = ScriptFunctionInvoker;

    ScriptRegistry();
    ~ScriptRegistry();

    void addProvider(const ScriptVariableProvider& provider);
    void addProvider(const ScriptFunctionProvider& provider);

    [[nodiscard]] VariableKind resolveVariable(const QString& var) const;
    [[nodiscard]] ScriptFunctionId resolveFunctionId(const QString& func) const;

    [[nodiscard]] bool isVariable(const QString& var, const Track& track) const;
    [[nodiscard]] bool isVariable(const QString& var, const TrackList& tracks) const;
    [[nodiscard]] bool isFunction(const QString& func) const;

    [[nodiscard]] ScriptResult value(VariableKind kind, const QString& var, const ScriptSubject& subject) const;
    [[nodiscard]] ScriptResult value(const QString& var, const ScriptSubject& subject) const;
    [[nodiscard]] ScriptResult function(const QString& func, const ScriptValueList& args,
                                        const ScriptSubject& subject) const;
    [[nodiscard]] ScriptResult function(ScriptFunctionId functionId, const QString& func, const ScriptValueList& args,
                                        const ScriptSubject& subject) const;

    template <typename Func>
    auto withContext(const ScriptContext& context, Func&& func)
    {
        struct ContextRestore
        {
            ScriptRegistry* registry;
            ScriptContext context;

            ~ContextRestore()
            {
                if(registry) {
                    registry->setContext(context);
                }
            }
        };

        ScriptContext previousContext{currentContext()};
        setContext(context);
        ContextRestore restore{this, previousContext};

        using Result = std::invoke_result_t<Func>;
        if constexpr(std::is_void_v<Result>) {
            std::invoke(std::forward<Func>(func));
            return;
        }
        else {
            return std::invoke(std::forward<Func>(func));
        }
    }

    struct RegisteredFunction
    {
        QString name;
        FunctionInvoker invoker;
    };

    struct TrackListAggregateCache
    {
        const TrackList* tracks{nullptr};
        int trackCount{-1};
        int currentPosition{-1};
        int playlistIndex{-1};
        QString playtime;
        QString genres;
        QString playlistElapsed;
    };

    [[nodiscard]] ScriptContext currentContext() const;
    void setContext(const ScriptContext& context);

    void addDefaultFunctions();
    void registerVariable(VariableKind kind, const QString& name, VariableInvoker invoker);
    ScriptFunctionId registerFunction(const QString& name, FunctionInvoker invoker);
    [[nodiscard]] const FunctionInvoker* functionInvoker(ScriptFunctionId functionId) const;

    [[nodiscard]] VariableKind resolveVariableInternal(const QString& var) const;
    [[nodiscard]] ScriptResult valueInternal(VariableKind kind, const QString& var, const ScriptSubject& subject) const;
    [[nodiscard]] ScriptResult valueInternal(const QString& var, const ScriptSubject& subject) const;
    [[nodiscard]] ScriptResult valueForTrack(VariableKind kind, const QString& var, const Track& track,
                                             const Playlist* playlist) const;
    [[nodiscard]] ScriptResult valueForTrackList(VariableKind kind, const QString& var, const TrackList& tracks,
                                                 const Playlist* playlist) const;
    [[nodiscard]] ScriptResult valueForPlaylist(VariableKind kind, const QString& var, const Playlist& playlist) const;

    [[nodiscard]] ScriptResult calculateResult(const FuncRet& funcRet) const;

    [[nodiscard]] FuncRet albumArtistMetadata(const Track& track) const;
    [[nodiscard]] FuncRet bitrateMetadata(const Track& track) const;
    [[nodiscard]] bool playbackValueAvailableForTrack(const Track& track = {}) const;
    [[nodiscard]] QString playbackTime() const;
    [[nodiscard]] QString playbackTimeSeconds() const;
    [[nodiscard]] QString playbackTimeRemaining() const;
    [[nodiscard]] QString playbackTimeRemainingSeconds() const;
    [[nodiscard]] QString isPlaying() const;
    [[nodiscard]] QString isPaused() const;
    [[nodiscard]] QString libraryNameVariable(const Track& track) const;
    [[nodiscard]] QString libraryPathVariable(const Track& track) const;
    [[nodiscard]] QString relativePathVariable(const Track& track) const;
    [[nodiscard]] QString getBitrate(const Track& track) const;
    [[nodiscard]] QString playlistDuration(const TrackList& tracks) const;

    void clearTrackListCache();
    [[nodiscard]] const TrackListAggregateCache& cachedTrackListValues(const TrackList& tracks) const;

private:
    ScriptContext m_context;
    std::unordered_map<QString, ScriptFunctionId> m_functionIds;
    std::vector<RegisteredFunction> m_functions;
    std::unordered_map<QString, VariableKind> m_customVariableKinds;
    std::unordered_map<VariableKind, VariableInvoker> m_customVariables;
    mutable TrackListAggregateCache m_trackListCache;
};
} // namespace Fooyin
