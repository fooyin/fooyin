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

#include <utils/settings/settingsentry.h>

#include <QNetworkProxy>
#include <QObject>
#include <QSettings>

namespace Fooyin {
class FYCORE_EXPORT FySettings : public QSettings
{
public:
    explicit FySettings(QObject* parent = nullptr);
};

class FYCORE_EXPORT FyStateSettings : public QSettings
{
public:
    explicit FyStateSettings(QObject* parent = nullptr);
};

namespace Settings::Core {
Q_NAMESPACE_EXPORT(FYCORE_EXPORT)

enum CoreSettings : uint32_t
{
    Version                   = 1 | Type::String,
    FirstRun                  = 2 | Type::Bool,
    PlayMode                  = 3 | Type::Int,
    AutoRefresh               = 4 | Type::Bool,
    LibrarySortScript         = 5 | Type::String,
    AudioOutput               = 6 | Type::String,
    OutputVolume              = 7 | Type::Double,
    RewindPreviousTrack       = 8 | Type::Bool,
    GaplessPlayback           = 9 | Type::Bool,
    Language                  = 10 | Type::String,
    BufferLength              = 11 | Type::Int,
    OpenFilesPlaylist         = 12 | Type::String,
    OpenFilesSendTo           = 13 | Type::Bool,
    SaveRatingToMetadata      = 14 | Type::Bool,
    SavePlaycountToMetadata   = 15 | Type::Bool,
    PlayedThreshold           = 16 | Type::Double,
    ExternalSortScript        = 17 | Type::String,
    Shutdown                  = 18 | Type::Bool,
    StopAfterCurrent          = 19 | Type::Bool,
    RGMode                    = 20 | Type::Int,
    RGType                    = 21 | Type::Int,
    RGPreAmp                  = 22 | Type::Float,
    NonRGPreAmp               = 23 | Type::Float,
    ProxyMode                 = 24 | Type::Int,
    ProxyConfig               = 25 | Type::Variant,
    UseVariousForCompilations = 26 | Type::Bool,
    ShuffleAlbumsGroupScript  = 27 | Type::String,
    ShuffleAlbumsSortScript   = 28 | Type::String,
};
Q_ENUM_NS(CoreSettings)
} // namespace Settings::Core
} // namespace Fooyin
