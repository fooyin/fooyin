/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "librarytreescriptregistry.h"

#include <core/constants.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
VariableKind LibraryTreeScriptRegistry::resolveVariableKind(const QString& var) const
{
    if(var == "FRONTCOVER"_L1) {
        return VariableKind::FrontCover;
    }
    if(var == "BACKCOVER"_L1) {
        return VariableKind::BackCover;
    }
    if(var == "ARTISTPICTURE"_L1) {
        return VariableKind::ArtistPicture;
    }

    return ScriptRegistry::resolveVariableKind(var);
}

bool LibraryTreeScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    if(resolveVariableKind(var) != VariableKind::Generic || isListVariable(var)) {
        return true;
    }

    return ScriptRegistry::isVariable(var, track);
}

ScriptResult LibraryTreeScriptRegistry::value(VariableKind kind, const QString& var, const Track& track) const
{
    switch(kind) {
        case VariableKind::FrontCover:
            return {.value = QLatin1String{Constants::FrontCover}, .cond = true};
        case VariableKind::BackCover:
            return {.value = QLatin1String{Constants::BackCover}, .cond = true};
        case VariableKind::ArtistPicture:
            return {.value = QLatin1String{Constants::ArtistPicture}, .cond = true};
        case VariableKind::Generic:
        case VariableKind::Track:
        case VariableKind::Disc:
        case VariableKind::DiscTotal:
        case VariableKind::Title:
        case VariableKind::UniqueArtist:
        case VariableKind::PlayCount:
        case VariableKind::Duration:
        case VariableKind::AlbumArtist:
        case VariableKind::Album:
        case VariableKind::Genres:
        case VariableKind::TrackCount:
        case VariableKind::Playtime:
        case VariableKind::PlaylistDuration:
        case VariableKind::Depth:
        case VariableKind::ListIndex:
        case VariableKind::QueueIndex:
        case VariableKind::QueueIndexes:
        case VariableKind::PlayingIcon:
            break;
    }

    return ScriptRegistry::value(kind, var, track);
}

ScriptResult LibraryTreeScriptRegistry::value(const QString& var, const Track& track) const
{
    return value(resolveVariableKind(var), var, track);
}
} // namespace Fooyin
