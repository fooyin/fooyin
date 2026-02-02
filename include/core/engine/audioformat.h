/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QString>

#include <array>
#include <cstdint>
#include <span>

namespace Fooyin {
/*!
 * PCM sample storage type used by engine buffers and output negotiation.
 */
enum class SampleFormat : uint8_t
{
    Unknown = 0,
    U8,
    S16,
    S24In32,
    S32,
    F32,
    F64,
};

/*!
 * Audio format descriptor used across decode, DSP and output stages.
 *
 * The format describes sample type, rate and channels. Channel layout is
 * optional; when absent, channel count still defines interleaving/stride rules.
 *
 * Conversion helpers (`bytesForDuration`, `framesForBytes`, etc.) clamp on
 * overflow where needed instead of wrapping.
 */
class FYCORE_EXPORT AudioFormat
{
public:
    enum class ChannelPosition : uint8_t
    {
        UnknownPosition = 0,
        FrontLeft,
        FrontRight,
        FrontCenter,
        LFE,
        BackLeft,
        BackRight,
        FrontLeftOfCenter,
        FrontRightOfCenter,
        BackCenter,
        SideLeft,
        SideRight,
        TopCenter,
        TopFrontLeft,
        TopFrontCenter,
        TopFrontRight,
        TopBackLeft,
        TopBackCenter,
        TopBackRight,
        LFE2,
        TopSideLeft,
        TopSideRight,
        BottomFrontCenter,
        BottomFrontLeft,
        BottomFrontRight,
    };

    static constexpr int MaxChannels = 32;
    using ChannelLayout              = std::vector<ChannelPosition>;

    AudioFormat();
    AudioFormat(SampleFormat format, int sampleRate, int channelCount);

    bool operator==(const AudioFormat& other) const noexcept;
    bool operator!=(const AudioFormat& other) const noexcept;

    //! True when sample format, sample rate and channel count describe a usable PCM stream.
    [[nodiscard]] bool isValid() const;

    [[nodiscard]] int sampleRate() const;
    [[nodiscard]] int channelCount() const;
    [[nodiscard]] SampleFormat sampleFormat() const;
    [[nodiscard]] bool hasChannelLayout() const;
    [[nodiscard]] ChannelPosition channelPosition(int channelIndex) const;
    [[nodiscard]] ChannelLayout channelLayout() const;
    [[nodiscard]] std::span<const ChannelPosition> channelLayoutView() const noexcept;

    void setSampleRate(int sampleRate);
    void setChannelCount(int channelCount);
    bool setChannelLayout(const ChannelLayout& layout);
    bool setChannelLayoutAndCount(const ChannelLayout& layout);
    void clearChannelLayout();
    void setSampleFormat(SampleFormat format);
    [[nodiscard]] static ChannelLayout defaultChannelLayoutForChannelCount(int channelCount);

    //! Convert duration to byte count for this format (saturates on overflow).
    [[nodiscard]] uint64_t bytesForDuration(uint64_t ms) const;
    //! Convert byte count to duration for this format.
    [[nodiscard]] uint64_t durationForBytes(uint64_t byteCount) const;

    //! Convert frame count to byte count.
    [[nodiscard]] uint64_t bytesForFrames(int frameCount) const;
    //! Convert byte count to frame count (clamped to `int` range).
    [[nodiscard]] int framesForBytes(uint64_t byteCount) const;

    //! Convert duration to frames (clamped to `int` range).
    [[nodiscard]] int framesForDuration(uint64_t ms) const;
    //! Convert frame count to duration.
    [[nodiscard]] uint64_t durationForFrames(int frameCount) const;

    [[nodiscard]] int bytesPerFrame() const;
    [[nodiscard]] int bytesPerSample() const;
    [[nodiscard]] int bitsPerSample() const;

    [[nodiscard]] QString prettyFormat() const;
    [[nodiscard]] QString prettyChannelLayout() const;

private:
    void applyDefaultChannelLayout();

    SampleFormat m_sampleFormat;
    int m_channelCount;
    int m_sampleRate;
    bool m_hasChannelLayout;
    std::array<ChannelPosition, MaxChannels> m_channelLayout;
};
} // namespace Fooyin
