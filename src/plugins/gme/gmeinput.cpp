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

#include "gmeinput.h"

#include "gmedefs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

#include <gme/gme.h>

#include <algorithm>
#include <limits>

Q_LOGGING_CATEGORY(GME, "fy.gme")

using namespace Qt::StringLiterals;

constexpr auto SampleRate = 44100;
constexpr auto Bps        = 16;

namespace {
struct GmeInfoDeleter
{
    void operator()(gme_info_t* info) const
    {
        if(info) {
            gme_free_info(info);
        }
    }
};
using GmeInfoPtr = std::unique_ptr<gme_info_t, GmeInfoDeleter>;

QString findM3u(const QString& filepath)
{
    const QFileInfo info{filepath};
    const QDir dir{info.absolutePath()};

    const auto files = dir.entryInfoList({info.completeBaseName() + u".m3u"_s}, QDir::Files);
    if(files.isEmpty()) {
        return {};
    }

    return files.front().absoluteFilePath();
}

uint64_t getDuration(const gme_info_t* info, bool repeatTrack, Fooyin::AudioDecoder::DecoderOptions options = {})
{
    if(info->length > 0) {
        return static_cast<uint64_t>(info->length);
    }

    using Fooyin::AudioDecoder;
    using namespace Fooyin::Gme;

    const Fooyin::FySettings settings;
    const double maxLengthMinutes = settings.value(MaxLength, DefaultMaxLength).toDouble();
    const uint64_t maxLengthMs    = static_cast<uint64_t>(std::max(0.0, maxLengthMinutes) * 60.0 * 1000.0);

    int loopCount = settings.value(LoopCount, DefaultLoopCount).toInt();

    if(options & AudioDecoder::NoLooping) {
        loopCount = 1;
    }

    if(repeatTrack) {
        return maxLengthMs;
    }

    if(info->loop_length > 0) {
        loopCount = std::max(loopCount, 1);

        const uint64_t introLength = static_cast<uint64_t>(std::max(info->intro_length, 0));
        const uint64_t loopLength  = static_cast<uint64_t>(info->loop_length);
        return introLength + (loopLength * static_cast<uint64_t>(loopCount));
    }

    if(info->length > 0) {
        return static_cast<uint64_t>(info->length);
    }

    return maxLengthMs;
}

QStringList supportedExtensions()
{
    static const QStringList extensions
        = {u"ay"_s, u"gbs"_s, u"hes"_s, u"kss"_s, u"nsf"_s, u"nsfe"_s, u"sap"_s, u"spc"_s};
    return extensions;
}

void loadM3uIfAvailable(Music_Emu* emu, const Fooyin::AudioSource& source, const Fooyin::Track& track)
{
    if(!emu) {
        return;
    }

    if(track.isInArchive()) {
        if(!source.archiveReader) {
            return;
        }

        const QFileInfo fileInfo{track.pathInArchive()};
        const QString m3uPath = fileInfo.dir().relativeFilePath(fileInfo.completeBaseName() + u".m3u"_s);
        auto m3uEntry         = source.archiveReader->entry(m3uPath);
        if(!m3uEntry) {
            return;
        }

        const auto m3uData = m3uEntry->readAll();
        if(m3uData.isEmpty()) {
            return;
        }

        const auto* err = gme_load_m3u_data(emu, m3uData.constData(), m3uData.size());
        if(err) {
            qCInfo(GME) << err;
        }
        return;
    }

    const QString m3u = findM3u(track.filepath());
    if(m3u.isEmpty()) {
        return;
    }

    const auto* err = gme_load_m3u(emu, m3u.toUtf8().constData());
    if(err) {
        qCInfo(GME) << err;
    }
}
} // namespace

namespace Fooyin::Gme {
void MusicEmuDeleter::operator()(Music_Emu* emu) const
{
    if(emu) {
        gme_delete(emu);
    }
}

GmeDecoder::GmeDecoder()
    : m_repeatTrack{false}
    , m_subsong{0}
    , m_duration{0}
    , m_loopLength{0}
{
    m_format.setSampleFormat(SampleFormat::S16);
    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(2);
}

QStringList GmeDecoder::extensions() const
{
    return supportedExtensions();
}

bool GmeDecoder::isSeekable() const
{
    return true;
}

bool GmeDecoder::trackHasChanged() const
{
    return m_changedTrack.isValid();
}

Track GmeDecoder::changedTrack() const
{
    return m_changedTrack;
}

std::optional<AudioFormat> GmeDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    m_repeatTrack = !(options & NoInfiniteLooping) && isRepeatingTrack();

    const QByteArray data = source.device->readAll();
    if(data.isEmpty()) {
        return {};
    }

    Music_Emu* emu{nullptr};
    gme_open_data(data.constData(), data.size(), &emu, SampleRate);
    if(!emu) {
        return {};
    }
    m_emu.reset(emu);
    loadM3uIfAvailable(m_emu.get(), source, track);

    m_subsong = track.subsong();

    gme_info_t* gmeInfo{nullptr};
    if(!gme_track_info(m_emu.get(), &gmeInfo, m_subsong) && gmeInfo) {
        const GmeInfoPtr info{gmeInfo};

        const auto duration = getDuration(gmeInfo, m_repeatTrack, options);

        if(options & UpdateTracks) {
            if(track.duration() != duration) {
                m_changedTrack = track;
                m_changedTrack.setDuration(duration);
            }
        }

        m_loopLength = info->loop_length;
        m_duration   = static_cast<int>(std::min<uint64_t>(duration, std::numeric_limits<int>::max()));
    }

    gme_enable_accuracy(m_emu.get(), 1);

    return m_format;
}

void GmeDecoder::start()
{
    gme_start_track(m_emu.get(), m_subsong);

    if(m_loopLength != 0 && m_repeatTrack) {
        gme_set_fade(m_emu.get(), -1);
    }
    else {
#if defined(GME_VERSION) && GME_VERSION >= 0x000604
        const int fadeLength = std::clamp(m_settings.value(FadeLength, DefaultFadeLength).toInt(), 0, m_duration);
        const int fadeStart  = std::max(m_duration - fadeLength, 0);
        gme_set_fade_msecs(m_emu.get(), fadeStart, fadeLength);
#else
        gme_set_fade(m_emu.get(), m_duration - 8000);
#endif
    }
}

void GmeDecoder::stop()
{
    m_emu.reset();
}

void GmeDecoder::seek(uint64_t pos)
{
    const auto* err = gme_seek(m_emu.get(), static_cast<int>(pos));
    if(err) {
        qCDebug(GME) << err;
    }
}

AudioBuffer GmeDecoder::readBuffer(size_t bytes)
{
    if(gme_track_ended(m_emu.get())) {
        return {};
    }

    const int channels = m_format.channelCount();
    if(channels <= 0) {
        return {};
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return {};
    }

    const int requestedFrames = std::max<int>(1, static_cast<int>(bytes / static_cast<size_t>(bytesPerFrame)));
    const auto startTime      = static_cast<uint64_t>(gme_tell(m_emu.get()));

    AudioBuffer buffer{m_format, startTime};
    buffer.resize(m_format.bytesForFrames(requestedFrames));

    const int requestedSamples = requestedFrames * channels;
    const auto* err            = gme_play(m_emu.get(), requestedSamples, reinterpret_cast<int16_t*>(buffer.data()));
    if(err) {
        qCDebug(GME) << err;
        return {};
    }

    return buffer;
}

GmeReader::GmeReader()
    : m_subsongCount{1}
{ }

QStringList GmeReader::extensions() const
{
    return supportedExtensions();
}

bool GmeReader::canReadCover() const
{
    return false;
}

bool GmeReader::canWriteMetaData() const
{
    return false;
}

int GmeReader::subsongCount() const
{
    return m_subsongCount;
}

bool GmeReader::init(const AudioSource& source)
{
    m_trackData = source.device->peek(source.device->size());
    if(m_trackData.isEmpty()) {
        return {};
    }

    Music_Emu* emu{nullptr};
    gme_open_data(m_trackData.constData(), m_trackData.size(), &emu, gme_info_only);
    if(!emu) {
        return false;
    }
    m_emu.reset(emu);

    m_subsongCount = gme_track_count(m_emu.get());

    return true;
}

bool GmeReader::readTrack(const AudioSource& source, Track& track)
{
    loadM3uIfAvailable(m_emu.get(), source, track);

    gme_info_t* gmeInfo{nullptr};
    const auto* err = gme_track_info(m_emu.get(), &gmeInfo, track.subsong());
    if(err) {
        qCWarning(GME) << err;
        return false;
    }
    GmeInfoPtr info{gmeInfo};

    track.setDuration(getDuration(info.get(), false));
    track.setSampleRate(SampleRate);
    track.setBitDepth(Bps);
    track.setEncoding(u"Synthesized"_s);

    if(*info->song) {
        track.setTitle(QString::fromUtf8(info->song));
    }
    if(*info->author) {
        track.setArtists({QString::fromUtf8(info->author)});
    }
    if(*info->system) {
        track.setAlbumArtists({QString::fromUtf8(info->system)});
    }
    if(*info->game) {
        track.setAlbum({QString::fromUtf8(info->game)});
    }
    if(*info->system) {
        track.addExtraTag(u"SYSTEM"_s, {QString::fromUtf8(info->system)});
    }
    if(*info->copyright) {
        track.addExtraTag(u"COPYRIGHT"_s, {QString::fromUtf8(info->copyright)});
    }
    if(*info->comment) {
        track.addExtraTag(u"COMMENT"_s, {QString::fromUtf8(info->comment)});
    }

    return true;
}
} // namespace Fooyin::Gme
