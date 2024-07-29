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

#include "rawaudioinput.h"

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(RAW_AUD, "RawAudio")

const constexpr auto Format = Fooyin::SampleFormat::S16;
constexpr auto SampleRate   = 44100;
constexpr auto Channels     = 2;

namespace {
QStringList fileExtensions()
{
    static const QStringList extensions = {QStringLiteral("bin")};
    return extensions;
}

bool isValidData(QFile* file)
{
    if(file->size() % 4 != 0) {
        return false;
    }

    static constexpr int preload = 1024;
    const QByteArray buffer      = file->read(preload);
    if(buffer.size() != preload) {
        return false;
    }

    return true;
}
} // namespace

namespace Fooyin::RawAudio {
RawAudioDecoder::RawAudioDecoder()
    : m_currentFrame{0}
{
    // Assume 16bit PCM
    m_format.setSampleFormat(Format);
    m_format.setSampleRate(SampleRate);
    m_format.setChannelCount(Channels);
}

QStringList RawAudioDecoder::extensions() const
{
    return fileExtensions();
}

bool RawAudioDecoder::isSeekable() const
{
    return m_file && !m_file->isSequential();
}

std::optional<AudioFormat> RawAudioDecoder::init(const Track& track, DecoderOptions /*options*/)
{
    m_file = std::make_unique<QFile>(track.filepath());
    if(!m_file->open(QIODevice::ReadOnly)) {
        qCWarning(RAW_AUD) << "Unable to open" << track.filepath();
        return {};
    }

    if(!isValidData(m_file.get())) {
        qCWarning(RAW_AUD) << "Invalid file" << track.filepath();
        return {};
    }

    m_file->seek(0);

    return m_format;
}

void RawAudioDecoder::stop()
{
    if(m_file) {
        if(m_file->isOpen()) {
            m_file->close();
        }
    }
    m_currentFrame = 0;
}

void RawAudioDecoder::seek(uint64_t pos)
{
    if(m_file && m_file->isOpen() && m_file->seek(static_cast<qint64>(m_format.bytesForDuration(pos)))) {
        m_currentFrame = m_format.framesForDuration(pos);
    }
}

AudioBuffer RawAudioDecoder::readBuffer(size_t bytes)
{
    AudioBuffer buffer{m_format, m_format.durationForFrames(static_cast<int>(m_currentFrame))};
    buffer.resize(bytes);

    const auto readBytes = m_file->read(std::bit_cast<char*>(buffer.data()), static_cast<qint64>(bytes));
    if(readBytes <= 0) {
        return {};
    }

    m_currentFrame += m_format.framesForBytes(static_cast<int>(readBytes));
    if(std::cmp_less(readBytes, bytes)) {
        buffer.resize(static_cast<size_t>(readBytes));
    }

    return buffer;
}

QStringList RawAudioReader::extensions() const
{
    return fileExtensions();
}

bool RawAudioReader::canReadCover() const
{
    return false;
}

bool RawAudioReader::canWriteMetaData() const
{
    return false;
}

bool RawAudioReader::readTrack(Track& track)
{
    QFile file{track.filepath()};
    if(!file.open(QIODevice::ReadOnly)) {
        qCWarning(RAW_AUD) << "Unable to open file" << track.filepath();
        return false;
    }

    if(!isValidData(&file)) {
        qCWarning(RAW_AUD) << "Invalid file" << track.filepath();
        return false;
    }

    track.setFileSize(file.size());
    track.setDuration(track.fileSize() / 4 / SampleRate * 1000);
    track.setSampleRate(SampleRate);
    track.setChannels(Channels);
    track.setBitrate(static_cast<int>(track.fileSize() * 8 / track.duration()));
    track.setBitDepth(16);
    track.setCodec(QStringLiteral("PCM"));

    return true;
}
} // namespace Fooyin::RawAudio
