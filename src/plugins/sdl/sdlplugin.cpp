/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "sdlplugin.h"

#include "sdlaudiosubsystem.h"
#include "sdloutput.h"

#include <SDL2/SDL.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Sdl {
SdlPlugin::SdlPlugin()
    : m_audioSubsystem{std::make_shared<SdlAudioSubsystem>()}
{ }

QString SdlPlugin::name() const
{
    auto audioLease    = m_audioSubsystem->acquire();
    const QString name = u"SDL2 (%1)"_s.arg(QString::fromLatin1(SDL_GetCurrentAudioDriver()));
    return name;
}

OutputCreator SdlPlugin::creator() const
{
    return [audioSubsystem = m_audioSubsystem]() {
        return std::make_unique<SdlOutput>(audioSubsystem);
    };
}
} // namespace Fooyin::Sdl

#include "moc_sdlplugin.cpp"
