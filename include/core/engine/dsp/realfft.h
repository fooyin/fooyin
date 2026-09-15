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

#include <memory>
#include <span>

namespace Fooyin::Dsp {
class FYCORE_EXPORT RealFft
{
public:
    RealFft();
    explicit RealFft(int fftSize);
    ~RealFft();

    RealFft(RealFft&& other) noexcept;
    RealFft& operator=(RealFft&& other) noexcept;
    RealFft(const RealFft&)            = delete;
    RealFft& operator=(const RealFft&) = delete;

    [[nodiscard]] bool reset(int fftSize);
    void clear();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] int fftSize() const;
    [[nodiscard]] int binCount() const;
    //! Return the supported real-transform size closest to fftSize, or 0 for an invalid request.
    [[nodiscard]] static int nearestValidSize(int fftSize);

    // Computes magnitude bins for a real-valued input signal. Output must provide fftSize()/2 + 1 bins.
    [[nodiscard]] bool transformMagnitudes(std::span<const float> input, std::span<float> output) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace Fooyin::Dsp
