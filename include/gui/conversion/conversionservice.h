/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fygui_export.h"

#include <core/engine/conversion/conversionrunner.h>
#include <core/track.h>

#include <QString>

#include <functional>
#include <memory>
#include <vector>

namespace Fooyin {
struct FYGUI_EXPORT ConversionPresetInfo
{
    QString id;
    QString name;
};

class FYGUI_EXPORT ConversionService
{
public:
    virtual ~ConversionService() = default;

    virtual void showSetup(const TrackList& tracks)                                                   = 0;
    virtual void showSetup(const TrackList& tracks, const QString& suggestedFilenamePattern,
                           std::shared_ptr<ConversionInputObserver> sourceObserver,
                           std::function<void(const std::vector<ConversionTrackResult>&)> completion) = 0;

    [[nodiscard]] virtual std::vector<ConversionPresetInfo> presets() const                             = 0;
    virtual bool startPreset(const QString& presetId, const TrackList& tracks)                          = 0;
    virtual bool startPreset(const QString& presetId, const TrackList& tracks, const QString& suggestedFilenamePattern,
                             std::shared_ptr<ConversionInputObserver> sourceObserver,
                             std::function<void(const std::vector<ConversionTrackResult>&)> completion) = 0;
};
} // namespace Fooyin
