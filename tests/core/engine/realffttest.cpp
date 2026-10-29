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

#include <core/engine/dsp/realfft.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace {
TEST(RealFftTest, DetectsDominantSineBin)
{
    constexpr int fftSize   = 1024;
    constexpr int targetBin = 7;
    constexpr float twoPi   = std::numbers::pi_v<float> * 2.0F;

    const Fooyin::Dsp::RealFft fft{fftSize};
    ASSERT_TRUE(fft.isValid());
    ASSERT_EQ(fft.binCount(), (fftSize / 2) + 1);

    std::vector<float> input(fftSize);
    for(int index{0}; index < fftSize; ++index) {
        const float phase
            = twoPi * static_cast<float>(targetBin) * static_cast<float>(index) / static_cast<float>(fftSize);
        input[static_cast<size_t>(index)] = std::sin(phase);
    }

    std::vector magnitudes(static_cast<size_t>(fft.binCount()), 0.0F);
    ASSERT_TRUE(fft.transformMagnitudes(input, magnitudes));

    const auto dominant = std::ranges::max_element(magnitudes);
    ASSERT_NE(dominant, magnitudes.end());
    EXPECT_EQ(std::distance(magnitudes.begin(), dominant), targetBin);
    EXPECT_GT(*dominant, 0.9F);
}

TEST(RealFftTest, ReportsDcBin)
{
    constexpr int fftSize = 1024;

    const Fooyin::Dsp::RealFft fft{fftSize};
    ASSERT_TRUE(fft.isValid());

    const std::vector input(fftSize, 1.0F);
    std::vector magnitudes(static_cast<size_t>(fft.binCount()), 0.0F);
    ASSERT_TRUE(fft.transformMagnitudes(input, magnitudes));

    EXPECT_NEAR(magnitudes.front(), 1.0F, 1.0e-4F);
    for(size_t bin{1}; bin < magnitudes.size(); ++bin) {
        EXPECT_NEAR(magnitudes[bin], 0.0F, 1.0e-4F);
    }
}

TEST(RealFftTest, ReportsNyquistBin)
{
    constexpr int fftSize = 1024;

    const Fooyin::Dsp::RealFft fft{fftSize};
    ASSERT_TRUE(fft.isValid());

    std::vector input(fftSize, 0.0F);
    for(int index{0}; index < fftSize; ++index) {
        input[static_cast<size_t>(index)] = (index % 2 == 0) ? 1.0F : -1.0F;
    }

    std::vector magnitudes(static_cast<size_t>(fft.binCount()), 0.0F);
    ASSERT_TRUE(fft.transformMagnitudes(input, magnitudes));

    EXPECT_NEAR(magnitudes.back(), 1.0F, 1.0e-4F);
    for(size_t bin{0}; bin + 1 < magnitudes.size(); ++bin) {
        EXPECT_NEAR(magnitudes[bin], 0.0F, 1.0e-4F);
    }
}

TEST(RealFftTest, SelectsSupportedSizeNearestRequest)
{
    for(const int requestedSize : {4101, 8928, 17856, 35712}) {
        const int selectedSize = Fooyin::Dsp::RealFft::nearestValidSize(requestedSize);
        EXPECT_GT(selectedSize, 0);

        const Fooyin::Dsp::RealFft fft{selectedSize};
        EXPECT_TRUE(fft.isValid());
        EXPECT_LT(std::abs(selectedSize - requestedSize), requestedSize / 10);
    }

    EXPECT_EQ(Fooyin::Dsp::RealFft::nearestValidSize(0), 0);
}
} // namespace
