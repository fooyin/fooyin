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

#include <core/engine/decoderprovider.h>

#include <core/track.h>

#include <mutex>
#include <shared_mutex>

namespace Fooyin {
struct DecoderProvider::Private
{
    std::unordered_map<QString, DecoderCreator> m_decoders;
    std::unordered_map<QString, std::vector<QString>> m_extensionToDecoderMap;
    std::shared_mutex m_decoderMutex;
};

DecoderProvider::DecoderProvider()
    : p{std::make_unique<Private>()}
{ }

DecoderProvider::~DecoderProvider() = default;

QStringList DecoderProvider::supportedFileExtensions() const
{
    const std::shared_lock lock{p->m_decoderMutex};

    QStringList extensions;

    for(const auto& [ext, _] : p->m_extensionToDecoderMap) {
        extensions.emplace_back(ext);
    }

    return extensions;
}

std::unique_ptr<AudioDecoder> DecoderProvider::createDecoderForTrack(const Track& track) const
{
    const std::shared_lock lock{p->m_decoderMutex};

    const auto extensions = track.extension();
    if(p->m_extensionToDecoderMap.contains(extensions)) {
        const QString decoderName = p->m_extensionToDecoderMap.at(extensions).front();
        if(p->m_decoders.contains(decoderName)) {
            return p->m_decoders.at(decoderName)();
        }
    }

    return nullptr;
}

void DecoderProvider::addDecoder(const QString& name, const QStringList& supportedExtensions,
                                 const DecoderCreator& creator)
{
    const std::unique_lock lock{p->m_decoderMutex};

    if(p->m_decoders.contains(name)) {
        qDebug() << QStringLiteral("Decoder (%1) already registered").arg(name);
        return;
    }

    // TODO: Add order/priority to handle multiple decoders supporting same extensions
    for(const auto& extension : supportedExtensions) {
        p->m_extensionToDecoderMap[extension].emplace_back(name);
    }

    p->m_decoders.emplace(name, creator);
}
} // namespace Fooyin
