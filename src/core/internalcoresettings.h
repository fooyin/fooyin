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

#include <utils/settings/settingsentry.h>

namespace Fooyin {
class SettingsManager;

struct FadingIntervals
{
    int inPauseStop{100};
    int outPauseStop{100};
    int inSeek{100};
    int outSeek{100};
    int inChange{100};
    int outChange{100};

    friend QDataStream& operator<<(QDataStream& stream, const FadingIntervals& fading)
    {
        stream << fading.inPauseStop;
        stream << fading.outPauseStop;
        stream << fading.inSeek;
        stream << fading.outSeek;
        stream << fading.inChange;
        stream << fading.outChange;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream& stream, FadingIntervals& fading)
    {
        stream >> fading.inPauseStop;
        stream >> fading.outPauseStop;
        stream >> fading.inSeek;
        stream >> fading.outSeek;
        stream >> fading.inChange;
        stream >> fading.outChange;
        return stream;
    }
};

namespace Settings::Core::Internal {
Q_NAMESPACE_EXPORT(FYCORE_EXPORT)

enum CoreInternalSettings : uint32_t
{
    MonitorLibraries       = 0 | Settings::Bool,
    MuteVolume             = 1 | Settings::Double,
    DisabledPlugins        = 2 | Settings::StringList,
    SavePlaybackState      = 3 | Settings::Bool,
    EngineFading           = 4 | Type::Bool,
    FadingIntervals        = 5 | Type::Variant,
    MarkUnavailable        = 6 | Type::Bool,
    MarkUnavailableStartup = 7 | Type::Bool,
};
Q_ENUM_NS(CoreInternalSettings)
} // namespace Settings::Core::Internal

class CoreSettings
{
public:
    explicit CoreSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::FadingIntervals)
