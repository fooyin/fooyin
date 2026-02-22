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

#include "downmixtostereodsp.h"

#include <numbers>

constexpr double InvSqrt2 = 1.0 / std::numbers::sqrt2_v<double>;

namespace {
using P = Fooyin::AudioFormat::ChannelPosition;

std::pair<double, double> stereoWeightsForChannel(P position, int channelIndex) noexcept
{
    switch(position) {
        case P::FrontLeft:
            return {1.0, 0.0};
        case P::FrontRight:
            return {0.0, 1.0};
        case P::FrontCenter:
            return {InvSqrt2, InvSqrt2};
        case P::LFE:
        case P::LFE2:
            return {0.0, 0.0};
        case P::BackLeft:
        case P::SideLeft:
        case P::TopFrontLeft:
        case P::TopBackLeft:
        case P::TopSideLeft:
        case P::FrontLeftOfCenter:
        case P::BottomFrontLeft:
            return {InvSqrt2, 0.0};
        case P::BackRight:
        case P::SideRight:
        case P::TopFrontRight:
        case P::TopBackRight:
        case P::TopSideRight:
        case P::FrontRightOfCenter:
        case P::BottomFrontRight:
            return {0.0, InvSqrt2};
        case P::BackCenter:
        case P::TopCenter:
        case P::TopFrontCenter:
        case P::TopBackCenter:
        case P::BottomFrontCenter:
            return {InvSqrt2, InvSqrt2};
        case P::UnknownPosition:
        default: {
            if(channelIndex == 0) {
                return {1.0, 0.0};
            }
            if(channelIndex == 1) {
                return {0.0, 1.0};
            }
            return {0.5, 0.5};
        }
    }
}
} // namespace

namespace Fooyin {
QString DownmixToStereoDsp::name() const
{
    return QStringLiteral("Downmix to stereo");
}

QString DownmixToStereoDsp::id() const
{
    return QStringLiteral("fooyin.dsp.downmixtostereo");
}

AudioFormat DownmixToStereoDsp::outputFormat(const AudioFormat& input) const
{
    if(!input.isValid()) {
        return {};
    }

    if(input.channelCount() == 2) {
        return input;
    }

    AudioFormat out{input};
    out.setChannelCount(2);
    out.setChannelLayout({P::FrontLeft, P::FrontRight});

    return out;
}

void DownmixToStereoDsp::prepare(const AudioFormat& format)
{
    m_format = format;
    m_format.setChannelCount(2);
    m_format.setChannelLayout({P::FrontLeft, P::FrontRight});
}

void DownmixToStereoDsp::process(ProcessingBufferList& chunks)
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

        if(inChannels == 2) {
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

        auto layoutView = format.channelLayoutView();
        AudioFormat::ChannelLayout fallbackLayout;

        if(!std::cmp_equal(layoutView.size(), inChannels)) {
            fallbackLayout = AudioFormat::defaultChannelLayoutForChannelCount(inChannels);
            layoutView     = std::span<const P>{fallbackLayout};
        }

        const bool layoutOk = std::cmp_equal(layoutView.size(), inChannels);

        const auto outSamples = static_cast<size_t>(frames) * 2;
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

        std::array<std::pair<double, double>, AudioFormat::MaxChannels> weights{};
        double sumL{0.0};
        double sumR{0.0};

        for(int ch{0}; ch < inChannels; ++ch) {
            const size_t idx = static_cast<size_t>(ch);
            const P pos      = layoutOk ? layoutView[ch] : P::UnknownPosition;
            const auto w     = stereoWeightsForChannel(pos, ch);
            weights[idx]     = w;
            sumL += w.first;
            sumR += w.second;
        }

        const double normL = (sumL > 0.0) ? (1.0 / sumL) : 1.0;
        const double normR = (sumR > 0.0) ? (1.0 / sumR) : 1.0;

        for(int frame{0}; frame < frames; ++frame) {
            const auto inBase = static_cast<size_t>(frame) * static_cast<size_t>(inChannels);

            double left{0.0};
            double right{0.0};

            for(int ch{0}; ch < inChannels; ++ch) {
                const double sample = inSamples[inBase + static_cast<size_t>(ch)];
                const auto [wL, wR] = weights[static_cast<size_t>(ch)];
                left += sample * wL;
                right += sample * wR;
            }

            const auto outBase   = static_cast<size_t>(frame) * 2;
            outSpan[outBase]     = left * normL;
            outSpan[outBase + 1] = right * normR;
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
