/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <set>

#include <QVariant>

namespace Core {
using IdSet = std::set<int>;

namespace Role {
const int Type         = Qt::UserRole + 1;
const int PlaylistType = Qt::UserRole + 2;
} // namespace Role

namespace InfoRole {
// These correspond to row numbers of the infomodel
// Do not change!
const int Title       = 1;
const int Artist      = 2;
const int Album       = 3;
const int Year        = 4;
const int Genre       = 5;
const int TrackNumber = 6;
const int Filename    = 8;
const int Path        = 9;
const int Duration    = 10;
const int Bitrate     = 11;
const int SampleRate  = 12;
} // namespace InfoRole

namespace Fy {
Q_NAMESPACE
enum Attribute
{
    HasActiveIcon = 1,
    AutoShift     = 2,
    Active        = 4,
};
Q_DECLARE_FLAGS(Attributes, Attribute)
} // namespace Fy
Q_DECLARE_OPERATORS_FOR_FLAGS(Fy::Attributes)

namespace Player {
Q_NAMESPACE
enum PlayState : uint8_t
{
    Playing = 1,
    Paused  = 2,
    Stopped = 3,
};
Q_ENUM_NS(PlayState)
enum PlayMode : uint8_t
{
    Default   = 1,
    RepeatAll = 2,
    Repeat    = 3,
    Shuffle   = 4,
};
Q_ENUM_NS(PlayMode)
} // namespace Player
} // namespace Core
