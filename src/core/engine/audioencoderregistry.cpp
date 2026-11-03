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

#include <utils/stringcollator.h>

#include <mutex>
#include <shared_mutex>

namespace Fooyin {
namespace {
struct Backend
{
    QString id;
    QString name;
    EncoderCreator creator;
    AudioEncoderRegistry::BackendPriority priority{AudioEncoderRegistry::BackendPriority::Normal};
};

std::vector<AudioEncoderInfo> collectEncoderInfos(const std::vector<Backend>& backends)
{
    std::vector<AudioEncoderInfo> encoders;

    for(const auto& backend : backends) {
        const auto encoder = backend.creator();
        if(!encoder) {
            continue;
        }

        const auto encoderInfos = encoder->availableEncoders();
        for(AudioEncoderInfo info : encoderInfos) {
            info.backendId   = backend.id;
            info.backendName = backend.name;
            if(info.id.isEmpty()) {
                info.id = info.profile.id;
            }
            if(info.id.isEmpty()) {
                continue;
            }
            if(info.profile.formatId.isEmpty()) {
                info.profile.formatId = info.id;
            }
            info.backendPriority = static_cast<int>(backend.priority);
            encoders.push_back(std::move(info));
        }
    }

    const StringCollator collator;
    std::ranges::sort(encoders, [&collator](const AudioEncoderInfo& lhs, const AudioEncoderInfo& rhs) {
        return collator.compare(lhs.name, rhs.name) < 0;
    });

    return encoders;
}

std::optional<AudioEncoderInfo> findEncoderInfo(const std::vector<Backend>& backends, const QString& encoderId)
{
    if(encoderId.isEmpty()) {
        return {};
    }

    const std::vector<AudioEncoderInfo> encoders = collectEncoderInfos(backends);

    if(const auto exact = std::ranges::find_if(
           encoders,
           [&encoderId](const AudioEncoderInfo& info) { return info.id == encoderId || info.profile.id == encoderId; });
       exact != encoders.end()) {
        return *exact;
    }

    std::optional<AudioEncoderInfo> preferred;
    for(const AudioEncoderInfo& info : encoders) {
        if(info.profile.formatId == encoderId && (!preferred || info.backendPriority > preferred->backendPriority)) {
            preferred = info;
        }
    }

    if(preferred) {
        preferred->id         = encoderId;
        preferred->profile.id = encoderId;
    }
    return preferred;
}
} // namespace

class AudioEncoderRegistryPrivate
{
public:
    [[nodiscard]] std::vector<Backend> backendSnapshot() const
    {
        const std::shared_lock lock{m_mutex};
        return m_backends;
    }

    std::vector<Backend> m_backends;
    mutable std::shared_mutex m_mutex;
};

AudioEncoderRegistry::AudioEncoderRegistry()
    : p{std::make_unique<AudioEncoderRegistryPrivate>()}
{ }

AudioEncoderRegistry::~AudioEncoderRegistry() = default;

void AudioEncoderRegistry::addEncoderBackend(const QString& id, const QString& name, EncoderCreator creator,
                                             BackendPriority priority)
{
    if(id.isEmpty() || !creator) {
        return;
    }

    const std::unique_lock lock{p->m_mutex};

    std::erase_if(p->m_backends, [&id](const Backend& backend) { return backend.id == id; });
    p->m_backends.emplace_back(id, name, std::move(creator), priority);
}

std::vector<AudioEncoderInfo> AudioEncoderRegistry::availableEncoders() const
{
    return collectEncoderInfos(p->backendSnapshot());
}

std::vector<AudioEncoderInfo> AudioEncoderRegistry::preferredEncoders() const
{
    std::vector<AudioEncoderInfo> preferred;

    const auto encoders = availableEncoders();
    for(AudioEncoderInfo info : encoders) {
        const auto existing = std::ranges::find(
            preferred, info.profile.formatId, [](const AudioEncoderInfo& encoder) { return encoder.profile.formatId; });
        if(existing == preferred.end()) {
            preferred.push_back(std::move(info));
        }
        else if(info.backendPriority > existing->backendPriority) {
            *existing = std::move(info);
        }
    }

    const StringCollator collator;
    std::ranges::sort(preferred, [&collator](const AudioEncoderInfo& lhs, const AudioEncoderInfo& rhs) {
        return collator.compare(lhs.name, rhs.name) < 0;
    });

    return preferred;
}

std::optional<AudioEncoderInfo> AudioEncoderRegistry::encoderInfo(const QString& encoderId) const
{
    if(encoderId.isEmpty()) {
        return {};
    }
    return findEncoderInfo(p->backendSnapshot(), encoderId);
}

std::unique_ptr<AudioEncoder> AudioEncoderRegistry::createEncoder(const QString& encoderId) const
{
    if(encoderId.isEmpty()) {
        return {};
    }

    const auto backends = p->backendSnapshot();
    const auto info     = findEncoderInfo(backends, encoderId);
    if(!info) {
        return {};
    }

    const auto backend = std::ranges::find(backends, info->backendId, &Backend::id);
    if(backend != backends.end()) {
        return backend->creator();
    }
    return {};
}

void AudioEncoderRegistry::reset()
{
    const std::unique_lock lock{p->m_mutex};
    p->m_backends.clear();
}
} // namespace Fooyin
