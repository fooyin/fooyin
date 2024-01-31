/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiooutput.h>

namespace Fooyin {
OutputType AudioPushOutput::type() const
{
    return OutputType::Push;
}

void AudioPushOutput::setPaused(bool /*pause*/) { }

void AudioPushOutput::setVolume(double /*volume*/) { }

void AudioPullOutput::reset() { }

OutputType AudioPullOutput::type() const
{
    return OutputType::Pull;
}

OutputState AudioPullOutput::currentState()
{
    return {};
}

int AudioPullOutput::bufferSize() const
{
    return 0;
}

int AudioPullOutput::write(const AudioBuffer& /*buffer*/)
{
    return 0;
}

void AudioPullOutput::setPaused(bool /*pause*/) { }

void AudioPullOutput::setVolume(double /*volume*/) { }
} // namespace Fooyin
