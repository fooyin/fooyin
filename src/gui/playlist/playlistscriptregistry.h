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

#include <core/player/playbackqueue.h>
#include <core/player/playerdefs.h>
#include <core/scripting/scriptregistry.h>

namespace Fooyin {
class PlaylistScriptRegistryPrivate;

constexpr auto PlayingIcon   = "%playingicon%";
constexpr auto FrontCover    = "%frontcover%";
constexpr auto BackCover     = "%backcover%";
constexpr auto ArtistPicture = "%artistpicture%";

class PlaylistScriptRegistry : public ScriptRegistry
{
public:
    PlaylistScriptRegistry();
    ~PlaylistScriptRegistry() override;

    using ScriptRegistry::isVariable;
    using ScriptRegistry::value;

    void setup(const UId& playlistId, const PlaybackQueue& queue);
    void setTrackProperties(int index, int depth);

    [[nodiscard]] bool isVariable(const QString& var, const Track& track) const override;
    [[nodiscard]] ScriptResult value(const QString& var, const Track& track) const override;

private:
    std::unique_ptr<PlaylistScriptRegistryPrivate> p;
};
} // namespace Fooyin
