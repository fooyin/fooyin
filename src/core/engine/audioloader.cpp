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

#include <core/engine/audioloader.h>

#include <core/track.h>

#include <QFileInfo>
#include <QLoggingCategory>
#include <QThreadStorage>

#include <mutex>
#include <shared_mutex>

Q_LOGGING_CATEGORY(AUD_LDR, "AudioLoader")

namespace Fooyin {
using DecoderInstances       = std::unordered_map<QString, std::unique_ptr<AudioDecoder>>;
using ReaderInstances        = std::unordered_map<QString, std::unique_ptr<AudioReader>>;
using ArchiveReaderInstances = std::unordered_map<QString, std::unique_ptr<ArchiveReader>>;

class AudioLoaderPrivate
{
public:
    std::unordered_map<QString, DecoderCreator> m_decoders;
    std::unordered_map<QString, ReaderCreator> m_readers;
    std::unordered_map<QString, ArchiveReaderCreator> m_archiveReaders;

    std::unordered_map<QString, std::vector<QString>> m_extensionToDecoderMap;
    std::unordered_map<QString, std::vector<QString>> m_extensionToReaderMap;
    std::unordered_map<QString, std::vector<QString>> m_extensionToArchiveMap;

    QThreadStorage<DecoderInstances*> m_decoderInstances;
    QThreadStorage<ReaderInstances*> m_readerInstances;
    QThreadStorage<ArchiveReaderInstances*> m_archiveReaderInstances;

    std::shared_mutex m_decoderMutex;
    std::shared_mutex m_readerMutex;
    std::shared_mutex m_archiveReaderMutex;
};

AudioLoader::AudioLoader()
    : p{std::make_unique<AudioLoaderPrivate>()}
{ }

AudioLoader::~AudioLoader() = default;

QStringList AudioLoader::supportedFileExtensions() const
{
    const std::shared_lock lock{p->m_decoderMutex};

    QStringList extensions;

    for(const auto& [ext, _] : p->m_extensionToDecoderMap) {
        extensions.emplace_back(ext);
    }
    for(const auto& [ext, _] : p->m_extensionToArchiveMap) {
        extensions.emplace_back(ext);
    }

    return extensions;
}

bool AudioLoader::canWriteMetadata(const Track& track) const
{
    if(auto* reader = readerForTrack(track)) {
        if(track.isInArchive()) {
            reader->init({track.filepath(), nullptr, nullptr});
        }
        return reader->canWriteMetaData();
    }
    return false;
}

bool AudioLoader::isArchive(const QString& file) const
{
    const QFileInfo info{file};

    const std::shared_lock lock{p->m_archiveReaderMutex};
    return p->m_extensionToArchiveMap.contains(info.suffix().toLower());
}

AudioDecoder* AudioLoader::decoderForFile(const QString& file) const
{
    const QFileInfo info{file};

    const std::shared_lock lock{p->m_decoderMutex};

    const auto extensions = info.suffix().toLower();
    if(!p->m_extensionToDecoderMap.contains(extensions)) {
        return nullptr;
    }

    const QString decoderName = p->m_extensionToDecoderMap.at(extensions).front();
    if(!p->m_decoders.contains(decoderName)) {
        return nullptr;
    }

    if(p->m_decoderInstances.hasLocalData()) {
        auto& decoders = p->m_decoderInstances.localData();
        if(decoders->contains(decoderName)) {
            return decoders->at(decoderName).get();
        }
        return decoders->emplace(decoderName, p->m_decoders.at(decoderName)()).first->second.get();
    }

    p->m_decoderInstances.setLocalData(new DecoderInstances());

    auto decoder = p->m_decoders.at(decoderName)();
    return p->m_decoderInstances.localData()->emplace(decoderName, std::move(decoder)).first->second.get();
}

AudioDecoder* AudioLoader::decoderForTrack(const Track& track) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    const auto extensions = track.extension();
    if(!p->m_extensionToDecoderMap.contains(extensions)) {
        return nullptr;
    }

    QString decoderName;

    if(track.isInArchive()) {
        decoderName = QStringLiteral("Archive");
    }
    else {
        decoderName = p->m_extensionToDecoderMap.at(extensions).front();
    }

    if(!p->m_decoders.contains(decoderName)) {
        return nullptr;
    }

    if(p->m_decoderInstances.hasLocalData()) {
        auto& decoders = p->m_decoderInstances.localData();
        if(decoders->contains(decoderName)) {
            return decoders->at(decoderName).get();
        }
        return decoders->emplace(decoderName, p->m_decoders.at(decoderName)()).first->second.get();
    }

    p->m_decoderInstances.setLocalData(new DecoderInstances());

    auto decoder = p->m_decoders.at(decoderName)();
    return p->m_decoderInstances.localData()->emplace(decoderName, std::move(decoder)).first->second.get();
}

AudioReader* AudioLoader::readerForFile(const QString& file) const
{
    const QFileInfo info{file};

    const std::shared_lock lock{p->m_readerMutex};

    const auto extensions = info.suffix().toLower();
    if(!p->m_extensionToReaderMap.contains(extensions)) {
        return nullptr;
    }

    QString readerName;

    if(file.first(9) == u"unpack://") {
        readerName = QStringLiteral("Archive");
    }
    else {
        readerName = p->m_extensionToReaderMap.at(extensions).front();
    }

    if(!p->m_readers.contains(readerName)) {
        return nullptr;
    }

    if(p->m_readerInstances.hasLocalData()) {
        auto& readers = p->m_readerInstances.localData();
        if(readers->contains(readerName)) {
            return readers->at(readerName).get();
        }
        return readers->emplace(readerName, p->m_readers.at(readerName)()).first->second.get();
    }

    p->m_readerInstances.setLocalData(new ReaderInstances());

    auto reader = p->m_readers.at(readerName)();
    return p->m_readerInstances.localData()->emplace(readerName, std::move(reader)).first->second.get();
}

AudioReader* AudioLoader::readerForTrack(const Track& track) const
{
    return readerForFile(track.filepath());
}

ArchiveReader* AudioLoader::archiveReaderForFile(const QString& file) const
{
    const QFileInfo info{file};

    const std::shared_lock lock{p->m_archiveReaderMutex};

    const auto extensions = info.suffix().toLower();
    if(!p->m_extensionToArchiveMap.contains(extensions)) {
        return nullptr;
    }

    const QString readerName = p->m_extensionToArchiveMap.at(extensions).front();
    if(!p->m_archiveReaders.contains(readerName)) {
        return nullptr;
    }

    if(p->m_archiveReaderInstances.hasLocalData()) {
        auto& readers = p->m_archiveReaderInstances.localData();
        if(readers->contains(readerName)) {
            return readers->at(readerName).get();
        }
        return readers->emplace(readerName, p->m_archiveReaders.at(readerName)()).first->second.get();
    }

    p->m_archiveReaderInstances.setLocalData(new ArchiveReaderInstances());

    auto reader = p->m_archiveReaders.at(readerName)();
    return p->m_archiveReaderInstances.localData()->emplace(readerName, std::move(reader)).first->second.get();
}

bool AudioLoader::readTrackMetadata(Track& track) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    auto* decoder = readerForTrack(track);
    if(!decoder) {
        qCInfo(AUD_LDR) << "Tag reader not available for file:" << track.filepath();
        return {};
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};
    if(!track.isInArchive()) {
        if(!file.open(QIODevice::ReadOnly)) {
            qCWarning(AUD_LDR) << "Failed to open file:" << source.filepath;
            return false;
        }
        source.device = &file;
    }

    if(decoder->init(source)) {
        return decoder->readTrack(source, track);
    }

    return false;
}

QByteArray AudioLoader::readTrackCover(const Track& track, Track::Cover cover) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    auto* decoder = readerForTrack(track);
    if(!decoder) {
        return {};
    }

    if(!track.isInArchive() && !decoder->canReadCover()) {
        return {};
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};
    if(!track.isInArchive()) {
        if(!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        source.device = &file;
    }

    if(decoder->init(source) && decoder->canReadCover()) {
        return decoder->readCover(source, track, cover);
    }

    return {};
}

bool AudioLoader::writeTrackMetadata(const Track& track, AudioReader::WriteOptions options) const
{
    if(track.isInArchive()) {
        return false;
    }

    const std::shared_lock lock{p->m_decoderMutex};

    auto* decoder = readerForTrack(track);
    if(!decoder || !decoder->canWriteMetaData()) {
        return false;
    }

    AudioSource source;
    source.filepath = track.filepath();
    QFile file{track.filepath()};
    if(!file.open(QIODeviceBase::ReadWrite)) {
        qCWarning(AUD_LDR) << "Failed to open file:" << source.filepath;
        return false;
    }
    source.device = &file;

    return decoder->writeTrack(source, track, options);
}

void AudioLoader::addDecoder(const QString& name, const DecoderCreator& creator)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Decoder" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_decoderMutex};

    if(p->m_decoders.contains(name)) {
        qCWarning(AUD_LDR) << "Decoder" << name << "already registered";
        return;
    }

    auto decoder    = creator();
    const auto exts = decoder->extensions();

    // TODO: Add order/priority to handle multiple decoders supporting same extensions
    for(const auto& extension : exts) {
        p->m_extensionToDecoderMap[extension].emplace_back(name);
    }

    p->m_decoders.emplace(name, creator);
}

void AudioLoader::addReader(const QString& name, const ReaderCreator& creator)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_readerMutex};

    if(p->m_readers.contains(name)) {
        qCWarning(AUD_LDR) << "Reader" << name << "already registered";
        return;
    }

    auto reader     = creator();
    const auto exts = reader->extensions();

    // TODO: Add order/priority to handle multiple readers supporting same extensions
    for(const auto& extension : exts) {
        p->m_extensionToReaderMap[extension].emplace_back(name);
    }

    p->m_readers.emplace(name, creator);
}

void AudioLoader::addArchiveReader(const QString& name, const ArchiveReaderCreator& creator)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_archiveReaderMutex};

    if(p->m_archiveReaders.contains(name)) {
        qCWarning(AUD_LDR) << "Reader" << name << "already registered";
        return;
    }

    auto reader     = creator();
    const auto exts = reader->extensions();

    // TODO: Add order/priority to handle multiple readers supporting same extensions
    for(const auto& extension : exts) {
        p->m_extensionToArchiveMap[extension].emplace_back(name);
    }

    p->m_archiveReaders.emplace(name, creator);
}

void AudioLoader::destroyThreadInstance()
{
    const std::scoped_lock lock{p->m_decoderMutex, p->m_readerMutex, p->m_archiveReaderMutex};

    p->m_decoderInstances.setLocalData(nullptr);
    p->m_readerInstances.setLocalData(nullptr);
    p->m_archiveReaderInstances.setLocalData(nullptr);
}
} // namespace Fooyin
