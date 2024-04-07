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

#include <core/track.h>

#include <taglib/audioproperties.h>

#include <QString>

class QPixmap;

namespace Fooyin::Tagging {
enum class Quality : uint8_t
{
    Fast = 0,
    Average,
    Accurate,
};

FYCORE_EXPORT bool readMetaData(Track& track, Quality quality = Quality::Average);
FYCORE_EXPORT QByteArray readCover(const Track& track, Track::Cover cover = Track::Cover::Front);
} // namespace Fooyin::Tagging
