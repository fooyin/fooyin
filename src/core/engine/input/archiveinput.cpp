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

#include "archiveinput.h"

#include <QFileInfo>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ARCH_DECODER, "fy.archivedecoder")

namespace Fooyin {
ArchiveDecoder::ArchiveDecoder(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
{ }

ArchiveDecoder::~ArchiveDecoder()
{
    m_audioLoader = nullptr;
}

QStringList ArchiveDecoder::extensions() const
{
    return {};
}

bool ArchiveDecoder::isSeekable() const
{
    return m_loadedDecoder.decoder != nullptr && m_loadedDecoder.decoder->isSeekable();
}

bool ArchiveDecoder::trackHasChanged() const
{
    return m_loadedDecoder.decoder != nullptr && m_loadedDecoder.decoder->trackHasChanged();
}

Track ArchiveDecoder::changedTrack() const
{
    return m_loadedDecoder.decoder ? m_loadedDecoder.decoder->changedTrack() : Track{};
}

int ArchiveDecoder::bitrate() const
{
    return m_loadedDecoder.decoder ? m_loadedDecoder.decoder->bitrate() : 0;
}

std::optional<AudioFormat> ArchiveDecoder::init(const AudioSource& /*source*/, const Track& track,
                                                DecoderOptions options)
{
    m_loadedDecoder = m_audioLoader->loadDecoderForArchiveTrack(track, options, playbackHints());
    if(!m_loadedDecoder.decoder) {
        qCWarning(ARCH_DECODER) << "No decoder available for archive track:" << track.filepath();
    }
    return m_loadedDecoder.format;
}

void ArchiveDecoder::start()
{
    if(m_loadedDecoder.decoder) {
        m_loadedDecoder.decoder->start();
    }
}

void ArchiveDecoder::stop()
{
    if(m_loadedDecoder.decoder) {
        m_loadedDecoder.decoder->stop();
    }
}

void ArchiveDecoder::seek(uint64_t pos)
{
    if(m_loadedDecoder.decoder) {
        m_loadedDecoder.decoder->seek(pos);
    }
}

AudioBuffer ArchiveDecoder::readBuffer(size_t bytes)
{
    if(!m_loadedDecoder.decoder) {
        return {};
    }

    return m_loadedDecoder.decoder->readBuffer(bytes);
}

Fooyin::GeneralArchiveReader::GeneralArchiveReader(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
{ }

GeneralArchiveReader::~GeneralArchiveReader()
{
    m_audioLoader.reset();
}

QStringList GeneralArchiveReader::extensions() const
{
    return m_loadedReader.reader ? m_loadedReader.reader->extensions() : QStringList{};
}

bool GeneralArchiveReader::canReadCover() const
{
    if(m_loadedReader.input.archiveReader) {
        return true;
    }
    if(m_loadedReader.reader) {
        return m_loadedReader.reader->canReadCover();
    }

    return false;
}

bool GeneralArchiveReader::canWriteMetaData() const
{
    // TODO: Support writing
    return false;
}

int GeneralArchiveReader::subsongCount() const
{
    return m_loadedReader.reader ? m_loadedReader.reader->subsongCount() : 1;
}

bool GeneralArchiveReader::init(const AudioSource& source)
{
    const Track track{source.filepath};

    m_loadedReader = m_audioLoader->loadReaderForArchiveTrack(track);
    if(!m_loadedReader.reader) {
        qCDebug(ARCH_DECODER) << "No reader available for archive track:" << track.filepath();
    }

    return m_loadedReader.reader != nullptr;
}

bool GeneralArchiveReader::readTrack(const AudioSource& source, Track& track)
{
    AudioSource trackSource{source};
    if(!trackSource.device) {
        trackSource.device = m_loadedReader.input.device.get();
    }
    if(trackSource.size == 0) {
        trackSource.size = m_loadedReader.input.source.size;
    }
    if(trackSource.modifiedTime == 0) {
        trackSource.modifiedTime = m_loadedReader.input.source.modifiedTime;
    }
    if(track.fileSize() == 0) {
        if(trackSource.size > 0) {
            track.setFileSize(trackSource.size);
        }
        else if(trackSource.device) {
            track.setFileSize(trackSource.device->size());
        }
    }
    if(track.modifiedTime() == 0) {
        if(trackSource.modifiedTime > 0) {
            track.setModifiedTime(trackSource.modifiedTime);
        }
        else {
            const QDateTime modifiedTime = QFileInfo{track.archivePath()}.lastModified();
            track.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
        }
    }

    trackSource.archiveReader = m_loadedReader.input.archiveReader.get();
    return m_loadedReader.reader && m_loadedReader.reader->readTrack(trackSource, track);
}

QByteArray GeneralArchiveReader::readCover(const AudioSource& /*source*/, const Track& track, Track::Cover cover)
{
    QByteArray coverData;
    if(m_loadedReader.reader) {
        AudioSource coverSource;
        coverSource.filepath      = track.filepath();
        coverSource.device        = m_loadedReader.input.device.get();
        coverSource.archiveReader = m_loadedReader.input.archiveReader.get();
        coverData                 = m_loadedReader.reader->readCover(coverSource, track, cover);
    }
    if(coverData.isEmpty() && m_loadedReader.input.archiveReader
       && m_loadedReader.input.archiveReader->init(track.archivePath())) {
        coverData = m_loadedReader.input.archiveReader->readCover(track, cover);
    }
    return coverData;
}

bool GeneralArchiveReader::writeTrack(const AudioSource& /*source*/, const Track& track, WriteOptions options)
{
    if(!m_loadedReader.reader) {
        return false;
    }

    AudioSource trackSource;
    trackSource.filepath      = track.filepath();
    trackSource.device        = m_loadedReader.input.device.get();
    trackSource.archiveReader = m_loadedReader.input.archiveReader.get();
    return m_loadedReader.reader->writeTrack(trackSource, track, options);
}
} // namespace Fooyin
