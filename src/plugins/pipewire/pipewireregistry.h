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

#pragma once

#include <core/engine/audiooutput.h>

#include <pipewire/core.h>
#include <spa/utils/hook.h>

namespace Fooyin::Pipewire {
class PipewireCore;

class PipewireRegistry
{
public:
    explicit PipewireRegistry(PipewireCore* core);
    ~PipewireRegistry();

    [[nodiscard]] OutputDevices devices() const;

private:
    static void onRegistryEvent(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version,
                                const struct spa_dict* props);

    struct PwRegistryDeleter
    {
        void operator()(pw_registry* registry) const
        {
            if(registry) {
                pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
            }
        }
    };
    using PwRegistryUPtr = std::unique_ptr<pw_registry, PwRegistryDeleter>;

    PwRegistryUPtr m_registry;
    spa_hook m_registryListener;
    OutputDevices m_sinks;
};
} // namespace Fooyin::Pipewire
