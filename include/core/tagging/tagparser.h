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

#include "fycore_export.h"

#include <core/track.h>

namespace Fooyin {

class FYCORE_EXPORT TagParser
{
public:
    struct WriteOptions
    {
        bool writeRating{false};
        bool writePlaycount{false};
    };

    virtual ~TagParser() = default;

    [[nodiscard]] virtual QStringList supportedExtensions() const = 0;
    [[nodiscard]] virtual bool canReadCover() const               = 0;
    [[nodiscard]] virtual bool canWriteMetaData() const           = 0;

    [[nodiscard]] virtual bool readMetaData(Track& track) const = 0;
    [[nodiscard]] virtual QByteArray readCover(const Track& track, Track::Cover cover) const;
    [[nodiscard]] virtual bool writeMetaData(const Track& track, const WriteOptions& options) const;
};
} // namespace Fooyin
