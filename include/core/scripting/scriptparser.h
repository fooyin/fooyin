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

#pragma once

#include "fycore_export.h"

#include "expression.h"
#include "scripttypes.h"

#include <core/playlist/playlist.h>

namespace Fooyin {
class ScriptParserPrivate;
class ScriptVariableProvider;
class ScriptFunctionProvider;

/*!
 * Parse error emitted by the script parser.
 */
struct ScriptError
{
    int position;
    QString value;
    QString message;
};
using ErrorList = std::vector<ScriptError>;

/*!
 * Parsed script plus cache identity and parse errors.
 */
struct ParsedScript
{
    uint64_t cacheId{0};
    QString input;
    ExpressionList expressions;
    ErrorList errors;

    [[nodiscard]] bool isValid() const
    {
        return errors.empty() && !expressions.empty();
    }
};

/*!
 * Parses and evaluates scripts for a given Track, TrackList or Playlist.
 */
class FYCORE_EXPORT ScriptParser
{
public:
    ScriptParser();
    ~ScriptParser();

    /*!
     * Installs all variables exported by `provider`.
     */
    void addProvider(const ScriptVariableProvider& provider);
    /*!
     * Installs all functions exported by `provider`.
     */
    void addProvider(const ScriptFunctionProvider& provider);

    ParsedScript parse(const QString& input);
    ParsedScript parseQuery(const QString& input);

    QString evaluate(const QString& input);
    QString evaluate(const ParsedScript& input);
    QString evaluate(const QString& input, const ScriptContext& context);
    QString evaluate(const ParsedScript& input, const ScriptContext& context);

    QString evaluate(const QString& input, const Track& track);
    QString evaluate(const ParsedScript& input, const Track& track);
    QString evaluate(const QString& input, const Track& track, const ScriptContext& context);
    QString evaluate(const ParsedScript& input, const Track& track, const ScriptContext& context);

    QString evaluate(const QString& input, const TrackList& tracks);
    QString evaluate(const ParsedScript& input, const TrackList& tracks);
    QString evaluate(const QString& input, const TrackList& tracks, const ScriptContext& context);
    QString evaluate(const ParsedScript& input, const TrackList& tracks, const ScriptContext& context);

    QString evaluate(const QString& input, const Playlist& playlist);
    QString evaluate(const ParsedScript& input, const Playlist& playlist);
    QString evaluate(const QString& input, const Playlist& playlist, const ScriptContext& context);
    QString evaluate(const ParsedScript& input, const Playlist& playlist, const ScriptContext& context);

    TrackList filter(const QString& input, const TrackList& tracks);
    TrackList filter(const ParsedScript& input, const TrackList& tracks);
    PlaylistTrackList filter(const QString& input, const PlaylistTrackList& tracks);
    PlaylistTrackList filter(const ParsedScript& input, const PlaylistTrackList& tracks);

    /*!
     * Returns the maximum number of cached parsed and bound scripts.
     */
    [[nodiscard]] int cacheLimit() const;
    /*!
     * Sets the maximum number of cached parsed and bound scripts.
     */
    void setCacheLimit(int limit);
    /*!
     * Clears the parse and bind caches.
     */
    void clearCache();

private:
    std::unique_ptr<ScriptParserPrivate> p;
};
} // namespace Fooyin
