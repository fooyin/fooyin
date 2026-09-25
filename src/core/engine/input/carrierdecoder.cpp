/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "carrierdecoder.h"

#include "ffmpeg/ffmpeginput.h"

#include <QByteArray>
#include <QIODevice>
#include <QLoggingCategory>

extern "C"
{
#include <libavformat/avformat.h>
}

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(CARRIER_DECODER, "fy.audio.carrier")

using namespace Qt::StringLiterals;

namespace {
constexpr qsizetype ProbeBytes            = 256 * 1024;
constexpr int MinimumProbeScore           = AVPROBE_SCORE_EXTENSION + 1;
constexpr int MaximumCarrierDrainAttempts = 16;

bool canContainCarrier(const Fooyin::AudioSource& source, const Fooyin::AudioFormat& format)
{
    return !source.remoteStreamDevice && format.sampleFormat() == Fooyin::SampleFormat::S16
        && format.channelCount() == 2;
}

const AVInputFormat* probeCarrier(const QByteArray& data)
{
    if(data.isEmpty() || data.size() > std::numeric_limits<int>::max()) {
        return nullptr;
    }

    QByteArray padded = data;
    padded.resize(data.size() + AVPROBE_PADDING_SIZE);
    std::memset(padded.data() + data.size(), 0, AVPROBE_PADDING_SIZE);

    AVProbeData probe{};
    probe.buf      = reinterpret_cast<unsigned char*>(padded.data());
    probe.buf_size = static_cast<int>(data.size());

    int score{0};
    const AVInputFormat* format = av_probe_input_format3(&probe, 1, &score);
    return score >= MinimumProbeScore ? format : nullptr;
}
} // namespace

namespace Fooyin {
class DecodedAudioDevice final : public QIODevice
{
public:
    DecodedAudioDevice(AudioDecoder* decoder, AudioFormat format, QByteArray prefix)
        : m_decoder{decoder}
        , m_format{std::move(format)}
        , m_prefix{std::move(prefix)}
    {
        open(QIODevice::ReadOnly);
    }

    bool isSequential() const override
    {
        return true;
    }

protected:
    qint64 readData(char* data, qint64 maxSize) override
    {
        if(maxSize <= 0) {
            return 0;
        }

        qint64 written{0};
        if(m_prefixOffset < m_prefix.size()) {
            const qsizetype available = m_prefix.size() - m_prefixOffset;
            const qsizetype count     = std::min<qsizetype>(available, maxSize);
            std::memcpy(data, m_prefix.constData() + m_prefixOffset, static_cast<size_t>(count));
            m_prefixOffset += count;
            written += count;
        }

        while(written < maxSize) {
            if(m_buffer.isValid() && m_bufferOffset < m_buffer.byteCount()) {
                const int available = m_buffer.byteCount() - m_bufferOffset;
                const int count     = std::min<int>(available, static_cast<int>(maxSize - written));
                std::memcpy(data + written, m_buffer.data() + m_bufferOffset, static_cast<size_t>(count));
                m_bufferOffset += count;
                written += count;
                continue;
            }

            const uint64_t requested = std::max<uint64_t>(static_cast<uint64_t>(maxSize - written),
                                                          static_cast<uint64_t>(m_format.bytesPerFrame()));
            const uint64_t aligned
                = requested
                + (m_format.bytesPerFrame() - requested % m_format.bytesPerFrame()) % m_format.bytesPerFrame();
            auto result = m_decoder->readAudio(static_cast<size_t>(aligned));
            if(result.status == AudioDecoder::ReadStatus::DecodedAudio) {
                m_buffer       = std::move(result.buffer);
                m_bufferOffset = 0;
                continue;
            }
            if(result.status == AudioDecoder::ReadStatus::Error) {
                qCWarning(CARRIER_DECODER)
                    << "Primary decoder failed while reading encoded-audio carrier:" << result.error;
                return written > 0 ? written : -1;
            }
            qCDebug(CARRIER_DECODER) << "Primary carrier input stopped:" << static_cast<int>(result.status);
            return written;
        }

        return written;
    }

    qint64 writeData(const char*, qint64) override
    {
        return -1;
    }

private:
    AudioDecoder* m_decoder;
    AudioFormat m_format;
    QByteArray m_prefix;
    qsizetype m_prefixOffset{0};
    AudioBuffer m_buffer;
    int m_bufferOffset{0};
};

class CarrierDecoderPrivate
{
public:
    explicit CarrierDecoderPrivate(std::unique_ptr<AudioDecoder> decoder)
        : primary{std::move(decoder)}
    { }

    AudioDecoder* activeDecoder() const
    {
        return secondary ? static_cast<AudioDecoder*>(secondary.get()) : primary.get();
    }

    bool initialiseSecondary(uint64_t position)
    {
        device    = std::make_unique<DecodedAudioDevice>(primary.get(), primaryFormat, std::exchange(prefix, {}));
        secondary = std::make_unique<FFmpegDecoder>();
        secondary->setPlaybackHints(primary->playbackHints());

        AudioSource source;
        source.filepath   = carrierFilename;
        source.device     = device.get();
        const auto format = secondary->init(source, track, options);
        if(!format) {
            secondary.reset();
            device.reset();
            return false;
        }

        outputFormat   = *format;
        timelineOffset = position;
        return true;
    }

    std::unique_ptr<AudioDecoder> primary;
    std::unique_ptr<DecodedAudioDevice> device;
    std::unique_ptr<FFmpegDecoder> secondary;
    AudioFormat primaryFormat;
    AudioFormat outputFormat;
    Track track;
    AudioDecoder::DecoderOptions options{AudioDecoder::None};
    QByteArray prefix;
    qsizetype prefixOffset{0};
    uint64_t prefixStartTime{0};
    uint64_t timelineOffset{0};
    QString error;
    QString carrierFilename;
    bool primaryStarted{false};
    bool started{false};
};

CarrierDecoder::CarrierDecoder(std::unique_ptr<AudioDecoder> decoder)
    : p{std::make_unique<CarrierDecoderPrivate>(std::move(decoder))}
{ }

CarrierDecoder::~CarrierDecoder() = default;

QStringList CarrierDecoder::extensions() const
{
    return p->primary->extensions();
}

QStringList CarrierDecoder::preferredExtensions() const
{
    return p->primary->preferredExtensions();
}

QStringList CarrierDecoder::supportedSchemes() const
{
    return p->primary->supportedSchemes();
}

bool CarrierDecoder::supportsRemoteSources() const
{
    return p->primary->supportsRemoteSources();
}

bool CarrierDecoder::needsMoreInput() const
{
    return p->activeDecoder()->needsMoreInput();
}

bool CarrierDecoder::isSeekable() const
{
    return !p->options.testFlag(NoSeeking) && p->primary->isSeekable();
}

bool CarrierDecoder::allowsConcurrentDecoding() const
{
    return p->primary->allowsConcurrentDecoding();
}

int CarrierDecoder::playbackPrebufferMs() const
{
    return p->activeDecoder()->playbackPrebufferMs();
}

QStringList CarrierDecoder::takeWarnings()
{
    QStringList warnings = p->primary->takeWarnings();
    if(p->secondary) {
        warnings.append(p->secondary->takeWarnings());
    }
    return warnings;
}

AudioDecoder::RepeatHandling CarrierDecoder::repeatHandling() const
{
    return p->primary->repeatHandling();
}

bool CarrierDecoder::trackHasChanged() const
{
    return p->activeDecoder()->trackHasChanged();
}

Track CarrierDecoder::changedTrack() const
{
    return p->activeDecoder()->changedTrack();
}

std::optional<AudioDecoder::TimedTrackChange> CarrierDecoder::takeTimedTrackChange()
{
    return p->activeDecoder()->takeTimedTrackChange();
}

int CarrierDecoder::bitrate() const
{
    return p->activeDecoder()->bitrate();
}

std::optional<AudioFormat> CarrierDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    stop();
    p->options = options;
    p->track   = track;
    p->primary->setPlaybackHints(playbackHints());

    const auto format = p->primary->init(source, track, options);
    if(!format) {
        return {};
    }

    p->primaryFormat = *format;
    p->outputFormat  = *format;
    if(!canContainCarrier(source, *format)) {
        return format;
    }

    p->primary->start();
    p->primaryStarted = true;

    while(p->prefix.size() < ProbeBytes) {
        const size_t remaining = static_cast<size_t>(ProbeBytes - p->prefix.size());
        auto result            = p->primary->readAudio(remaining);
        if(result.status != ReadStatus::DecodedAudio) {
            break;
        }
        if(p->prefix.isEmpty()) {
            p->prefixStartTime = result.buffer.startTime();
        }
        const auto data = result.buffer.constData();
        p->prefix.append(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
    }

    const AVInputFormat* carrierFormat = probeCarrier(p->prefix);
    if(!carrierFormat) {
        return format;
    }

    qCDebug(CARRIER_DECODER) << "Detected encoded audio carrier:" << carrierFormat->name;
    const QString extension = carrierFormat->extensions
                                ? QString::fromLatin1(carrierFormat->extensions).section(u',', 0, 0)
                                : QString::fromLatin1(carrierFormat->name);
    p->carrierFilename      = u"carrier.%1"_s.arg(extension);
    if(p->initialiseSecondary(0)) {
        return p->outputFormat;
    }

    qCWarning(CARRIER_DECODER) << "Failed to initialise detected audio carrier:" << carrierFormat->name;
    stop();
    return {};
}

void CarrierDecoder::start()
{
    if(!p->primaryStarted) {
        p->primary->start();
        p->primaryStarted = true;
    }
    if(p->secondary) {
        p->secondary->start();
    }
    p->started = true;
}

void CarrierDecoder::stop()
{
    if(p->secondary) {
        p->secondary->stop();
    }
    if(p->primary) {
        p->primary->stop();
    }
    p->device.reset();
    p->secondary.reset();
    p->primaryFormat = {};
    p->outputFormat  = {};
    p->track         = {};
    p->options       = None;
    p->prefix.clear();
    p->prefixOffset    = 0;
    p->prefixStartTime = 0;
    p->timelineOffset  = 0;
    p->error.clear();
    p->carrierFilename.clear();
    p->primaryStarted = false;
    p->started        = false;
}

void CarrierDecoder::seek(uint64_t pos)
{
    if(!isSeekable()) {
        return;
    }

    p->prefix.clear();
    p->prefixOffset = 0;
    p->error.clear();
    p->primary->seek(pos);

    if(p->secondary) {
        p->secondary->stop();
        p->secondary.reset();
        p->device.reset();
        if(!p->initialiseSecondary(pos)) {
            p->error = u"Failed to resynchronise encoded-audio carrier after seeking"_s;
            return;
        }
        if(p->started) {
            p->secondary->start();
        }
    }
}

AudioDecoder::ReadResult CarrierDecoder::readAudio(size_t bytes)
{
    if(!p->error.isEmpty()) {
        return ReadResult::errorResult(p->error);
    }

    if(p->secondary) {
        ReadResult result;
        for(int attempt{0}; attempt < MaximumCarrierDrainAttempts; ++attempt) {
            result = p->secondary->readAudio(bytes);
            if(result.status != ReadStatus::EndOfStream) {
                break;
            }
        }
        if(result.status == ReadStatus::DecodedAudio && p->timelineOffset > 0) {
            result.buffer.setStartTime(p->timelineOffset + result.buffer.startTime());
        }
        return result;
    }

    if(p->prefixOffset < p->prefix.size()) {
        const size_t frameBytes = static_cast<size_t>(p->primaryFormat.bytesPerFrame());
        size_t count            = std::min<size_t>(bytes, static_cast<size_t>(p->prefix.size() - p->prefixOffset));
        count -= count % frameBytes;
        if(count == 0) {
            return ReadResult::needMoreInput();
        }

        const uint64_t startTime
            = p->prefixStartTime + p->primaryFormat.durationForBytes(static_cast<uint64_t>(p->prefixOffset));
        AudioBuffer buffer{reinterpret_cast<const uint8_t*>(p->prefix.constData() + p->prefixOffset), count,
                           p->primaryFormat, startTime};
        p->prefixOffset += static_cast<qsizetype>(count);
        return ReadResult::data(std::move(buffer));
    }

    return p->primary->readAudio(bytes);
}

AudioBuffer CarrierDecoder::readBuffer(size_t bytes)
{
    auto result = readAudio(bytes);
    return result.status == ReadStatus::DecodedAudio ? std::move(result.buffer) : AudioBuffer{};
}

void CarrierDecoder::playbackHintsChanged(PlaybackHints hints)
{
    p->primary->setPlaybackHints(hints);
    if(p->secondary) {
        p->secondary->setPlaybackHints(hints);
    }
}

void CarrierDecoder::interruptRead()
{
    p->primary->requestAbort();
    if(p->secondary) {
        p->secondary->requestAbort();
    }
}
} // namespace Fooyin
