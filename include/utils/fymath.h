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

#include <cmath>
#include <cstdlib>

namespace Fooyin::Math {
#if(defined(__GNUC__) && defined(__x86_64__))
#include <emmintrin.h>
inline int32_t fltToInt(float flt)
{
    return _mm_cvtss_si32(_mm_load_ss(&flt));
}

inline int32_t fltToInt(double flt)
{
    return _mm_cvtsd_si32(_mm_load_sd(&flt));
}
#else
inline int32_t fltToInt(float flt)
{
    return static_cast<int32_t>(std::floor(flt + 0.5));
}

inline int32_t fltToInt(double flt)
{
    return static_cast<int32_t>(std::floor(flt + 0.5));
}
#endif
} // namespace Fooyin::Math
