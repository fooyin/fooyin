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

#include <QLoggingCategory>
#include <QThreadStorage>

#include <mutex>
#include <shared_mutex>

Q_LOGGING_CATEGORY(AUD_LDR, "AudioLoader")

namespace Fooyin {
using DecoderInstances = std::unordered_map<QString, std::unique_ptr<AudioDecoder>>;
using ReaderInstances  = std::unordered_map<QString, std::unique_ptr<AudioReader>>;

class AudioLoaderPrivate
{
public:
    std::unordered_map<QString, DecoderCreator> m_decoders;
    std::unordered_map<QString, ReaderCreator> m_readers;

    std::unordered_map<QString, std::vector<QString>> m_extensionToDecoderMap;
    std::unordered_map<QString, std::vector<QString>> m_extensionToReaderMap;

    QThreadStorage<DecoderInstances*> m_decoderInstances;
    QThreadStorage<ReaderInstances*> m_readerInstances;

    std::shared_mutex m_decoderMutex;
    std::shared_mutex m_readerMutex;
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

    return extensions;
}

bool AudioLoader::canWriteMetadata(const Track& track) const
{
    if(auto* decoder = readerForTrack(track)) {
        return decoder->canWriteMetaData();
    }
    return false;
}

AudioDecoder* AudioLoader::decoderForTrack(const Track& track) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    const auto extensions = track.extension();
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

AudioReader* AudioLoader::readerForTrack(const Track& track) const
{
    const std::shared_lock lock{p->m_readerMutex};

    const auto extensions = track.extension();
    if(!p->m_extensionToReaderMap.contains(extensions)) {
        return nullptr;
    }

    const QString readerName = p->m_extensionToReaderMap.at(extensions).front();
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

bool AudioLoader::readTrackMetadata(Track& track) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    if(auto* decoder = readerForTrack(track)) {
        return decoder->readMetaData(track);
    }

    return false;
}

QByteArray AudioLoader::readTrackCover(const Track& track, Track::Cover cover) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    if(auto* decoder = readerForTrack(track)) {
        if(decoder->canReadCover()) {
            return decoder->readCover(track, cover);
        }
    }

    return {};
}

bool AudioLoader::writeTrackMetadata(const Track& track, AudioReader::WriteOptions options) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    if(auto* decoder = readerForTrack(track)) {
        if(decoder->canWriteMetaData()) {
            return decoder->writeMetaData(track, options);
        }
    }

    return false;
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

void AudioLoader::destroyThreadInstance()
{
    const std::scoped_lock lock{p->m_decoderMutex, p->m_readerMutex};

    p->m_decoderInstances.setLocalData(nullptr);
    p->m_readerInstances.setLocalData(nullptr);
}
} // namespace Fooyin
