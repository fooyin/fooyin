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

#include <core/engine/audioformat.h>

#include <QStringList>

#include <algorithm>
#include <tuple>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString channelPositionName(AudioFormat::ChannelPosition pos)
{
    using P = AudioFormat::ChannelPosition;
    // clang-format off
    switch(pos) {
        case P::FrontLeft:           return u"FL"_s;   case P::FrontRight:          return u"FR"_s;
        case P::FrontCenter:         return u"FC"_s;   case P::LFE:                 return u"LFE"_s;
        case P::BackLeft:            return u"BL"_s;   case P::BackRight:           return u"BR"_s;
        case P::FrontLeftOfCenter:   return u"FLC"_s;  case P::FrontRightOfCenter:  return u"FRC"_s;
        case P::BackCenter:          return u"BC"_s;   case P::SideLeft:            return u"SL"_s;
        case P::SideRight:           return u"SR"_s;   case P::TopCenter:           return u"TC"_s;
        case P::TopFrontLeft:        return u"TFL"_s;  case P::TopFrontCenter:      return u"TFC"_s;
        case P::TopFrontRight:       return u"TFR"_s;  case P::TopBackLeft:         return u"TBL"_s;
        case P::TopBackCenter:       return u"TBC"_s;  case P::TopBackRight:        return u"TBR"_s;
        case P::LFE2:                return u"LFE2"_s; case P::TopSideLeft:         return u"TSL"_s;
        case P::TopSideRight:        return u"TSR"_s;  case P::BottomFrontCenter:   return u"BFC"_s;
        case P::BottomFrontLeft:     return u"BFL"_s;  case P::BottomFrontRight:    return u"BFR"_s;
        case P::UnknownPosition:
        default:                     return u"?"_s;
    }
    // clang-format on
}
} // namespace

AudioFormat::AudioFormat()
    : AudioFormat{SampleFormat::Unknown, 0, 0}
{ }

AudioFormat::AudioFormat(SampleFormat format, int sampleRate, int channelCount)
    : m_sampleFormat{format}
    , m_channelCount{channelCount}
    , m_sampleRate{sampleRate}
    , m_hasChannelLayout{false}
{
    if(m_channelCount < 0 || m_channelCount > MaxChannels) {
        m_channelCount = 0;
    }

    m_channelLayout.fill(ChannelPosition::UnknownPosition);
    applyDefaultChannelLayout();
}

bool AudioFormat::operator==(const AudioFormat& other) const noexcept
{
    if(std::tie(m_sampleFormat, m_channelCount, m_sampleRate)
       != std::tie(other.m_sampleFormat, other.m_channelCount, other.m_sampleRate)) {
        return false;
    }

    if(hasChannelLayout() != other.hasChannelLayout()) {
        return false;
    }

    if(!hasChannelLayout()) {
        return true;
    }

    return std::equal(m_channelLayout.begin(), m_channelLayout.begin() + m_channelCount, other.m_channelLayout.begin());
}

bool AudioFormat::operator!=(const AudioFormat& other) const noexcept
{
    return !(*this == other);
}

bool AudioFormat::isValid() const
{
    return m_sampleRate > 0 && m_channelCount > 0 && m_channelCount <= MaxChannels
        && m_sampleFormat != SampleFormat::Unknown;
}

int AudioFormat::sampleRate() const
{
    return m_sampleRate;
}

int AudioFormat::channelCount() const
{
    return m_channelCount;
}

SampleFormat AudioFormat::sampleFormat() const
{
    return m_sampleFormat;
}

bool AudioFormat::hasChannelLayout() const
{
    return m_hasChannelLayout && m_channelCount > 0 && m_channelCount <= MaxChannels;
}

std::span<const AudioFormat::ChannelPosition> AudioFormat::channelLayoutView() const noexcept
{
    if(!hasChannelLayout()) {
        return {};
    }

    const auto n = static_cast<size_t>(std::clamp(m_channelCount, 0, MaxChannels));
    return {m_channelLayout.data(), n};
}

AudioFormat::ChannelPosition AudioFormat::channelPosition(int channelIndex) const
{
    if(!hasChannelLayout() || channelIndex < 0 || channelIndex >= m_channelCount) {
        return ChannelPosition::UnknownPosition;
    }

    return m_channelLayout.at(channelIndex);
}

AudioFormat::ChannelLayout AudioFormat::channelLayout() const
{
    if(!hasChannelLayout()) {
        return {};
    }

    ChannelLayout layout(static_cast<size_t>(m_channelCount));
    std::copy_n(m_channelLayout.begin(), m_channelCount, layout.begin());

    return layout;
}

void AudioFormat::setSampleRate(int sampleRate)
{
    m_sampleRate = sampleRate;
}

void AudioFormat::setChannelCount(int channelCount)
{
    if(channelCount < 0 || channelCount > MaxChannels || m_channelCount == channelCount) {
        return;
    }

    m_channelCount = channelCount;

    applyDefaultChannelLayout();
}

bool AudioFormat::setChannelLayout(const ChannelLayout& layout)
{
    const auto n = static_cast<int>(layout.size());

    if(m_channelCount <= 0 || m_channelCount > MaxChannels || n != m_channelCount) {
        return false;
    }

    m_channelLayout.fill(ChannelPosition::UnknownPosition);

    std::copy_n(layout.begin(), n, m_channelLayout.begin());
    m_hasChannelLayout = true;

    return true;
}

bool AudioFormat::setChannelLayoutAndCount(const ChannelLayout& layout)
{
    const auto n = static_cast<int>(layout.size());

    if(n <= 0 || n > MaxChannels) {
        return false;
    }

    m_channelCount = n;

    return setChannelLayout(layout);
}

void AudioFormat::clearChannelLayout()
{
    m_hasChannelLayout = false;
    m_channelLayout.fill(ChannelPosition::UnknownPosition);
}

void AudioFormat::setSampleFormat(SampleFormat format)
{
    m_sampleFormat = format;
}

AudioFormat::ChannelLayout AudioFormat::defaultChannelLayoutForChannelCount(int channelCount)
{
    using P = ChannelPosition;
    switch(channelCount) {
        case 1:
            return {P::FrontCenter};
        case 2:
            return {P::FrontLeft, P::FrontRight};
        case 3:
            return {P::FrontLeft, P::FrontRight, P::FrontCenter};
        case 4:
            return {P::FrontLeft, P::FrontRight, P::BackLeft, P::BackRight};
        case 5:
            return {P::FrontLeft, P::FrontRight, P::FrontCenter, P::BackLeft, P::BackRight};
        case 6:
            return {P::FrontLeft, P::FrontRight, P::FrontCenter, P::LFE, P::BackLeft, P::BackRight};
        case 7:
            return {P::FrontLeft, P::FrontRight, P::FrontCenter, P::BackLeft, P::BackRight, P::SideLeft, P::SideRight};
        case 8:
            return {P::FrontLeft, P::FrontRight, P::FrontCenter, P::LFE,
                    P::BackLeft,  P::BackRight,  P::SideLeft,    P::SideRight};
        default:
            return {};
    }
}

uint64_t AudioFormat::bytesForDuration(uint64_t ms) const
{
    if(!isValid() || ms == 0) {
        return 0;
    }

    const uint64_t secs = ms / 1000;
    const uint64_t rem  = ms % 1000;

    const auto sRate = static_cast<uint64_t>(sampleRate());
    const auto bpf   = static_cast<uint64_t>(bytesPerFrame());

    if(secs > std::numeric_limits<uint64_t>::max() / sRate) {
        return std::numeric_limits<uint64_t>::max();
    }

    const uint64_t frames = (secs * sRate) + ((rem * sRate) / 1000);

    if(frames > std::numeric_limits<uint64_t>::max() / bpf) {
        return std::numeric_limits<uint64_t>::max();
    }

    return frames * bpf;
}

uint64_t AudioFormat::durationForBytes(uint64_t byteCount) const
{
    if(!isValid() || byteCount == 0) {
        return 0;
    }

    const auto bpf   = static_cast<uint64_t>(bytesPerFrame());
    const auto sRate = static_cast<uint64_t>(sampleRate());

    const uint64_t frames = byteCount / bpf;
    return ((frames / sRate) * 1000) + ((frames % sRate) * 1000 / sRate);
}

uint64_t AudioFormat::bytesForFrames(int frameCount) const
{
    if(!isValid() || frameCount <= 0) {
        return 0;
    }

    return static_cast<uint64_t>(frameCount) * static_cast<uint64_t>(bytesPerFrame());
}

int AudioFormat::framesForBytes(uint64_t byteCount) const
{
    if(!isValid() || byteCount == 0) {
        return 0;
    }

    const auto bpf = uint64_t(bytesPerFrame());

    const uint64_t frames = byteCount / bpf;
    if(frames > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }

    return static_cast<int>(frames);
}

int AudioFormat::framesForDuration(uint64_t ms) const
{
    if(!isValid()) {
        return 0;
    }

    const uint64_t secs = ms / 1000;
    const uint64_t rem  = ms % 1000;

    const auto sRate = static_cast<uint64_t>(sampleRate());

    if(secs > uint64_t(std::numeric_limits<int>::max()) / sRate) {
        return std::numeric_limits<int>::max();
    }

    const uint64_t frames = (secs * sRate) + ((rem * sRate) / 1000);

    return static_cast<int>(frames);
}

uint64_t AudioFormat::durationForFrames(int frameCount) const
{
    if(!isValid() || frameCount <= 0) {
        return 0;
    }

    return static_cast<uint64_t>(frameCount) * 1000 / sampleRate();
}

int AudioFormat::bytesPerFrame() const
{
    return bytesPerSample() * channelCount();
}

int AudioFormat::bytesPerSample() const
{
    switch(m_sampleFormat) {
        case(SampleFormat::U8):
            return 1;
        case(SampleFormat::S16):
            return 2;
        case(SampleFormat::S24In32):
            // Stored in low three bytes of 32bit int
        case(SampleFormat::S32):
        case(SampleFormat::F32):
            return 4;
        case(SampleFormat::F64):
            return 8;
        case(SampleFormat::Unknown):
        default:
            return 0;
    }
}

int AudioFormat::bitsPerSample() const
{
    return bytesPerSample() * 8;
}

QString AudioFormat::prettyFormat() const
{
    switch(m_sampleFormat) {
        case(SampleFormat::U8):
            return u"8 bit (unsigned)"_s;
        case(SampleFormat::S16):
            return u"16 bit (signed)"_s;
        case(SampleFormat::S24In32):
            return u"24 bit (signed)"_s;
        case(SampleFormat::S32):
            return u"32 bit (signed)"_s;
        case(SampleFormat::F32):
            return u"32 bit (float)"_s;
        case(SampleFormat::F64):
            return u"64 bit (float)"_s;
        case(SampleFormat::Unknown):
        default:
            return u"Unknown format"_s;
    }
}

QString AudioFormat::prettyChannelLayout() const
{
    if(!hasChannelLayout()) {
        return u"unknown"_s;
    }

    QStringList names;
    names.reserve(m_channelCount);

    for(auto i{0}; i < m_channelCount; ++i) {
        names.push_back(channelPositionName(m_channelLayout.at(i)));
    }

    return names.join(u","_s);
}

void AudioFormat::applyDefaultChannelLayout()
{
    m_channelLayout.fill(ChannelPosition::UnknownPosition);
    m_hasChannelLayout = false;

    if(m_channelCount <= 0 || m_channelCount > MaxChannels) {
        return;
    }

    const auto layout = defaultChannelLayoutForChannelCount(m_channelCount);
    if(std::cmp_not_equal(layout.size(), m_channelCount)) {
        return;
    }

    std::ranges::copy(layout, m_channelLayout.begin());
    m_hasChannelLayout = true;
}
} // namespace Fooyin
