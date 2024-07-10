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

#pragma once

#include "fycore_export.h"

#include <core/tagging/tagparser.h>

namespace Fooyin {
class FYCORE_EXPORT TagLibParser : public TagParser
{
public:
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;

    [[nodiscard]] bool readMetaData(Track& track) const override;
    [[nodiscard]] QByteArray readCover(const Track& track, Track::Cover cover) const override;
    [[nodiscard]] bool writeMetaData(const Track& track, const WriteOptions& options) const override;
};
} // namespace Fooyin
