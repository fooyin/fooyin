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
#include <QHash>
#include <QIODevice>

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
        stream << quint32(list.size());

        for(const auto& entry : list) {
            stream << entry.id;
            stream << entry.hasSettings;
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

    if(stream.status() != QDataStream::Ok || version != ChainVersion) {
        return {};
    }

    const auto deserialise = [&stream](DspChain& list) {
        quint32 count{0};
        stream >> count;

        for(quint32 i{0}; i < count; ++i) {
            DspDefinition entry;

            stream >> entry.id;
            stream >> entry.hasSettings;
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

Engine::DspChain DspChainStore::availableDsps() const
{
    Engine::DspChain defs;

    if(!m_registry) {
        return defs;
    }

    const auto entries = m_registry->entries();
    defs.reserve(entries.size());

    for(const auto& entry : entries) {
        const auto instance = entry.factory();
        defs.push_back({entry.id, entry.name, false, {}});
    }

    return defs;
}

Engine::DspChains DspChainStore::activeChain() const
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
    persistChain(m_activeChain);

    if(m_engine) {
        m_engine->setDspChain(m_activeChain);
    }
}

void DspChainStore::loadFromSettings()
{
    if(!m_settings) {
        m_activeChain.clear();
        return;
    }

    const auto chainData = m_settings->fileValue(ActiveChainKey).toByteArray();
    m_activeChain        = normaliseChain(deserialiseChain(chainData));
}

void DspChainStore::persistChain(const Engine::DspChains& chain)
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

Engine::DspChains DspChainStore::normaliseChain(const Engine::DspChains& chain) const
{
    if(!m_registry) {
        return chain;
    }

    QHash<QString, QString> names;

    const auto entries = m_registry->entries();
    names.reserve(static_cast<int>(entries.size()));

    for(const auto& entry : entries) {
        names.insert(entry.id, entry.name);
    }

    const auto normalise = [&names](const DspChain& subChain) {
        Engine::DspChain normalised;
        normalised.reserve(subChain.size());

        for(const auto& entry : subChain) {
            Engine::DspDefinition normalisedEntry{entry};
            const auto it = names.constFind(entry.id);

            if(it != names.constEnd()) {
                normalisedEntry.name = it.value();
            }
            else if(normalisedEntry.name.isEmpty()) {
                normalisedEntry.name = entry.id;
            }

            normalised.push_back(std::move(normalisedEntry));
        }

        return normalised;
    };

    Engine::DspChains normalised;
    normalised.perTrackChain = normalise(chain.perTrackChain);
    normalised.masterChain   = normalise(chain.masterChain);

    return normalised;
}
} // namespace Fooyin
