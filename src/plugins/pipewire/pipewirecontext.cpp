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

#include "pipewirecontext.h"

#include "pipewireutils.h"

#include <QDebug>

namespace Fooyin::Pipewire {
PipewireContext::PipewireContext(PipewireThreadLoop* loop)
    : m_loop{loop}
    , m_context{pw_context_new(m_loop->loop(), nullptr, 0)}
{
    if(!m_context) {
        qCWarning(PIPEWIRE) << "Failed to create context";
    }
}

pw_context* PipewireContext::context() const
{
    if(!m_context) {
        return nullptr;
    }

    return m_context.get();
}

PipewireThreadLoop* PipewireContext::threadLoop() const
{
    return m_loop;
}
} // namespace Fooyin::Pipewire
