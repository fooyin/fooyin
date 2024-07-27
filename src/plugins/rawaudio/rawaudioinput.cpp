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

namespace Fooyin::RawAudio {
RawAudioInput::RawAudioInput()
    : m_currentFrame{0}
{
    // Assume 16bit PCM
    m_format.setSampleFormat(SampleFormat::S16);
    m_format.setChannelCount(2);
    m_format.setSampleRate(44100);
}

QStringList RawAudioInput::supportedExtensions() const
{
    static const QStringList extensions{QStringLiteral("bin")};
    return extensions;
}

bool RawAudioInput::canReadCover() const
{
    return false;
}

bool RawAudioInput::canWriteMetaData() const
{
    return false;
}

bool RawAudioInput::isSeekable() const
{
    return m_file && !m_file->isSequential();
}

bool RawAudioInput::init(const QString& source, DecoderOptions /*options*/)
{
    m_file = std::make_unique<QFile>(source);
    if(!m_file->open(QIODevice::ReadOnly)) {
        qCWarning(RAW_AUD) << "Unable to open" << source;
        return false;
    }

    if(!isValidData(m_file.get())) {
        qCWarning(RAW_AUD) << "Invalid file" << source;
        return false;
    }

    m_file->seek(0);

    return true;
}

void RawAudioInput::start() { }

void RawAudioInput::stop()
{
    if(m_file) {
        if(m_file->isOpen()) {
            m_file->close();
        }
    }
    m_currentFrame = 0;
}

void RawAudioInput::seek(uint64_t pos)
{
    if(m_file && m_file->isOpen() && m_file->seek(static_cast<qint64>(m_format.bytesForDuration(pos)))) {
        m_currentFrame = m_format.framesForDuration(pos);
    }
}

AudioBuffer RawAudioInput::readBuffer(size_t bytes)
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

bool RawAudioInput::readMetaData(Track& track)
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
    track.setDuration(track.fileSize() / m_format.bytesPerFrame() / m_format.sampleRate() * 1000);
    track.setSampleRate(m_format.sampleRate());
    track.setChannels(m_format.channelCount());
    track.setBitrate(static_cast<int>(track.fileSize() * 8 / track.duration()));
    track.setBitDepth(m_format.bytesPerSample() * 8);
    track.setCodec(QStringLiteral("PCM"));

    return true;
}

AudioFormat RawAudioInput::format() const
{
    return m_format;
}

AudioInput::Error RawAudioInput::error() const
{
    return {};
}

// TODO: Add further checks
bool RawAudioInput::isValidData(QFile* file) const
{
    const int bps = m_format.bytesPerFrame();

    if(file->size() % bps != 0) {
        return false;
    }

    static constexpr int preload = 1024;
    const QByteArray buffer      = file->read(preload);
    if(buffer.size() != preload) {
        return false;
    }

    return true;
}
} // namespace Fooyin::RawAudio
