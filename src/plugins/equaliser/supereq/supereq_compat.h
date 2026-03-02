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

#include <algorithm>
#include <vector>

#ifndef NOVTABLE
#if defined(_MSC_VER)
#define NOVTABLE __declspec(novtable)
#else
#define NOVTABLE
#endif
#endif

using t_size       = size_t;
using audio_sample = double;

namespace pfc {
template <typename T>
class array_t
{
public:
    void set_size(const t_size size)
    {
        m_data.resize(size);
    }

    void grow_size(const t_size size)
    {
        if(m_data.size() < size) {
            m_data.resize(size);
        }
    }

    void fill_null()
    {
        std::fill(m_data.begin(), m_data.end(), T{});
    }

    [[nodiscard]] t_size get_size() const
    {
        return m_data.size();
    }

    T* get_ptr()
    {
        return m_data.empty() ? nullptr : m_data.data();
    }

    [[nodiscard]] const T* get_ptr() const
    {
        return m_data.empty() ? nullptr : m_data.data();
    }

    T& operator[](const t_size index)
    {
        return m_data[index];
    }

    const T& operator[](const t_size index) const
    {
        return m_data[index];
    }

private:
    std::vector<T> m_data;
};
} // namespace pfc
