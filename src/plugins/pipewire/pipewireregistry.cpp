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

#include "pipewireregistry.h"

#include "pipewirecore.h"
#include "pipewireutils.h"

#include <pipewire/keys.h>
#include <pipewire/node.h>
#include <pipewire/type.h>

#include <QDebug>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

namespace Fooyin::Pipewire {
PipewireRegistry::PipewireRegistry(PipewireCore* core)
    : m_registry{pw_core_get_registry(core->core(), PW_VERSION_REGISTRY, 0)}
{
    if(!m_registry) {
        qCWarning(PIPEWIRE) << "Failed to create registry";
    }

    static constexpr pw_registry_events registryEvents = {
        .version = PW_VERSION_REGISTRY_EVENTS,
        .global  = onRegistryEvent,
    };

    pw_registry_add_listener(m_registry.get(), &m_registryListener, &registryEvents, this); // NOLINT
}

PipewireRegistry::~PipewireRegistry() = default;

OutputDevices PipewireRegistry::devices() const
{
    return m_sinks;
}

void PipewireRegistry::onRegistryEvent(void* data, uint32_t /*id*/, uint32_t /*permissions*/, const char* type,
                                       uint32_t /*version*/, const spa_dict* props)
{
    auto* registry = static_cast<PipewireRegistry*>(data);

    if(strcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
        return;
    }

    const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if(!media_class) {
        return;
    }

    if(strcmp(media_class, "Audio/Sink") != 0) {
        return;
    }

    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* desc = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

    registry->m_sinks.emplace_back(QString::fromUtf8(name), QString::fromUtf8(desc));
}
} // namespace Fooyin::Pipewire
