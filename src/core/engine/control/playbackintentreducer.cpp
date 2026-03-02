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

#include "playbackintentreducer.h"

namespace Fooyin {
PlaybackAction reducePlaybackIntent(const PlaybackIntentReducerState& state, const PlaybackIntent intent)
{
    const bool canFadePause = state.fadingEnabled && state.pauseFadeOutMs > 0;
    const bool canFadeStop  = state.fadingEnabled && state.stopFadeOutMs > 0;

    switch(intent) {
        case PlaybackIntent::Play:
            return state.playbackState == Engine::PlaybackState::Playing ? PlaybackAction::Continue
                                                                         : PlaybackAction::Immediate;
        case PlaybackIntent::Pause:
            if(state.playbackState == Engine::PlaybackState::Stopped
               || state.playbackState == Engine::PlaybackState::Paused) {
                return PlaybackAction::NoOp;
            }
            if(state.fadeState == FadeState::FadingToPause) {
                return PlaybackAction::Continue;
            }
            if(canFadePause) {
                return PlaybackAction::BeginFade;
            }
            return PlaybackAction::Immediate;
        case PlaybackIntent::Stop:
            if(state.playbackState == Engine::PlaybackState::Stopped) {
                return PlaybackAction::NoOp;
            }
            if(state.fadeState == FadeState::FadingToStop) {
                return PlaybackAction::Continue;
            }
            if(canFadeStop && state.playbackState == Engine::PlaybackState::Playing) {
                return PlaybackAction::BeginFade;
            }
            return PlaybackAction::Immediate;
    }

    std::unreachable();
}
} // namespace Fooyin
