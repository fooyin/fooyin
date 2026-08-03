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

#include "sdlaudiosubsystem.h"

#include <SDL2/SDL.h>

namespace Fooyin::Sdl {
SdlAudioLease::SdlAudioLease(SdlAudioSubsystem* subsystem)
    : m_subsystem{subsystem}
{ }

SdlAudioLease::~SdlAudioLease()
{
    if(m_subsystem) {
        m_subsystem->release();
    }
}

SdlAudioLease::SdlAudioLease(SdlAudioLease&& other) noexcept
    : m_subsystem{std::exchange(other.m_subsystem, nullptr)}
{ }

SdlAudioLease& SdlAudioLease::operator=(SdlAudioLease&& other) noexcept
{
    if(this != &other) {
        if(m_subsystem) {
            m_subsystem->release();
        }
        m_subsystem = std::exchange(other.m_subsystem, nullptr);
    }
    return *this;
}

std::optional<SdlAudioLease> SdlAudioSubsystem::acquire()
{
    const std::scoped_lock lock{m_mutex};
    if(SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        return {};
    }
    return SdlAudioLease{this};
}

void SdlAudioSubsystem::release()
{
    const std::scoped_lock lock{m_mutex};

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    if(SDL_WasInit(0) == 0) {
        SDL_Quit();
    }
}
} // namespace Fooyin::Sdl
