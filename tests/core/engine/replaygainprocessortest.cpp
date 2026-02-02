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

#include "core/engine/output/postprocessor/replaygainprocessor.h"

#include <core/engine/audioformat.h>
#include <core/track.h>

#include <gtest/gtest.h>

#include <array>

namespace Fooyin::Testing {
TEST(ReplayGainProcessorTest, ReactsToSharedSettingsChangesMidTrack)
{
    auto sharedSettings = ReplayGainProcessor::makeSharedSettings();
    ReplayGainProcessor processor{std::shared_ptr<const ReplayGainProcessor::SharedSettings>{sharedSettings}};

    Track track;
    track.setRGTrackGain(6.0F);

    const AudioFormat format{SampleFormat::F64, 48000, 2};
    processor.init(track, format);

    std::array<double, 4> samples{0.25, 0.25, 0.25, 0.25};
    processor.process(samples.data(), samples.size());
    EXPECT_NEAR(samples[0], 0.25, 0.0001);

    sharedSettings->update([](ReplayGainProcessor::RuntimeSettings& settings) {
        settings.mode          = ReplayGainProcessor::SelectionMode::Track;
        settings.processing    = Engine::RGProcessing{Engine::ApplyGain};
        settings.rgPreampDb    = 0.0;
        settings.nonRgPreampDb = 0.0;
    });

    samples = {0.25, 0.25, 0.25, 0.25};
    processor.process(samples.data(), samples.size());
    EXPECT_GT(samples[0], 0.45);

    sharedSettings->update([](ReplayGainProcessor::RuntimeSettings& settings) {
        settings.mode          = ReplayGainProcessor::SelectionMode::Off;
        settings.processing    = Engine::NoProcessing;
        settings.rgPreampDb    = 0.0;
        settings.nonRgPreampDb = 0.0;
    });

    samples = {0.25, 0.25, 0.25, 0.25};
    processor.process(samples.data(), samples.size());
    EXPECT_NEAR(samples[0], 0.25, 0.0001);
}
} // namespace Fooyin::Testing
