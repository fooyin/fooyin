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

#include <core/scripting/scripttrackwriter.h>

#include "scriptbinder.h"

#include <functional>
#include <type_traits>

namespace Fooyin {
namespace {
template <auto Func>
void invokeTrackSetter(Track& track, const ScriptFieldValue& arg)
{
    std::visit(
        [&]<typename Param>(Param&& value) {
            if constexpr(std::is_invocable_v<decltype(Func), Track&, Param>) {
                std::invoke(Func, track, value);
            }
        },
        arg);
}

bool setBuiltInTrackValue(const VariableKind kind, const ScriptFieldValue& value, Track& track)
{
    switch(kind) {
        case VariableKind::Title:
            invokeTrackSetter<&Track::setTitle>(track, value);
            return true;
        case VariableKind::Artist:
            invokeTrackSetter<&Track::setArtists>(track, value);
            return true;
        case VariableKind::Album:
            invokeTrackSetter<&Track::setAlbum>(track, value);
            return true;
        case VariableKind::AlbumArtist:
            invokeTrackSetter<&Track::setAlbumArtists>(track, value);
            return true;
        case VariableKind::Track:
            invokeTrackSetter<&Track::setTrackNumber>(track, value);
            return true;
        case VariableKind::TrackTotal:
            invokeTrackSetter<&Track::setTrackTotal>(track, value);
            return true;
        case VariableKind::Disc:
            invokeTrackSetter<&Track::setDiscNumber>(track, value);
            return true;
        case VariableKind::DiscTotal:
            invokeTrackSetter<&Track::setDiscTotal>(track, value);
            return true;
        case VariableKind::Genre:
        case VariableKind::Genres:
            invokeTrackSetter<&Track::setGenres>(track, value);
            return true;
        case VariableKind::Composer:
            invokeTrackSetter<&Track::setComposers>(track, value);
            return true;
        case VariableKind::Performer:
            invokeTrackSetter<&Track::setPerformers>(track, value);
            return true;
        case VariableKind::Duration:
            invokeTrackSetter<&Track::setDuration>(track, value);
            return true;
        case VariableKind::Comment:
            invokeTrackSetter<&Track::setComment>(track, value);
            return true;
        case VariableKind::Date:
            invokeTrackSetter<&Track::setDate>(track, value);
            return true;
        case VariableKind::Rating:
        case VariableKind::RatingStars:
        case VariableKind::RatingEditor:
            invokeTrackSetter<&Track::setRating>(track, value);
            return true;
        case VariableKind::PlayCount:
            invokeTrackSetter<&Track::setPlayCount>(track, value);
            return true;
        case VariableKind::FirstPlayed:
            invokeTrackSetter<&Track::setFirstPlayed>(track, value);
            return true;
        case VariableKind::LastPlayed:
            invokeTrackSetter<&Track::setLastPlayed>(track, value);
            return true;
        default:
            return false;
    }
}
} // namespace

void setTrackScriptValue(const QString& var, const ScriptFieldValue& value, Track& track)
{
    if(var.isEmpty()) {
        return;
    }

    const QString tag = var.toUpper();
    if(setBuiltInTrackValue(resolveBuiltInVariableKind(tag), value, track)) {
        return;
    }

    const auto setOrAddTag = [&](const auto& val) {
        if(track.hasExtraTag(tag)) {
            track.replaceExtraTag(tag, val);
        }
        else {
            track.addExtraTag(tag, val);
        }
    };

    if(const auto* strVal = std::get_if<QString>(&value)) {
        setOrAddTag(*strVal);
    }
    else if(const auto* listVal = std::get_if<QStringList>(&value)) {
        setOrAddTag(*listVal);
    }
}
} // namespace Fooyin
