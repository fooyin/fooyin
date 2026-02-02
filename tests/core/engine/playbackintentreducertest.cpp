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

#include "core/engine/control/playbackintentreducer.h"

#include <gtest/gtest.h>

namespace Fooyin::Testing {
TEST(PlaybackIntentReducerTest, PlayFromPausedIsImmediate)
{
    PlaybackIntentReducerState state;
    state.playbackState = Engine::PlaybackState::Paused;

    EXPECT_EQ(reducePlaybackIntent(state, PlaybackIntent::Play), PlaybackAction::Immediate);
}

TEST(PlaybackIntentReducerTest, PlayWhilePlayingContinues)
{
    PlaybackIntentReducerState state;
    state.playbackState = Engine::PlaybackState::Playing;

    EXPECT_EQ(reducePlaybackIntent(state, PlaybackIntent::Play), PlaybackAction::Continue);
}

TEST(PlaybackIntentReducerTest, PauseWhileStoppedIsNoOp)
{
    PlaybackIntentReducerState state;
    state.playbackState = Engine::PlaybackState::Stopped;

    EXPECT_EQ(reducePlaybackIntent(state, PlaybackIntent::Pause), PlaybackAction::NoOp);
}

TEST(PlaybackIntentReducerTest, PauseBeginsFadeWhenEnabled)
{
    PlaybackIntentReducerState state;
    state.playbackState  = Engine::PlaybackState::Playing;
    state.fadingEnabled  = true;
    state.pauseFadeOutMs = 250;

    EXPECT_EQ(reducePlaybackIntent(state, PlaybackIntent::Pause), PlaybackAction::BeginFade);
}

TEST(PlaybackIntentReducerTest, StopFromPausedIsImmediate)
{
    PlaybackIntentReducerState state;
    state.playbackState = Engine::PlaybackState::Paused;

    EXPECT_EQ(reducePlaybackIntent(state, PlaybackIntent::Stop), PlaybackAction::Immediate);
}

TEST(PlaybackIntentReducerTest, StopWhileFadeToStopContinues)
{
    PlaybackIntentReducerState state;
    state.playbackState = Engine::PlaybackState::Playing;
    state.fadeState     = FadeState::FadingToStop;

    EXPECT_EQ(reducePlaybackIntent(state, PlaybackIntent::Stop), PlaybackAction::Continue);
}

TEST(PlaybackIntentReducerTest, StopFadeUsesStopDuration)
{
    PlaybackIntentReducerState state;
    state.playbackState  = Engine::PlaybackState::Playing;
    state.fadingEnabled  = true;
    state.pauseFadeOutMs = 0;
    state.stopFadeOutMs  = 250;

    EXPECT_EQ(reducePlaybackIntent(state, PlaybackIntent::Stop), PlaybackAction::BeginFade);
}
} // namespace Fooyin::Testing
