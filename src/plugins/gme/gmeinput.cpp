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

Q_LOGGING_CATEGORY(GME, "GME")

constexpr auto SampleRate = 44100;
constexpr auto Bps        = 16;
constexpr auto BufferLen  = 1024;

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

    const auto files = dir.entryInfoList({info.completeBaseName() + QStringLiteral(".m3u")}, QDir::Files);
    if(files.isEmpty()) {
        return {};
    }

    return files.front().absoluteFilePath();
}

uint64_t getDuration(const gme_info_t* info, Fooyin::AudioDecoder::DecoderOptions options = {})
{
    if(info->length > 0) {
        return info->length;
    }

    using Fooyin::AudioDecoder;
    using namespace Fooyin::Gme;

    const Fooyin::FySettings settings;
    const int maxLength = settings.value(QLatin1String{MaxLength}, DefaultMaxLength).toInt();
    int loopCount       = settings.value(QLatin1String{LoopCount}, DefaultLoopCount).toInt();

    if(options & AudioDecoder::NoLooping) {
        loopCount = 1;
    }

    if(info->loop_length <= 0 || loopCount <= 0) {
        return static_cast<uint64_t>(maxLength * 60.0 * 1000);
    }

    uint64_t songLength{0};
    if(info->intro_length > 0) {
        songLength = static_cast<int>(info->intro_length);
    }
    songLength += static_cast<int>(info->loop_length * loopCount);

    return songLength;
}

QStringList supportedExtensions()
{
    static const QStringList extensions
        = {QStringLiteral("ay"),  QStringLiteral("gbs"),  QStringLiteral("hes"), QStringLiteral("kss"),
           QStringLiteral("nsf"), QStringLiteral("nsfe"), QStringLiteral("sap"), QStringLiteral("spc")};
    return extensions;
}
} // namespace

namespace Fooyin::Gme {
GmeDecoder::GmeDecoder()
    : m_subsong{0}
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
    m_options = options;

    const QByteArray data = source.device->readAll();
    if(data.isEmpty()) {
        return {};
    }

    Music_Emu* emu{nullptr};
    gme_open_data(data.constData(), data.size(), &emu, SampleRate);
    if(!emu) {
        return {};
    }
    m_emu = {emu, MusicEmuDeleter()};

    m_subsong = track.subsong();

    gme_info_t* gmeInfo{nullptr};
    if(!gme_track_info(m_emu.get(), &gmeInfo, m_subsong) && gmeInfo) {
        const GmeInfoPtr info{gmeInfo};

        const auto duration = getDuration(gmeInfo, options);

        if(options & UpdateTracks) {
            if(track.duration() != duration) {
                m_changedTrack = track;
                m_changedTrack.setDuration(duration);
            }
        }

        m_loopLength = info->loop_length;
        m_duration   = static_cast<int>(duration);
    }

    gme_enable_accuracy(m_emu.get(), 1);

    return m_format;
}

void GmeDecoder::start()
{
    gme_start_track(m_emu.get(), m_subsong);
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

AudioBuffer GmeDecoder::readBuffer(size_t /*bytes*/)
{
    if(gme_track_ended(m_emu.get())) {
        return {};
    }

    const int loopCount = m_settings.value(QLatin1String{LoopCount}, DefaultLoopCount).toInt();
    if(m_loopLength > 0 && !(m_options & NoInfiniteLooping) && loopCount == 0) {
        gme_set_fade(m_emu.get(), -1);
    }
    else {
#if defined(GME_VERSION) && GME_VERSION >= 0x000604
        const int fadeLength = m_settings.value(QLatin1String{FadeLength}, DefaultFadeLength).toInt();
        gme_set_fade_msecs(m_emu.get(), m_duration - fadeLength, fadeLength);
#else
        gme_set_fade(m_emu.get(), m_duration - 8000);
#endif
    }

    const auto startTime = static_cast<uint64_t>(gme_tell(m_emu.get()));

    AudioBuffer buffer{m_format, startTime};
    buffer.resize(m_format.bytesForFrames(BufferLen));

    const int frames = BufferLen * 2;
    const auto* err  = gme_play(m_emu.get(), frames, std::bit_cast<int16_t*>(buffer.data()));
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
    const QByteArray data = source.device->readAll();
    if(data.isEmpty()) {
        return {};
    }

    Music_Emu* emu{nullptr};
    gme_open_data(data.constData(), data.size(), &emu, gme_info_only);
    if(!emu) {
        return false;
    }
    m_emu.reset(emu);

    m_subsongCount = gme_track_count(m_emu.get());

    return true;
}

bool GmeReader::readTrack(const AudioSource& source, Track& track)
{
    const QByteArray data = source.device->readAll();
    if(data.isEmpty()) {
        return {};
    }

    Music_Emu* gme{nullptr};
    gme_open_data(data.constData(), data.size(), &gme, gme_info_only);
    if(!gme) {
        return false;
    }

    const MusicEmuPtr emu{gme};
    gme_info_t* gmeInfo{nullptr};

    const auto* err = gme_track_info(emu.get(), &gmeInfo, track.subsong());
    if(err) {
        qCWarning(GME) << err;
        return false;
    }

    GmeInfoPtr info{gmeInfo};

    if(source.findArchiveFile) {
        auto m3uEntry = source.findArchiveFile(track.archiveDirectory() + track.filename() + QStringLiteral(".m3u"));
        if(m3uEntry) {
            const auto m3uData = m3uEntry->readAll();
            err                = gme_load_m3u_data(emu.get(), m3uData.constData(), m3uData.size());
            if(err) {
                qCWarning(GME) << err;
            }
        }
    }
    else {
        const QString m3u = findM3u(track.filepath());
        if(!m3u.isEmpty()) {
            err = gme_load_m3u(emu.get(), m3u.toUtf8().constData());
            if(err) {
                qCWarning(GME) << err;
            }
        }
    }

    track.setDuration(getDuration(info.get()));
    track.setSampleRate(SampleRate);
    track.setBitDepth(Bps);

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
        track.addExtraTag(QStringLiteral("SYSTEM"), {QString::fromUtf8(info->system)});
    }
    if(*info->copyright) {
        track.addExtraTag(QStringLiteral("COPYRIGHT"), {QString::fromUtf8(info->copyright)});
    }
    if(*info->comment) {
        track.addExtraTag(QStringLiteral("COMMENT"), {QString::fromUtf8(info->comment)});
    }

    return true;
}
} // namespace Fooyin::Gme
