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

#include "core/engine/output/fadecontroller.h"
#include "core/engine/pipeline/audiopipelinefader.h"

#include <gtest/gtest.h>

namespace {
struct FakePipeline : Fooyin::AudioPipelineFader
{
    bool fading{false};
    int lastFadeInDurationMs{0};
    int lastFadeOutDurationMs{0};

    void setFaderCurve(Fooyin::Engine::FadeCurve /*curve*/) override { }

    void faderFadeIn(int durationMs, double, uint64_t /*token*/) override
    {
        fading               = durationMs > 0;
        lastFadeInDurationMs = durationMs;
    }

    void faderFadeOut(int durationMs, double, uint64_t /*token*/) override
    {
        fading                = durationMs > 0;
        lastFadeOutDurationMs = durationMs;
    }

    void faderStop() override
    {
        fading = false;
    }

    void faderReset() override
    {
        fading = false;
    }

    [[nodiscard]] bool faderIsFading() const override
    {
        return fading;
    }
};
} // namespace

namespace Fooyin::Testing {
namespace {
Track makeTrack(const QString& path)
{
    Track track{path};
    track.setDuration(1000);
    return track;
}
} // namespace

TEST(FadeControllerTest, PauseFadeCompletesToPauseAction)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 250;

    ASSERT_TRUE(controller.beginPauseFade(true, values, 0.85, 42));
    EXPECT_EQ(controller.state(), FadeState::FadingToPause);
    EXPECT_TRUE(controller.hasPendingFade());

    const auto result = controller.handlePipelineFadeEvent(true, 42);
    EXPECT_TRUE(result.pauseNow);
    EXPECT_EQ(result.transportTransitionId, 42U);
    EXPECT_FALSE(result.stopImmediateNow);
    EXPECT_EQ(controller.state(), FadeState::Idle);
    EXPECT_FALSE(controller.hasPendingFade());
}

TEST(FadeControllerTest, StopEscalatesExistingPauseFade)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 500;
    values.stop.out  = 500;

    ASSERT_TRUE(controller.beginPauseFade(true, values, 1.0, 5));
    ASSERT_TRUE(controller.beginStopFade(true, values, 1.0, Engine::PlaybackState::Playing, 9));
    EXPECT_EQ(controller.state(), FadeState::FadingToStop);

    const auto result = controller.handlePipelineFadeEvent(true, 5);
    EXPECT_FALSE(result.pauseNow);
    EXPECT_TRUE(result.stopImmediateNow);
    EXPECT_EQ(result.transportTransitionId, 9U);
    EXPECT_EQ(controller.state(), FadeState::Idle);
}

TEST(FadeControllerTest, ReissuedStopWhileStoppingUpdatesTransportTransitionId)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.stop.out = 120;

    ASSERT_TRUE(controller.beginStopFade(true, values, 1.0, Engine::PlaybackState::Playing, 11));
    ASSERT_TRUE(controller.beginStopFade(true, values, 1.0, Engine::PlaybackState::Playing, 12));

    const auto result = controller.handlePipelineFadeEvent(true, 11);
    EXPECT_TRUE(result.stopImmediateNow);
    EXPECT_EQ(result.transportTransitionId, 12U);
}

TEST(FadeControllerTest, StopFadeUsesStopDuration)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 100;
    values.stop.out  = 420;

    ASSERT_TRUE(controller.beginStopFade(true, values, 1.0, Engine::PlaybackState::Playing, 1));
    EXPECT_EQ(pipeline.lastFadeOutDurationMs, 420);
}

TEST(FadeControllerTest, IgnoresMismatchedFadeEvents)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 150;

    ASSERT_TRUE(controller.beginPauseFade(true, values, 0.7, 1));

    const auto wrongDirection = controller.handlePipelineFadeEvent(false, 1);
    EXPECT_FALSE(wrongDirection.pauseNow);
    EXPECT_FALSE(wrongDirection.stopImmediateNow);
    EXPECT_EQ(controller.state(), FadeState::FadingToPause);

    const auto wrongToken = controller.handlePipelineFadeEvent(true, 2);
    EXPECT_FALSE(wrongToken.pauseNow);
    EXPECT_FALSE(wrongToken.stopImmediateNow);
    EXPECT_EQ(controller.state(), FadeState::FadingToPause);

    const auto correct = controller.handlePipelineFadeEvent(true, 1);
    EXPECT_TRUE(correct.pauseNow);
    EXPECT_EQ(controller.state(), FadeState::Idle);
}

TEST(FadeControllerTest, ManualChangeLoadsTrackAndRestoresPauseCurveAfterFadeIn)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::CrossfadingValues crossfade;
    crossfade.manualChange.out = 200;
    crossfade.manualChange.in  = 120;

    const Track nextTrack = makeTrack(QStringLiteral("/tmp/manual-change.flac"));

    ASSERT_TRUE(controller.handleManualChangeFade(nextTrack, true, crossfade, true, 0.8, 77));
    EXPECT_EQ(controller.state(), FadeState::FadingToManualChange);

    const auto fadeOutComplete = controller.handlePipelineFadeEvent(true, 77);
    ASSERT_TRUE(fadeOutComplete.loadTrack.has_value());
    EXPECT_EQ(fadeOutComplete.loadTrack->filepath(), nextTrack.filepath());
    EXPECT_FALSE(fadeOutComplete.setPauseCurve);

    Engine::FadingValues pauseValues;
    pauseValues.pause.in = 300;
    EXPECT_TRUE(controller.applyPlayFade(Engine::PlaybackState::Paused, true, pauseValues, 0.8));

    const auto fadeInComplete = controller.handlePipelineFadeEvent(false, 77);
    EXPECT_TRUE(fadeInComplete.setPauseCurve);
}

TEST(FadeControllerTest, CancelForReinitClearsPendingTransportState)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 100;

    ASSERT_TRUE(controller.beginPauseFade(true, values, 0.5, 15));
    EXPECT_TRUE(controller.hasPendingFade());

    controller.cancelForReinit(Engine::FadeCurve::Cosine);

    EXPECT_EQ(controller.state(), FadeState::Idle);
    EXPECT_FALSE(controller.hasPendingFade());
}

TEST(FadeControllerTest, LateCompletionAfterCancelForReinitIsIgnored)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 180;

    ASSERT_TRUE(controller.beginPauseFade(true, values, 0.6, 14));
    controller.cancelForReinit(Engine::FadeCurve::Cosine);

    const auto late = controller.handlePipelineFadeEvent(true, 1);
    EXPECT_FALSE(late.pauseNow);
    EXPECT_FALSE(late.stopImmediateNow);
    EXPECT_EQ(late.transportTransitionId, 0U);
    EXPECT_EQ(controller.state(), FadeState::Idle);
}

TEST(FadeControllerTest, PauseFadeCompletionArmsResumeFadeIn)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 250;
    values.pause.in  = 420;

    ASSERT_TRUE(controller.beginPauseFade(true, values, 0.75, 1));
    const auto result = controller.handlePipelineFadeEvent(true, 1);
    EXPECT_TRUE(result.pauseNow);
    EXPECT_TRUE(controller.hasPendingResumeFadeIn());

    EXPECT_TRUE(controller.applyPlayFade(Engine::PlaybackState::Playing, true, values, 0.75));
    EXPECT_EQ(pipeline.lastFadeInDurationMs, 420);
    EXPECT_FALSE(controller.hasPendingResumeFadeIn());
}

TEST(FadeControllerTest, PlayFromStoppedUsesStopFadeIn)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.in = 80;
    values.stop.in  = 360;

    EXPECT_TRUE(controller.applyPlayFade(Engine::PlaybackState::Stopped, true, values, 0.75));
    EXPECT_EQ(pipeline.lastFadeInDurationMs, 360);
}

TEST(FadeControllerTest, InvalidateActiveFadeFencesPreviousFadeCompletions)
{
    FakePipeline pipeline;
    FadeController controller{&pipeline};

    Engine::FadingValues values;
    values.pause.out = 120;

    ASSERT_TRUE(controller.beginPauseFade(true, values, 0.9, 1));
    const auto first = controller.handlePipelineFadeEvent(true, 1);
    ASSERT_TRUE(first.pauseNow);

    controller.invalidateActiveFade();
    ASSERT_TRUE(controller.beginPauseFade(true, values, 0.9, 2));

    const auto stale = controller.handlePipelineFadeEvent(true, 1);
    EXPECT_FALSE(stale.pauseNow);
    EXPECT_FALSE(stale.stopImmediateNow);

    const auto second = controller.handlePipelineFadeEvent(true, 2);
    EXPECT_TRUE(second.pauseNow);
}
} // namespace Fooyin::Testing
