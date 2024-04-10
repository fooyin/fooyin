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

#include "pipewirecontext.h"

#include <pipewire/core.h>

#include <memory>

namespace Fooyin::Pipewire {
class PipewireCore
{
public:
    explicit PipewireCore(PipewireContext* context);

    [[nodiscard]] pw_core* core() const;

    [[nodiscard]] bool initialised() const;
    [[nodiscard]] int sync() const;

    void syncCore();

private:
    static void onCoreDone(void* userdata, uint32_t id, int seq);

    struct PwCoreDeleter
    {
        void operator()(pw_core* core) const
        {
            if(core) {
                pw_core_disconnect(core);
            }
        }
    };
    using PwCoreUPtr = std::unique_ptr<pw_core, PwCoreDeleter>;

    PipewireContext* m_context;
    bool m_initialised;
    PwCoreUPtr m_core;
    spa_hook m_coreListener;
    int m_sync;
};
} // namespace Fooyin::Pipewire
