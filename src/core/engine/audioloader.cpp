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

Q_LOGGING_CATEGORY(AUD_LDR, "fy.audioloader")

namespace Fooyin {
using DecoderInstances       = std::unordered_map<QString, std::unique_ptr<AudioDecoder>>;
using ReaderInstances        = std::unordered_map<QString, std::unique_ptr<AudioReader>>;
using ArchiveReaderInstances = std::unordered_map<QString, std::unique_ptr<ArchiveReader>>;

template <typename T>
struct Loader
{
    int index{0};
    QString name;
    QStringList extensions;
    bool enabled{true};
    T creator;
};

class AudioLoaderPrivate
{
public:
    std::vector<Loader<DecoderCreator>> m_decoders;
    std::vector<Loader<ReaderCreator>> m_readers;
    std::vector<Loader<ArchiveReaderCreator>> m_archiveReaders;

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

    auto addExtensions = [&extensions](const auto& loaders) {
        for(const auto& loader : loaders) {
            extensions.append(loader.extensions);
        }
    };

    addExtensions(p->m_decoders);
    addExtensions(p->m_readers);
    addExtensions(p->m_archiveReaders);

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
    const QString ext = QFileInfo{file}.suffix().toLower();

    const std::shared_lock lock{p->m_archiveReaderMutex};
    return std::ranges::any_of(p->m_archiveReaders,
                               [&ext](const auto& loader) { return loader.extensions.contains(ext); });
}

AudioDecoder* AudioLoader::decoderForFile(const QString& file) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    const QString ext      = QFileInfo{file}.suffix().toLower();
    const bool isInArchive = file.length() >= 9 && file.first(9) == u"unpack://";

    for(const auto& loader : p->m_decoders) {
        if((isInArchive && loader.name == u"Archive") || loader.extensions.contains(ext)) {
            if(p->m_decoderInstances.hasLocalData()) {
                auto& readers = p->m_decoderInstances.localData();
                if(readers->contains(loader.name)) {
                    return readers->at(loader.name).get();
                }
                return readers->emplace(loader.name, loader.creator()).first->second.get();
            }
            p->m_decoderInstances.setLocalData(new DecoderInstances());
            return p->m_decoderInstances.localData()->emplace(loader.name, loader.creator()).first->second.get();
        }
    }

    return nullptr;
}

AudioDecoder* AudioLoader::decoderForTrack(const Track& track) const
{
    return decoderForFile(track.filepath());
}

AudioReader* AudioLoader::readerForFile(const QString& file) const
{
    const std::shared_lock lock{p->m_readerMutex};

    const QString ext      = QFileInfo{file}.suffix().toLower();
    const bool isInArchive = file.length() >= 9 && file.first(9) == u"unpack://";

    for(const auto& loader : p->m_readers) {
        if((isInArchive && loader.name == u"Archive") || loader.extensions.contains(ext)) {
            if(p->m_readerInstances.hasLocalData()) {
                auto& readers = p->m_readerInstances.localData();
                if(readers->contains(loader.name)) {
                    return readers->at(loader.name).get();
                }
                return readers->emplace(loader.name, loader.creator()).first->second.get();
            }
            p->m_readerInstances.setLocalData(new ReaderInstances());
            return p->m_readerInstances.localData()->emplace(loader.name, loader.creator()).first->second.get();
        }
    }

    return nullptr;
}

AudioReader* AudioLoader::readerForTrack(const Track& track) const
{
    return readerForFile(track.filepath());
}

ArchiveReader* AudioLoader::archiveReaderForFile(const QString& file) const
{
    const std::shared_lock lock{p->m_archiveReaderMutex};

    if(!isArchive(file)) {
        return nullptr;
    }

    const QString ext = QFileInfo{file}.suffix().toLower();

    for(const auto& loader : p->m_archiveReaders) {
        if(loader.extensions.contains(ext)) {
            if(p->m_archiveReaderInstances.hasLocalData()) {
                auto& readers = p->m_archiveReaderInstances.localData();
                if(readers->contains(loader.name)) {
                    return readers->at(loader.name).get();
                }
                return readers->emplace(loader.name, loader.creator()).first->second.get();
            }
            p->m_archiveReaderInstances.setLocalData(new ArchiveReaderInstances());
            return p->m_archiveReaderInstances.localData()->emplace(loader.name, loader.creator()).first->second.get();
        }
    }

    return nullptr;
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

    if(std::ranges::any_of(p->m_decoders, [&name](const auto& loader) { return loader.name == name; })) {
        qCWarning(AUD_LDR) << "Decoder" << name << "already registered";
        return;
    }

    auto decoder = creator();

    Loader<DecoderCreator> loader;
    loader.name       = name;
    loader.index      = static_cast<int>(p->m_decoders.size());
    loader.extensions = decoder->extensions();
    loader.creator    = creator;

    p->m_decoders.push_back(loader);
}

void AudioLoader::addReader(const QString& name, const ReaderCreator& creator)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_readerMutex};

    if(std::ranges::any_of(p->m_readers, [&name](const auto& loader) { return loader.name == name; })) {
        qCWarning(AUD_LDR) << "Reader" << name << "already registered";
        return;
    }

    auto reader = creator();

    Loader<ReaderCreator> loader;
    loader.name       = name;
    loader.index      = static_cast<int>(p->m_readers.size());
    loader.extensions = reader->extensions();
    loader.creator    = creator;

    p->m_readers.push_back(loader);
}

void AudioLoader::addArchiveReader(const QString& name, const ArchiveReaderCreator& creator)
{
    if(!creator) {
        qCWarning(AUD_LDR) << "Reader" << name << "cannot be created";
        return;
    }

    const std::unique_lock lock{p->m_archiveReaderMutex};

    if(std::ranges::any_of(p->m_archiveReaders, [&name](const auto& loader) { return loader.name == name; })) {
        qCWarning(AUD_LDR) << "Reader" << name << "already registered";
        return;
    }

    auto reader = creator();

    Loader<ArchiveReaderCreator> loader;
    loader.name       = name;
    loader.index      = static_cast<int>(p->m_archiveReaders.size());
    loader.extensions = reader->extensions();
    loader.creator    = creator;

    p->m_archiveReaders.push_back(loader);
}

void AudioLoader::destroyThreadInstance()
{
    const std::scoped_lock lock{p->m_decoderMutex, p->m_readerMutex, p->m_archiveReaderMutex};

    p->m_decoderInstances.setLocalData(nullptr);
    p->m_readerInstances.setLocalData(nullptr);
    p->m_archiveReaderInstances.setLocalData(nullptr);
}
} // namespace Fooyin
