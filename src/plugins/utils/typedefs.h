/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QVariant>

using IdSet = QSet<int>;

namespace Role {
const int Type = Qt::UserRole + 1;
const int PlaylistType = Qt::UserRole + 2;
} // namespace Role

namespace ItemRole {
const int Id = Qt::UserRole + 10;
const int Artist = Qt::UserRole + 11;
const int Year = Qt::UserRole + 12;
const int Duration = Qt::UserRole + 13;
const int Cover = Qt::UserRole + 14;
const int Number = Qt::UserRole + 15;
const int PlayCount = Qt::UserRole + 16;
const int MultiDisk = Qt::UserRole + 17;
const int Playing = Qt::UserRole + 18;
const int State = Qt::UserRole + 19;
const int Path = Qt::UserRole + 20;
const int Index = Qt::UserRole + 21;
const int Data = Qt::UserRole + 22;
} // namespace ItemRole

namespace InfoRole {
// These correspond to row numbers of the infomodel
// Do not change!
const int Title = 1;
const int Artist = 2;
const int Album = 3;
const int Year = 4;
const int Genre = 5;
const int TrackNumber = 6;
const int Filename = 8;
const int Path = 9;
const int Duration = 10;
const int Bitrate = 11;
const int SampleRate = 12;
} // namespace InfoRole

namespace FilterRole {
const int Id = Qt::UserRole + 40;
const int Name = Qt::UserRole + 41;
} // namespace FilterRole

namespace LayoutRole {
const int Type = Qt::UserRole + 50;
} // namespace LayoutRole

namespace Filters {
Q_NAMESPACE
enum class FilterType
{
    Genre = 0,
    Year,
    AlbumArtist,
    Artist,
    Album,
};
Q_ENUM_NS(FilterType)
} // namespace Filters

namespace Widgets {
Q_NAMESPACE
enum class WidgetType
{
    Dummy = 0,
    Filter,
    Playlist,
    Status,
    Info,
    Controls,
    Artwork,
    Search,
    HorizontalSplitter,
    VerticalSplitter,
    Spacer
};
Q_ENUM_NS(WidgetType)
} // namespace Widgets

namespace Player {
Q_NAMESPACE
enum class PlayState
{
    Playing = 0,
    Paused,
    Stopped
};
Q_ENUM_NS(PlayState)
enum class PlayMode
{
    Default = 0,
    RepeatAll,
    Repeat,
    Shuffle
};
Q_ENUM_NS(PlayMode)
} // namespace Player
