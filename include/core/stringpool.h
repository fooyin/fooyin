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

#include <QStringList>

namespace Fooyin {
class StringPoolPrivate;

/*!
 * Shared vocabulary for metadata strings.
 *
 * `StringPool` stores one canonical copy of repeated strings per metadata
 * domain and lets callers keep compact ids or packed list references instead
 * of resident `QString`/`QStringList` copies.
 *
 * The pool is intentionally append-only for the current session. This keeps
 * lookups cheap and makes the same vocabulary reusable by a higher-level owner
 * such as a library metadata store.
 *
 * Thread safety:
 * All public methods are internally synchronized. Returned ids and list refs
 * remain valid for the process lifetime.
 */
class FYCORE_EXPORT StringPool
{
public:
    using StringId = uint32_t;

    static constexpr StringId EmptyStringId = 0;

    /*!
     * Slice into the packed id storage for a multivalue field.
     */
    struct StringListRef
    {
        uint32_t offset{0};
        uint32_t size{0};

        [[nodiscard]] bool isEmpty() const
        {
            return size == 0;
        }
    };

    /*!
     * Independent vocabularies for fields that should not share ids.
     */
    enum class Domain : uint8_t
    {
        Artist = 0,
        Album,
        AlbumArtist,
        Genre,
        Codec,
        Encoding,
        ExtraTagKey,
    };

    StringPool();
    ~StringPool();

    //! Interns a value and returns the canonical string for the domain.
    [[nodiscard]] QString intern(Domain domain, const QString& value);
    //! Interns a value and returns its stable process-local id.
    [[nodiscard]] StringId internId(Domain domain, const QString& value);
    //! Packs a multivalue field into the domain's list arena.
    [[nodiscard]] StringListRef internList(Domain domain, const QStringList& values);

    //! Resolves a single id back to a string.
    [[nodiscard]] QString resolve(Domain domain, StringId id) const;
    //! Resolves a packed list into a materialized `QStringList`.
    [[nodiscard]] QStringList resolveList(Domain domain, StringListRef ref) const;
    //! Joins a packed list without exposing the caller to the backing storage.
    [[nodiscard]] QString joined(Domain domain, StringListRef ref, const QString& separator) const;

    //! Random access into a packed list.
    [[nodiscard]] QString valueAt(Domain domain, StringListRef ref, qsizetype index) const;
    //! Membership check without resolving the full list.
    [[nodiscard]] bool contains(Domain domain, StringListRef ref, const QString& value) const;

    //! Sorted snapshot of all known values in a domain.
    [[nodiscard]] QStringList values(Domain domain) const;

private:
    std::unique_ptr<StringPoolPrivate> p;
};
} // namespace Fooyin
