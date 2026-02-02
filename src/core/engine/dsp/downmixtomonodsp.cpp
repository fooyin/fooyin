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

#include "downmixtomonodsp.h"

#include <numbers>

constexpr double InvSqrt2 = 1.0 / std::numbers::sqrt2_v<double>;

namespace {
using P = Fooyin::AudioFormat::ChannelPosition;

constexpr double weightForPosition(P pos) noexcept
{
    switch(pos) {
        // Front stage
        case P::FrontLeft:
        case P::FrontRight:
            return 0.5;
        case P::FrontCenter:
            return InvSqrt2;
        case P::FrontLeftOfCenter:
        case P::FrontRightOfCenter:
            return 0.5;
            // Low-frequency effects
        case P::LFE:
        case P::LFE2:
            return 0.0;
            // Surround / rear
        case P::SideLeft:
        case P::SideRight:
        case P::BackLeft:
        case P::BackRight:
        case P::BackCenter:
            return 0.5;
            // Top layer
        case P::TopFrontLeft:
        case P::TopFrontRight:
        case P::TopFrontCenter:
        case P::TopSideLeft:
        case P::TopSideRight:
        case P::TopCenter:
        case P::TopBackLeft:
        case P::TopBackRight:
        case P::TopBackCenter:
            return 0.5;
            // Bottom layer
        case P::BottomFrontLeft:
        case P::BottomFrontRight:
        case P::BottomFrontCenter:
            return 0.5;
        case P::UnknownPosition:
        default:
            return 0.5;
    }
}
} // namespace

namespace Fooyin {
QString DownmixToMonoDsp::name() const
{
    return QStringLiteral("Downmix to mono");
}

QString DownmixToMonoDsp::id() const
{
    return QStringLiteral("fooyin.dsp.downmixtomono");
}

AudioFormat DownmixToMonoDsp::outputFormat(const AudioFormat& input) const
{
    if(!input.isValid()) {
        return {};
    }

    if(input.channelCount() == 1) {
        return input;
    }

    AudioFormat out{input};
    out.setChannelCount(1);
    out.setChannelLayout({AudioFormat::ChannelPosition::FrontCenter});

    return out;
}

void DownmixToMonoDsp::prepare(const AudioFormat& format)
{
    m_format = format;
    m_format.setChannelCount(1);
    m_format.setChannelLayout({P::FrontCenter});
}

void DownmixToMonoDsp::process(ProcessingBufferList& chunks)
{
    const size_t count = chunks.count();
    if(count == 0) {
        return;
    }

    ProcessingBufferList output;

    for(size_t i{0}; i < count; ++i) {
        const auto* buffer = chunks.item(i);
        if(!buffer || !buffer->isValid()) {
            continue;
        }

        const auto format    = buffer->format();
        const int inChannels = format.channelCount();
        if(inChannels <= 0) {
            output.addChunk(*buffer);
            continue;
        }

        if(inChannels == 1) {
            output.addChunk(*buffer);
            continue;
        }

        const int frames = buffer->frameCount();
        if(frames <= 0) {
            output.addChunk(*buffer);
            continue;
        }

        const auto inSamples = buffer->constData();
        if(inSamples.empty()) {
            output.addChunk(*buffer);
            continue;
        }

        if(static_cast<size_t>(inChannels) > AudioFormat::MaxChannels) {
            output.addChunk(*buffer);
            continue;
        }

        auto layoutView = format.channelLayoutView();
        AudioFormat::ChannelLayout fallbackLayout;
        if(!std::cmp_equal(layoutView.size(), inChannels)) {
            fallbackLayout = AudioFormat::defaultChannelLayoutForChannelCount(inChannels);
            layoutView     = std::span<const P>{fallbackLayout};
        }
        const bool layoutOk = std::cmp_equal(layoutView.size(), inChannels);

        std::array<double, AudioFormat::MaxChannels> w{};
        double weightSum{0.0};

        for(int ch{0}; ch < inChannels; ++ch) {
            const auto idx  = static_cast<size_t>(ch);
            const double ww = layoutOk ? weightForPosition(layoutView[idx]) : 0.5;
            w[idx]          = ww;
            weightSum += ww;
        }

        const double norm = (weightSum > 0.0) ? (1.0 / weightSum) : 1.0;

        const auto outSamples = static_cast<size_t>(frames);
        auto* outBuffer       = output.addItem(m_format, buffer->startTimeNs(), outSamples);
        if(!outBuffer) {
            output.addChunk(*buffer);
            continue;
        }

        auto outSpan = outBuffer->data();
        if(outSpan.size() < outSamples) {
            output.addChunk(*buffer);
            continue;
        }

        for(int frame{0}; frame < frames; ++frame) {
            const size_t inBase = static_cast<size_t>(frame) * static_cast<size_t>(inChannels);
            double acc{0.0};

            for(int ch{0}; ch < inChannels; ++ch) {
                const auto idx = static_cast<size_t>(ch);
                acc += inSamples[inBase + idx] * w[idx];
            }

            outSpan[static_cast<size_t>(frame)] = acc * norm;
        }
    }

    chunks.clear();

    const size_t outCount = output.count();
    for(size_t i{0}; i < outCount; ++i) {
        const auto* buffer = output.item(i);
        if(buffer && buffer->isValid()) {
            chunks.addChunk(*buffer);
        }
    }
}
} // namespace Fooyin
