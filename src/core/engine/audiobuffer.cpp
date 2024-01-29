/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiobuffer.h>

#include <QDebug>

#include <ranges>
#include <utility>

namespace Fooyin {
struct AudioBuffer::Private : public QSharedData
{
public:
    std::vector<std::byte> data;
    AudioFormat format;
    uint64_t startTime;

    Private(std::span<const std::byte> data, AudioFormat format, uint64_t startTime)
        : format{format}
        , startTime{startTime}
    {
        this->data.assign(data.begin(), data.end());
    }

    void fillSilence()
    {
        const bool unsignedFormat = format.sampleFormat() == Fooyin::AudioFormat::UInt8;
        std::ranges::fill(data, unsignedFormat ? std::byte{0x80} : std::byte{0});
    }

    void fillRemainingWithSilence()
    {
        const bool unsignedFormat = format.sampleFormat() == Fooyin::AudioFormat::UInt8;
        std::fill(data.begin() + data.size(), data.begin() + data.capacity(),
                  unsignedFormat ? std::byte{0x80} : std::byte{0});
    }
};

AudioBuffer::AudioBuffer() = default;

AudioBuffer::AudioBuffer(std::span<const std::byte> data, AudioFormat format, uint64_t startTime)
    : p{new Private(data, format, startTime)}
{ }

void AudioBuffer::reserve(size_t size)
{
    if(!!p) {
        p->data.reserve(size);
    }
}

void AudioBuffer::append(std::span<const std::byte> data)
{
    if(!!p) {
        p->data.insert(p->data.end(), data.begin(), data.end());
    }
}

void AudioBuffer::clear()
{
    if(!!p) {
        p->data.clear();
    }
}

void AudioBuffer::reset()
{
    if(!!p) {
        p.reset();
    }
}

AudioBuffer::AudioBuffer(const uint8_t* data, int size, AudioFormat format, uint64_t startTime)
    : AudioBuffer{{std::bit_cast<const std::byte*>(data), static_cast<std::size_t>(size)}, format, startTime}
{ }

AudioBuffer::~AudioBuffer() = default;

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept = default;
AudioBuffer::AudioBuffer(const AudioBuffer& other)     = default;

AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other) = default;

bool AudioBuffer::isValid() const
{
    return !!p;
}

void AudioBuffer::detach()
{
    p = new Private(*p);
}

AudioFormat AudioBuffer::format() const
{
    return !!p ? p->format : AudioFormat{};
}

int AudioBuffer::frameCount() const
{
    return !!p ? p->format.framesForBytes(byteCount()) : 0;
}

int AudioBuffer::sampleCount() const
{
    return frameCount() * format().channelCount();
}

int AudioBuffer::byteCount() const
{
    return !!p ? static_cast<int>(p->data.size()) : 0;
}

uint64_t AudioBuffer::startTime() const
{
    return !!p ? p->startTime : -1;
}

uint64_t AudioBuffer::duration() const
{
    return format().durationForFrames(frameCount());
}

std::span<const std::byte> AudioBuffer::constData() const
{
    return p->data;
}

const std::byte* AudioBuffer::data() const
{
    return p->data.data();
}

std::byte* AudioBuffer::data()
{
    return p->data.data();
}

void AudioBuffer::fillSilence()
{
    if(!!p) {
        p->fillSilence();
    }
}

void AudioBuffer::fillRemainingWithSilence()
{
    if(!!p) {
        p->fillRemainingWithSilence();
    }
}

void AudioBuffer::adjustVolumeOfSamples(double volume)
{
    if(volume == 1.0) {
        return;
    }

    if(volume == 0.0) {
        fillSilence();
        return;
    }

    const int bytes = byteCount();
    const int bps   = format().bytesPerSample();
    const auto vol  = static_cast<float>(volume);

    switch(format().sampleFormat()) {
        case(AudioFormat::SampleFormat::UInt8): {
            auto* samples = std::bit_cast<int8_t*>(data());
            for(int i{0}; i < bytes; ++i) {
                samples[i] = static_cast<int8_t>(static_cast<float>(samples[i]) * vol);
            }
            break;
        }
        case(AudioFormat::SampleFormat::Int16): {
            auto* samples   = std::bit_cast<int16_t*>(data());
            const int count = bytes / bps;
            for(int i{0}; i < count; ++i) {
                samples[i] = static_cast<int16_t>(static_cast<float>(samples[i]) * vol);
            }
            break;
        }
        case(AudioFormat::SampleFormat::Int32): {
            auto* samples   = std::bit_cast<int32_t*>(data());
            const int count = bytes / bps;
            for(int i{0}; i < count; ++i) {
                const auto offset1 = static_cast<int>(i * bps);
                const auto offset2 = offset1 + 1;
                const auto offset3 = offset1 + 2;

                const int32_t sample
                    = (static_cast<int8_t>(samples[offset1]) | static_cast<int8_t>(samples[offset2] << 8)
                       | static_cast<int8_t>(samples[offset3] << 16));

                const auto newSample = static_cast<int32_t>(static_cast<float>(sample) * vol);

                samples[offset1] = static_cast<uint8_t>(newSample & 0x0000FF);
                samples[offset2] = static_cast<uint8_t>((newSample & 0x00FF00) >> 8);
                samples[offset3] = static_cast<uint8_t>((newSample & 0xFF0000) >> 16);
            }
            break;
        }
        case(AudioFormat::SampleFormat::Float): {
            const int count = bytes / bps;
            auto* samples   = std::bit_cast<float*>(data());
            for(int i{0}; i < count; ++i) {
                samples[i] = samples[i] * vol;
            }
            break;
        }
        case(AudioFormat::SampleFormat::Double): {
            const int count = bytes / bps;
            auto* samples   = std::bit_cast<double*>(data());
            for(int i{0}; i < count; ++i) {
                samples[i] = static_cast<double>(static_cast<float>(samples[i]) * vol);
            }
            break;
        }
        default:
            qDebug() << "Unable to adjust volume of unsupported format";
    }
}
} // namespace Fooyin
