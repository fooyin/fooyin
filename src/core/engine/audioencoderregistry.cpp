/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <core/engine/audioencoderregistry.h>

namespace Fooyin {
void AudioEncoderRegistry::addEncoderBackend(const QString& id, const QString& name, EncoderCreator creator)
{
    if(id.isEmpty() || !creator) {
        return;
    }

    std::erase_if(m_backends, [&id](const Backend& backend) { return backend.id == id; });
    m_backends.emplace_back(id, name, std::move(creator));
}

std::vector<AudioEncoderInfo> AudioEncoderRegistry::availableEncoders() const
{
    std::vector<AudioEncoderInfo> encoders;

    for(const Backend& backend : m_backends) {
        const auto encoder = backend.creator();
        if(!encoder) {
            continue;
        }

        const auto encoderInfos = encoder->availableEncoders();
        for(AudioEncoderInfo info : encoderInfos) {
            if(info.backendId.isEmpty()) {
                info.backendId = backend.id;
            }
            if(info.id.isEmpty()) {
                info.id = info.profile.id;
            }
            if(info.id.isEmpty()) {
                continue;
            }
            encoders.push_back(std::move(info));
        }
    }

    return encoders;
}

std::unique_ptr<AudioEncoder> AudioEncoderRegistry::createEncoder(const QString& encoderId) const
{
    if(encoderId.isEmpty()) {
        return {};
    }

    for(const Backend& backend : m_backends) {
        auto probe = backend.creator();
        if(!probe) {
            continue;
        }

        const auto infos   = probe->availableEncoders();
        const bool matches = std::ranges::any_of(infos, [&encoderId](const AudioEncoderInfo& info) {
            return info.id == encoderId || (!info.profile.id.isEmpty() && info.profile.id == encoderId);
        });
        if(matches) {
            return backend.creator();
        }
    }

    return {};
}

void AudioEncoderRegistry::reset()
{
    m_backends.clear();
}
} // namespace Fooyin
