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

#include "core/engine/output/fadecontroller.h"

#include <core/engine/enginedefs.h>

namespace Fooyin {
enum class PlaybackIntent : uint8_t
{
    Play = 0,
    Pause,
    Stop
};

enum class PlaybackAction : uint8_t
{
    NoOp = 0,
    Continue,
    BeginFade,
    Immediate
};

struct PlaybackIntentReducerState
{
    Engine::PlaybackState playbackState{Engine::PlaybackState::Stopped};
    FadeState fadeState{FadeState::Idle};
    bool fadingEnabled{false};
    int pauseFadeOutMs{0};
    int stopFadeOutMs{0};
};

FYCORE_EXPORT PlaybackAction reducePlaybackIntent(const PlaybackIntentReducerState& state, PlaybackIntent intent);
} // namespace Fooyin
