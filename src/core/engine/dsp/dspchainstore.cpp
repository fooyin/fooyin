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

#include "dspchainstore.h"

#include "dspregistry.h"
#include "engine/enginehandler.h"

#include <utils/settings/settingsmanager.h>

#include <QDataStream>
#include <QIODevice>

#include <cstdint>
#include <limits>
#include <type_traits>
#include <unordered_set>

constexpr auto ActiveChainKey = "DSP/ActiveChain";
constexpr auto ChainVersion   = 1;

namespace {
using namespace Fooyin::Engine;

QByteArray serialiseChain(const DspChains& chain)
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(ChainVersion);

    const auto serialise = [&stream](const DspChain& list) {
        stream << static_cast<quint32>(list.size());

        for(const auto& entry : list) {
            stream << entry.id;
            stream << entry.hasSettings;
            stream << entry.enabled;
            stream << static_cast<quint64>(entry.instanceId);
            stream << entry.settings;
        }
    };

    serialise(chain.perTrackChain);
    serialise(chain.masterChain);

    return qCompress(data, 9);
}

DspChains deserialiseChain(const QByteArray& data)
{
    DspChains chain;
    if(data.isEmpty()) {
        return chain;
    }

    QByteArray uncompressed = qUncompress(data);
    if(uncompressed.isEmpty()) {
        return chain;
    }

    QDataStream stream{&uncompressed, QIODevice::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 version{0};
    stream >> version;

    if(stream.status() != QDataStream::Ok || (version < 1 || version > ChainVersion)) {
        return {};
    }

    const auto deserialise = [&stream](DspChain& list) {
        quint32 count{0};
        stream >> count;

        for(quint32 i{0}; i < count; ++i) {
            DspDefinition entry;

            stream >> entry.id;
            stream >> entry.hasSettings;
            stream >> entry.enabled;

            quint64 instanceId{0};
            stream >> instanceId;
            entry.instanceId = instanceId;

            stream >> entry.settings;

            if(stream.status() != QDataStream::Ok) {
                return;
            }

            list.push_back(std::move(entry));
        }
    };

    deserialise(chain.perTrackChain);
    deserialise(chain.masterChain);

    return chain;
}
} // namespace

namespace Fooyin {
DspChainStore::DspChainStore(SettingsManager* settings, DspRegistry* registry, EngineHandler* engine)
    : m_settings{settings}
    , m_registry{registry}
    , m_engine{engine}
    , m_nextInstanceId{1}
{
    loadFromSettings();
}

void DspChainStore::setEngine(EngineHandler* engine)
{
    m_engine      = engine;
    m_activeChain = normaliseChain(m_activeChain);

    if(m_engine) {
        m_engine->setDspChain(m_activeChain);
    }
}

DspChain DspChainStore::availableDsps() const
{
    DspChain defs;

    if(!m_registry) {
        return defs;
    }

    const auto entries = m_registry->entries();
    defs.reserve(entries.size());

    for(const auto& entry : entries) {
        defs.push_back({entry.id, entry.name, false, true, 0, {}});
    }

    return defs;
}

DspChains DspChainStore::activeChain() const
{
    return m_activeChain;
}

std::unique_ptr<DspNode> DspChainStore::createDsp(const QString& id) const
{
    return m_registry ? m_registry->create(id) : nullptr;
}

void DspChainStore::setActiveChain(const Engine::DspChains& chain)
{
    m_activeChain = normaliseChain(chain);
    m_liveRevisionByKey.clear();
    persistChain(m_activeChain);

    if(m_engine) {
        m_engine->setDspChain(m_activeChain);
    }
}

bool DspChainStore::updateLiveDspSettings(const Engine::DspChainScope scope, uint64_t instanceId,
                                          const QByteArray& settings, bool persist)
{
    if(instanceId == 0) {
        return false;
    }

    auto inChain = [instanceId](const Engine::DspChain& chain) {
        return std::ranges::any_of(chain, [instanceId](const auto& entry) { return entry.instanceId == instanceId; });
    };

    DspChain* targetChain{nullptr};
    if(scope == Engine::DspChainScope::Master) {
        targetChain = &m_activeChain.masterChain;
    }
    else {
        targetChain = &m_activeChain.perTrackChain;
    }

    if(!targetChain || !inChain(*targetChain)) {
        return false;
    }

    if(persist) {
        for(auto& entry : *targetChain) {
            if(entry.instanceId == instanceId) {
                entry.settings = settings;
            }
        }
        persistChain(m_activeChain);
    }

    if(m_engine) {
        m_engine->updateLiveDspSettings(LiveDspSettingsUpdate{
            .scope      = scope,
            .instanceId = instanceId,
            .settings   = settings,
            .revision   = nextLiveRevision(scope, instanceId),
        });
    }

    return true;
}

void DspChainStore::loadFromSettings()
{
    if(!m_settings) {
        m_activeChain.clear();
        return;
    }

    const auto chainData = m_settings->fileValue(ActiveChainKey).toByteArray();
    m_activeChain        = normaliseChain(deserialiseChain(chainData));

    m_liveRevisionByKey.clear();
    if(!chainData.isEmpty()) {
        persistChain(m_activeChain);
    }
}

void DspChainStore::persistChain(const DspChains& chain)
{
    if(!m_settings) {
        return;
    }

    if(chain.isEmpty()) {
        m_settings->fileRemove(ActiveChainKey);
        return;
    }

    m_settings->fileSet(ActiveChainKey, serialiseChain(chain));
}

uint64_t DspChainStore::nextInstanceId()
{
    if(m_nextInstanceId == 0 || m_nextInstanceId == std::numeric_limits<uint64_t>::max()) {
        m_nextInstanceId = 1;
    }

    return m_nextInstanceId++;
}

void DspChainStore::noteExistingInstanceId(uint64_t instanceId)
{
    if(instanceId == 0) {
        return;
    }

    if(instanceId >= m_nextInstanceId && instanceId < std::numeric_limits<uint64_t>::max()) {
        m_nextInstanceId = instanceId + 1;
    }
}

uint64_t DspChainStore::nextLiveRevision(const DspChainScope scope, uint64_t instanceId)
{
    const auto scopeRaw = static_cast<uint64_t>(static_cast<std::underlying_type_t<DspChainScope>>(scope));
    const uint64_t key  = (scopeRaw << 63) | (instanceId & ~(1ULL << 63));

    uint64_t revision{0};
    if(const auto it = m_liveRevisionByKey.find(key); it != m_liveRevisionByKey.end()) {
        revision = it->second;
    }

    ++revision;
    m_liveRevisionByKey[key] = revision;

    return revision;
}

DspChains DspChainStore::normaliseChain(const Engine::DspChains& chain)
{
    std::unordered_map<QString, QString> names;

    if(m_registry) {
        const auto entries = m_registry->entries();
        names.reserve(entries.size());

        for(const auto& entry : entries) {
            names[entry.id] = entry.name;
        }
    }

    std::unordered_set<uint64_t> usedInstanceIds;

    const auto normalise = [this, &names, &usedInstanceIds](const DspChain& subChain) {
        DspChain normalised;
        normalised.reserve(subChain.size());

        for(const auto& entry : subChain) {
            DspDefinition normalisedEntry{entry};
            if(normalisedEntry.instanceId == 0 || usedInstanceIds.contains(normalisedEntry.instanceId)) {
                normalisedEntry.instanceId = nextInstanceId();
            }

            usedInstanceIds.insert(normalisedEntry.instanceId);
            noteExistingInstanceId(normalisedEntry.instanceId);

            const auto it = names.find(entry.id);

            if(it != names.end()) {
                normalisedEntry.name = it->second;
            }
            else if(normalisedEntry.name.isEmpty()) {
                normalisedEntry.name = entry.id;
            }

            normalised.push_back(std::move(normalisedEntry));
        }

        return normalised;
    };

    DspChains normalised;
    normalised.perTrackChain = normalise(chain.perTrackChain);
    normalised.masterChain   = normalise(chain.masterChain);

    return normalised;
}
} // namespace Fooyin
