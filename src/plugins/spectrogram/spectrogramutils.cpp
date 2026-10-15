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

#include "spectrogramutils.h"

#include <cmath>

constexpr auto MinimumLogFrequencyHz = 25.0;

namespace Fooyin::Spectrogram {
namespace {
double binPositionForRatio(const VisualisationSession::SpectrumWindow& spectrum, bool logFrequency, double ratio)
{
    const int maxBinIndex = spectrum.binCount() - 1;
    if(maxBinIndex <= 0) {
        return 0.0;
    }

    const double clampedRatio = std::clamp(ratio, 0.0, 1.0);

    if(logFrequency) {
        const double binFrequency = static_cast<double>(spectrum.sampleRate) / static_cast<double>(spectrum.fftSize);
        const double minimumBin
            = std::clamp(MinimumLogFrequencyHz / binFrequency, 1.0, static_cast<double>(maxBinIndex));
        return std::exp(std::lerp(std::log(minimumBin), std::log(static_cast<double>(maxBinIndex)), clampedRatio));
    }

    return clampedRatio * static_cast<double>(maxBinIndex);
}

double magnitudeDb(float magnitude)
{
    static constexpr double MinMagnitude = 1.0e-12;
    return 20.0 * std::log10(std::max<double>(magnitude, MinMagnitude));
}

float sampleMagnitude(const VisualisationSession::SpectrumWindow& spectrum, bool logFrequency, int row, int rows)
{
    if(!spectrum.isValid() || rows <= 0) {
        return 0.0F;
    }

    const int maxBinIndex = spectrum.binCount() - 1;
    if(maxBinIndex <= 0) {
        return 0.0F;
    }

    const double upperRatio = 1.0 - (static_cast<double>(row) / static_cast<double>(rows));
    const double lowerRatio = 1.0 - (static_cast<double>(row + 1) / static_cast<double>(rows));
    const double binStart   = binPositionForRatio(spectrum, logFrequency, lowerRatio);
    const double binEnd     = binPositionForRatio(spectrum, logFrequency, upperRatio);

    const auto bins = spectrum.bins();

    if(binEnd - binStart < 1.0) {
        const double center = std::clamp((binStart + binEnd) * 0.5, 0.0, static_cast<double>(maxBinIndex));
        const int lowerBin  = static_cast<int>(std::floor(center));
        const int upperBin  = std::min(lowerBin + 1, maxBinIndex);
        return static_cast<float>(std::lerp(static_cast<double>(bins[static_cast<size_t>(lowerBin)]),
                                            static_cast<double>(bins[static_cast<size_t>(upperBin)]),
                                            center - static_cast<double>(lowerBin)));
    }

    double peak{0.0};

    // Use only bins whose centres fall inside this pixel's frequency interval
    const int firstBin = std::clamp(static_cast<int>(std::ceil(binStart)), 0, maxBinIndex);
    const int endBin   = std::clamp(static_cast<int>(std::ceil(binEnd)), firstBin + 1, maxBinIndex + 1);

    for(int bin{firstBin}; bin < endBin; ++bin) {
        const double magnitude = bins[static_cast<size_t>(bin)];
        peak                   = std::max(peak, magnitude);
    }

    return static_cast<float>(peak);
}
} // namespace

QColor colourForLevel(double level, const std::vector<QColor>& colours)
{
    if(colours.size() == 1) {
        return colours.front();
    }

    const double position = std::clamp(level, 0.0, 1.0) * static_cast<double>(colours.size() - 1);
    const auto first      = static_cast<size_t>(std::floor(position));
    const size_t second   = std::min(first + 1, colours.size() - 1);
    const auto fraction   = static_cast<float>(position - static_cast<double>(first));

    const QColor& from = colours[first];
    const QColor& to   = colours[second];

    return QColor::fromRgbF(
        std::lerp(from.redF(), to.redF(), fraction), std::lerp(from.greenF(), to.greenF(), fraction),
        std::lerp(from.blueF(), to.blueF(), fraction), std::lerp(from.alphaF(), to.alphaF(), fraction));
}

void fillSpectrumSection(std::vector<float>& levels, int top, int height,
                         const VisualisationSession::SpectrumWindow& spectrum, const SpectrogramConfig& config)
{
    if(height <= 0 || top < 0 || std::cmp_greater(top + height, levels.size())) {
        return;
    }

    const auto levelSpan = static_cast<double>(std::max(1, config.maxDb - config.minDb));

    for(int row{0}; row < height; ++row) {
        const double db = magnitudeDb(sampleMagnitude(spectrum, config.logFrequency, row, height));
        const float level
            = db < static_cast<double>(config.minDb)
                ? -1.0F
                : static_cast<float>(std::clamp((db - static_cast<double>(config.minDb)) / levelSpan, 0.0, 1.0));
        levels[top + row] = level;
    }
}
} // namespace Fooyin::Spectrogram
