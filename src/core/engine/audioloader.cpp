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

#include "taglibparser.h"

#include <core/track.h>

#include <QThreadStorage>

#include <mutex>
#include <shared_mutex>

namespace Fooyin {
using LoaderInstances = std::unordered_map<QString, std::unique_ptr<AudioInput>>;

class AudioLoaderPrivate
{
public:
    std::unordered_map<QString, InputCreator> m_decoders;
    std::unordered_map<QString, std::vector<QString>> m_extensionToDecoderMap;
    QThreadStorage<LoaderInstances*> m_instances;
    std::shared_mutex m_decoderMutex;
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
    if(auto* decoder = decoderForTrack(track)) {
        return decoder->canWriteMetaData();
    }

    return Tagging::supportedExtensions().contains(track.extension());
}

AudioInput* AudioLoader::decoderForTrack(const Track& track) const
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

    if(p->m_instances.hasLocalData()) {
        auto& decoders = p->m_instances.localData();
        if(decoders->contains(decoderName)) {
            return decoders->at(decoderName).get();
        }
        return decoders->emplace(decoderName, p->m_decoders.at(decoderName)()).first->second.get();
    }

    p->m_instances.setLocalData(new LoaderInstances());

    auto decoder = p->m_decoders.at(decoderName)();
    return p->m_instances.localData()->emplace(decoderName, std::move(decoder)).first->second.get();
}

bool AudioLoader::readTrackMetadata(Track& track) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    if(Tagging::supportedExtensions().contains(track.extension())) {
        return Tagging::readMetaData(track);
    }
    if(auto* decoder = decoderForTrack(track)) {
        return decoder->readMetaData(track);
    }

    return false;
}

QByteArray AudioLoader::readTrackCover(const Track& track, Track::Cover cover) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    if(Tagging::supportedExtensions().contains(track.extension())) {
        return Tagging::readCover(track, cover);
    }
    if(auto* decoder = decoderForTrack(track)) {
        if(decoder->canReadCover()) {
            return decoder->readCover(track, cover);
        }
    }

    return {};
}

bool AudioLoader::writeTrackMetadata(const Track& track, const AudioInput::WriteOptions& options) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    if(Tagging::supportedExtensions().contains(track.extension())) {
        return Tagging::writeMetaData(track, options);
    }
    if(auto* decoder = decoderForTrack(track)) {
        if(decoder->canWriteMetaData()) {
            return decoder->writeMetaData(track, options);
        }
    }

    return false;
}

void AudioLoader::addDecoder(const QString& name, const InputCreator& creator)
{
    const std::unique_lock lock{p->m_decoderMutex};

    if(p->m_decoders.contains(name)) {
        qDebug() << QStringLiteral("Decoder (%1) already registered").arg(name);
        return;
    }

    auto decoder    = creator();
    const auto exts = decoder->supportedExtensions();

    // TODO: Add order/priority to handle multiple decoders supporting same extensions
    for(const auto& extension : exts) {
        p->m_extensionToDecoderMap[extension].emplace_back(name);
    }

    p->m_decoders.emplace(name, creator);
}

void AudioLoader::destroyThreadInstance()
{
    const std::shared_lock lock{p->m_decoderMutex};
    p->m_instances.setLocalData(nullptr);
}
} // namespace Fooyin
