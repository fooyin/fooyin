/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <core/stringpool.h>

namespace Fooyin {
/*!
 * Explicit owner for shared track metadata vocabularies.
 *
 * A library can keep one `TrackMetadataStore` and let all resident `Track`
 * instances intern repeated strings through it. Standalone tracks may also own
 * their own store when no library is involved.
 */
class FYCORE_EXPORT TrackMetadataStore
{
public:
    [[nodiscard]] StringPool& stringPool()
    {
        return m_stringPool;
    }

    [[nodiscard]] const StringPool& stringPool() const
    {
        return m_stringPool;
    }

    [[nodiscard]] QStringList values(StringPool::Domain domain) const
    {
        return m_stringPool.values(domain);
    }

private:
    StringPool m_stringPool;
};
} // namespace Fooyin
