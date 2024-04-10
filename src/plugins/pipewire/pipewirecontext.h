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

#include "pipewirethreadloop.h"

#include <pipewire/context.h>

namespace Fooyin::Pipewire {
class PipewireContext
{
public:
    explicit PipewireContext(PipewireThreadLoop* loop);

    [[nodiscard]] pw_context* context() const;
    [[nodiscard]] PipewireThreadLoop* threadLoop() const;

private:
    struct PwContextDeleter
    {
        void operator()(pw_context* context)
        {
            if(context) {
                pw_context_destroy(context);
            }
        }
    };
    using PwContextUPtr = std::unique_ptr<pw_context, PwContextDeleter>;

    PipewireThreadLoop* m_loop;
    PwContextUPtr m_context;
};
} // namespace Fooyin::Pipewire
