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

#include <core/engine/dsp/realfft.h>

#include <pffft.h>

#include <algorithm>
#include <cmath>

namespace Fooyin::Dsp {
class RealFft::Impl
{
public:
    ~Impl()
    {
        reset();
    }

    bool initialise(int fftSize)
    {
        reset();

        if(fftSize < 2 || pffft_is_valid_size(fftSize, PFFFT_REAL) == 0) {
            return false;
        }

        const size_t bufferSize = sizeof(float) * static_cast<size_t>(fftSize);

        m_input = static_cast<float*>(pffft_aligned_malloc(bufferSize));
        if(m_input == nullptr) {
            return false;
        }

        m_output = static_cast<float*>(pffft_aligned_malloc(bufferSize));
        if(m_output == nullptr) {
            reset();
            return false;
        }

        m_work = static_cast<float*>(pffft_aligned_malloc(bufferSize));
        if(m_work == nullptr) {
            reset();
            return false;
        }

        m_setup = pffft_new_setup(fftSize, PFFFT_REAL);
        if(m_setup == nullptr) {
            reset();
            return false;
        }

        m_fftSize = fftSize;
        return true;
    }

    void reset()
    {
        if(m_setup != nullptr) {
            pffft_destroy_setup(m_setup);
            m_setup = nullptr;
        }

        if(m_output != nullptr) {
            pffft_aligned_free(m_output);
            m_output = nullptr;
        }

        if(m_input != nullptr) {
            pffft_aligned_free(m_input);
            m_input = nullptr;
        }

        if(m_work != nullptr) {
            pffft_aligned_free(m_work);
            m_work = nullptr;
        }

        m_fftSize = 0;
    }

    [[nodiscard]] bool isValid() const
    {
        return m_setup != nullptr && m_input != nullptr && m_output != nullptr && m_work != nullptr && m_fftSize > 0;
    }

    [[nodiscard]] int binCount() const
    {
        return (m_fftSize / 2) + 1;
    }

    float* m_input{nullptr};
    float* m_output{nullptr};
    float* m_work{nullptr};
    PFFFT_Setup* m_setup{nullptr};
    int m_fftSize{0};
};

RealFft::RealFft()
    : m_impl{std::make_unique<Impl>()}
{ }

RealFft::RealFft(int fftSize)
    : RealFft{}
{
    (void)reset(fftSize);
}

RealFft::~RealFft() = default;

RealFft::RealFft(RealFft&& other) noexcept = default;

RealFft& RealFft::operator=(RealFft&& other) noexcept = default;

bool RealFft::reset(int fftSize)
{
    return m_impl->initialise(fftSize);
}

void RealFft::clear()
{
    m_impl->reset();
}

bool RealFft::isValid() const
{
    return m_impl->isValid();
}

int RealFft::fftSize() const
{
    return m_impl->m_fftSize;
}

int RealFft::binCount() const
{
    return m_impl->binCount();
}

int RealFft::nearestValidSize(int fftSize)
{
    if(fftSize < 2) {
        return 0;
    }

    if(pffft_is_valid_size(fftSize, PFFFT_REAL) != 0) {
        return fftSize;
    }

    const int lower = pffft_nearest_transform_size(fftSize, PFFFT_REAL, 0);
    const int upper = pffft_nearest_transform_size(fftSize, PFFFT_REAL, 1);

    if(lower <= 0) {
        return std::max(0, upper);
    }
    if(upper <= 0) {
        return lower;
    }

    return (fftSize - lower) <= (upper - fftSize) ? lower : upper;
}

bool RealFft::transformMagnitudes(std::span<const float> input, std::span<float> output) const
{
    const int fftSize = this->fftSize();

    if(!isValid() || input.size() < static_cast<size_t>(fftSize) || output.size() < static_cast<size_t>(binCount())) {
        return false;
    }

    std::copy_n(input.data(), fftSize, m_impl->m_input);
    pffft_transform_ordered(m_impl->m_setup, m_impl->m_input, m_impl->m_output, m_impl->m_work, PFFFT_FORWARD);

    const float edgeScale = 1.0F / static_cast<float>(fftSize);
    const float bandScale = 2.0F / static_cast<float>(fftSize);
    output[0]             = std::abs(m_impl->m_output[0]) * edgeScale;

    for(int bin{1}; bin < binCount() - 1; ++bin) {
        const size_t offset              = static_cast<size_t>(bin) * 2;
        const float real                 = m_impl->m_output[offset];
        const float imag                 = m_impl->m_output[offset + 1];
        output[static_cast<size_t>(bin)] = std::hypot(real, imag) * bandScale;
    }

    if(binCount() > 1) {
        output[static_cast<size_t>(binCount() - 1)] = std::abs(m_impl->m_output[1]) * edgeScale;
    }

    return true;
}
} // namespace Fooyin::Dsp
