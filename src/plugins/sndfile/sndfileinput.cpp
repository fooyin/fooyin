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

#include "sndfileinput.h"

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(SND_FILE, "fy.sndfile")

namespace {
QStringList fileExtensions()
{
    static const QStringList extensions
        = {QStringLiteral("aif"), QStringLiteral("aiff"), QStringLiteral("au"),  QStringLiteral("snd"),
           QStringLiteral("sph"), QStringLiteral("voc"),  QStringLiteral("wav"), QStringLiteral("wavex"),
           QStringLiteral("w64"), QStringLiteral("wv"),   QStringLiteral("wve")};
    return extensions;
}

sf_count_t sndFileLen(void* data)
{
    auto* file = static_cast<QIODevice*>(data);
    if(file->isOpen()) {
        return file->size();
    }
    return -1;
}

sf_count_t sndSeek(sf_count_t offset, int whence, void* data)
{
    auto* file = static_cast<QIODevice*>(data);
    if(!file->isOpen() || file->isSequential()) {
        return -1;
    }

    qint64 start{0};
    switch(whence) {
        case(SEEK_END):
            start = file->size();
            break;
        case(SEEK_CUR):
            start = file->pos();
            break;
        case(SEEK_SET):
        default:
            start = 0;
    }

    if(file->seek(start + offset)) {
        return start + offset;
    }

    return -1;
}

sf_count_t sndRead(void* ptr, sf_count_t count, void* data)
{
    auto* file = static_cast<QIODevice*>(data);
    if(file->isOpen()) {
        return file->read(static_cast<char*>(ptr), count);
    }
    return -1;
}

sf_count_t sndTell(void* data)
{
    auto* file = static_cast<QIODevice*>(data);
    if(file->isOpen()) {
        return file->pos();
    }
    return -1;
}

QString codecForFormat(int format)
{
    if(format == SF_FORMAT_FLAC) {
        return QStringLiteral("FLAC");
    }
    if(format == SF_FORMAT_VORBIS) {
        return QStringLiteral("Vorbis");
    }
    if(format == SF_FORMAT_OPUS) {
        return QStringLiteral("Opus");
    }
    if(format >= SF_FORMAT_ALAC_16 && format <= SF_FORMAT_ALAC_32) {
        return QStringLiteral("ALAC");
    }
    return QStringLiteral("PCM");
}
} // namespace

namespace Fooyin::Snd {
SndFileDecoder::SndFileDecoder()
    : m_sndFile{nullptr}
    , m_currentFrame{0}
{ }

QStringList SndFileDecoder::extensions() const
{
    return fileExtensions();
}

bool SndFileDecoder::isSeekable() const
{
    return true;
}

std::optional<AudioFormat> SndFileDecoder::init(const AudioSource& source, const Track& track,
                                                DecoderOptions /*options*/)
{
    SF_INFO info;
    info.format = 0;

    m_file = source.device;

    m_vio.get_filelen = sndFileLen;
    m_vio.seek        = sndSeek;
    m_vio.read        = sndRead;
    m_vio.tell        = sndTell;

    m_sndFile = sf_open_virtual(&m_vio, SFM_READ, &info, m_file);
    if(!m_sndFile) {
        qCWarning(SND_FILE) << "Unable to open" << track.filepath();
        return {};
    }

    m_format.setChannelCount(info.channels);
    m_format.setSampleRate(info.samplerate);
    m_format.setSampleFormat(SampleFormat::F64);

    return m_format;
}

void SndFileDecoder::stop()
{
    if(m_sndFile) {
        sf_close(m_sndFile);
        m_sndFile = nullptr;
    }
    m_currentFrame = 0;
}

void SndFileDecoder::seek(uint64_t pos)
{
    const auto frames = sf_seek(m_sndFile, static_cast<sf_count_t>(m_format.framesForDuration(pos)), SEEK_SET);
    if(frames >= 0) {
        m_currentFrame = frames;
    }
}

AudioBuffer SndFileDecoder::readBuffer(size_t bytes)
{
    AudioBuffer buffer{m_format, m_format.durationForFrames(static_cast<int>(m_currentFrame))};
    buffer.resize(bytes);

    const auto frames = static_cast<sf_count_t>(m_format.framesForBytes(static_cast<int>(bytes)));

    const auto readFrames = sf_readf_double(m_sndFile, std::bit_cast<double*>(buffer.data()), frames);
    m_currentFrame += readFrames;

    if(readFrames == 0) {
        return {};
    }
    if(readFrames < frames) {
        const auto bufferSize = m_format.bytesForFrames(static_cast<int>(readFrames));
        buffer.resize(static_cast<size_t>(bufferSize));
    }

    return buffer;
}

QStringList SndFileReader::extensions() const
{
    return fileExtensions();
}

bool SndFileReader::canReadCover() const
{
    return false;
}

bool SndFileReader::canWriteMetaData() const
{
    return false;
}

bool SndFileReader::readTrack(const AudioSource& source, Track& track)
{
    SF_INFO info;
    info.format = 0;

    SF_VIRTUAL_IO vio;

    vio.get_filelen = sndFileLen;
    vio.seek        = sndSeek;
    vio.read        = sndRead;
    vio.tell        = sndTell;

    SNDFILE* sndFile = sf_open_virtual(&vio, SFM_READ, &info, source.device);
    if(!sndFile) {
        qCWarning(SND_FILE) << "Unable to open" << track.filepath();
        return {};
    }

    track.setFileSize(QFileInfo{track.filepath()}.size());
    track.setDuration(static_cast<int>(static_cast<double>(info.frames) / info.samplerate * 1000));
    track.setSampleRate(info.samplerate);
    track.setChannels(info.channels);
    track.setBitrate(static_cast<int>(track.fileSize() * 8 / track.duration()));
    track.setCodec(codecForFormat(info.format & SF_FORMAT_SUBMASK));

    switch(info.format & SF_FORMAT_SUBMASK) {
        case(SF_FORMAT_PCM_U8):
        case(SF_FORMAT_PCM_S8):
            track.setBitDepth(8);
            break;
        case(SF_FORMAT_PCM_16):
            track.setBitDepth(16);
            break;
        case(SF_FORMAT_PCM_24):
            track.setBitDepth(24);
            break;
        case(SF_FORMAT_PCM_32):
        case(SF_FORMAT_FLOAT):
            track.setBitDepth(32);
            break;
        case(SF_FORMAT_DOUBLE):
            track.setBitDepth(64);
            break;
        default:
            break;
    }

    bool lossless{true};
    switch(info.format & SF_FORMAT_TYPEMASK) {
        case(SF_FORMAT_MPEG):
        case(SF_FORMAT_OGG):
        case(SF_FORMAT_VOC):
            lossless = false;
            break;
        default:
            break;
    }

    track.setEncoding(lossless ? QStringLiteral("Lossless") : QStringLiteral("Lossy"));

    if(const char* title = sf_get_string(sndFile, SF_STR_TITLE)) {
        track.setTitle(QString::fromUtf8(title));
    }
    if(const char* date = sf_get_string(sndFile, SF_STR_DATE)) {
        track.setDate(QString::fromUtf8(date));
    }
    if(const char* album = sf_get_string(sndFile, SF_STR_ALBUM)) {
        track.setAlbum(QString::fromUtf8(album));
    }
    if(const char* trackNum = sf_get_string(sndFile, SF_STR_TRACKNUMBER)) {
        track.setTrackNumber(QString::fromUtf8(trackNum));
    }
    if(const char* artist = sf_get_string(sndFile, SF_STR_ARTIST)) {
        track.setArtists({QString::fromUtf8(artist)});
    }
    if(const char* comment = sf_get_string(sndFile, SF_STR_COMMENT)) {
        track.setComment(QString::fromUtf8(comment));
    }
    if(const char* genre = sf_get_string(sndFile, SF_STR_GENRE)) {
        track.setGenres({QString::fromUtf8(genre)});
    }
    if(const char* copyright = sf_get_string(sndFile, SF_STR_COPYRIGHT)) {
        track.addExtraTag(QStringLiteral("COPYRIGHT"), QString::fromUtf8(copyright));
    }
    if(const char* license = sf_get_string(sndFile, SF_STR_LICENSE)) {
        track.addExtraTag(QStringLiteral("LICENSE"), QString::fromUtf8(license));
    }

    return true;
}
} // namespace Fooyin::Snd
