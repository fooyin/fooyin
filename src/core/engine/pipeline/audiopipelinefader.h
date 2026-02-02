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

#include <core/engine/enginedefs.h>

#include <cstdint>

namespace Fooyin {
class AudioPipelineFader
{
public:
    virtual ~AudioPipelineFader() = default;

    virtual void setFaderCurve(Engine::FadeCurve curve)                             = 0;
    virtual void faderFadeIn(int durationMs, double targetVolume, uint64_t token)   = 0;
    virtual void faderFadeOut(int durationMs, double currentVolume, uint64_t token) = 0;
    virtual void faderStop()                                                        = 0;
    virtual void faderReset()                                                       = 0;
    [[nodiscard]] virtual bool faderIsFading() const                                = 0;
};
} // namespace Fooyin
