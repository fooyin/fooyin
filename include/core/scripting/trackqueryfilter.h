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

#include <core/playlist/playlist.h>

#include <memory>

namespace Fooyin {
class FySettings;
class SettingsManager;
class ScriptFunctionProvider;
class ScriptVariableProvider;

enum class ScriptSearchMode : uint8_t
{
    MatchWordBeginnings = 0,
    MatchAnywhere,
};

struct ScriptSearchOptions
{
    QString script;
    ScriptSearchMode mode{ScriptSearchMode::MatchWordBeginnings};
};

class TrackSearchFilterPrivate;

class FYCORE_EXPORT TrackQueryFilter
{
public:
    static constexpr auto DefaultSearchMode   = 0;
    static constexpr auto DefaultSearchScript = "[%artist%] [%title%] [%album%] [%albumartist%] [%performer%] "
                                                "[%composer%] [%genre%] [%comment%] [%filepath%]";

    TrackQueryFilter();
    explicit TrackQueryFilter(ScriptSearchOptions options);
    ~TrackQueryFilter();

    [[nodiscard]] static bool matchesSearch(const QString& text, const QString& search, ScriptSearchMode mode,
                                            bool singleString = false);

    void addProvider(const ScriptVariableProvider& provider);
    void addProvider(const ScriptFunctionProvider& provider);

    [[nodiscard]] TrackList filter(const QString& search, const TrackList& tracks);
    [[nodiscard]] PlaylistTrackList filter(const QString& search, const PlaylistTrackList& tracks);

private:
    std::unique_ptr<TrackSearchFilterPrivate> p;
};
} // namespace Fooyin
