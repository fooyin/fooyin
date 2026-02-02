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

#pragma once

#include "fycore_export.h"

#include <core/engine/enginedefs.h>

#include <QByteArray>

namespace Fooyin {
class DspNode;
class DspRegistry;
class EngineHandler;
class SettingsManager;

class FYCORE_EXPORT DspChainStore
{
public:
    DspChainStore(SettingsManager* settings, DspRegistry* registry, EngineHandler* engine = nullptr);

    void setEngine(EngineHandler* engine);

    [[nodiscard]] Engine::DspChain availableDsps() const;
    [[nodiscard]] Engine::DspChains activeChain() const;
    [[nodiscard]] std::unique_ptr<DspNode> createDsp(const QString& id) const;

    void setActiveChain(const Engine::DspChains& chain);

private:
    void loadFromSettings();
    void persistChain(const Engine::DspChains& chain);
    [[nodiscard]] Engine::DspChains normaliseChain(const Engine::DspChains& chain) const;

    SettingsManager* m_settings;
    DspRegistry* m_registry;
    EngineHandler* m_engine;

    Engine::DspChains m_activeChain;
};
} // namespace Fooyin
