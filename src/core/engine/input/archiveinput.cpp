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

namespace Fooyin {
ArchiveDecoder::ArchiveDecoder(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
    , m_decoder{nullptr}
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
    return m_decoder != nullptr && m_decoder->isSeekable();
}

bool ArchiveDecoder::trackHasChanged() const
{
    return m_decoder != nullptr && m_decoder->trackHasChanged();
}

Track ArchiveDecoder::changedTrack() const
{
    return m_decoder ? m_decoder->changedTrack() : Track{};
}

int ArchiveDecoder::bitrate() const
{
    return m_decoder ? m_decoder->bitrate() : 0;
}

std::optional<AudioFormat> ArchiveDecoder::init(const AudioSource& /*source*/, const Track& track,
                                                DecoderOptions options)
{
    const QString archivePath = track.archivePath();
    m_archiveReader           = m_audioLoader->archiveReaderForFile(archivePath);
    if(!m_archiveReader) {
        return {};
    }

    if(!m_archiveReader->init(archivePath)) {
        return {};
    }

    const QString filepath = track.pathInArchive();
    m_device               = m_archiveReader->entry(filepath);
    if(!m_device) {
        return {};
    }

    m_decoder = m_audioLoader->decoderForFile(filepath);
    if(!m_decoder) {
        return {};
    }

    AudioSource aSource;
    aSource.filepath      = filepath;
    aSource.device        = m_device.get();
    aSource.archiveReader = m_archiveReader.get();

    return m_decoder->init(aSource, track, options);
}

void ArchiveDecoder::start()
{
    if(m_decoder) {
        m_decoder->start();
    }
}

void ArchiveDecoder::stop()
{
    if(m_decoder) {
        m_decoder->stop();
    }
}

void ArchiveDecoder::seek(uint64_t pos)
{
    if(m_decoder) {
        m_decoder->seek(pos);
    }
}

AudioBuffer ArchiveDecoder::readBuffer(size_t bytes)
{
    return m_decoder ? m_decoder->readBuffer(bytes) : AudioBuffer{};
}

Fooyin::GeneralArchiveReader::GeneralArchiveReader(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
    , m_reader{nullptr}
{ }

GeneralArchiveReader::~GeneralArchiveReader()
{
    m_audioLoader.reset();
}

QStringList GeneralArchiveReader::extensions() const
{
    return m_reader ? m_reader->extensions() : QStringList{};
}

bool GeneralArchiveReader::canReadCover() const
{
    if(m_archiveReader) {
        return true;
    }
    if(m_reader) {
        return m_reader->canReadCover();
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
    return m_reader ? m_reader->subsongCount() : 1;
}

bool GeneralArchiveReader::init(const AudioSource& source)
{
    const Track track{source.filepath};

    const QString archivePath = track.archivePath();
    m_archiveReader           = m_audioLoader->archiveReaderForFile(archivePath);
    if(!m_archiveReader) {
        return false;
    }

    if(!m_archiveReader->init(archivePath)) {
        return false;
    }

    const QString filepath = track.pathInArchive();
    m_device               = m_archiveReader->entry(filepath);
    if(!m_device) {
        return false;
    }

    m_reader = m_audioLoader->readerForFile(filepath);
    if(!m_reader) {
        return false;
    }

    AudioSource aSource;
    aSource.filepath      = filepath;
    aSource.device        = m_device.get();
    aSource.archiveReader = m_archiveReader.get();

    return m_reader->init(aSource);
}

bool GeneralArchiveReader::readTrack(const AudioSource& source, Track& track)
{
    AudioSource trackSource{source};
    if(!trackSource.device) {
        trackSource.device = m_device.get();
    }
    if(track.fileSize() == 0) {
        track.setFileSize(m_device->size());
    }
    if(track.modifiedTime() == 0) {
        const QDateTime modifiedTime = QFileInfo{track.archivePath()}.lastModified();
        track.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
    }
    trackSource.archiveReader = m_archiveReader.get();
    return m_reader && m_reader->readTrack(trackSource, track);
}

QByteArray GeneralArchiveReader::readCover(const AudioSource& /*source*/, const Track& track, Track::Cover cover)
{
    QByteArray coverData;
    if(m_reader) {
        AudioSource coverSource;
        coverSource.filepath      = track.filepath();
        coverSource.device        = m_device.get();
        coverSource.archiveReader = m_archiveReader.get();
        coverData                 = m_reader->readCover(coverSource, track, cover);
    }
    if(coverData.isEmpty() && m_archiveReader && m_archiveReader->init(track.archivePath())) {
        coverData = m_archiveReader->readCover(track, cover);
    }
    return coverData;
}

bool GeneralArchiveReader::writeTrack(const AudioSource& /*source*/, const Track& track, WriteOptions options)
{
    if(!m_reader) {
        return false;
    }

    AudioSource trackSource;
    trackSource.filepath      = track.filepath();
    trackSource.device        = m_device.get();
    trackSource.archiveReader = m_archiveReader.get();
    return m_reader->writeTrack(trackSource, track, options);
}
} // namespace Fooyin
