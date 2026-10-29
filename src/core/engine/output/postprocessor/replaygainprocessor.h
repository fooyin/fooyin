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

#include "fycore_export.h"

#include "trackprocessor.h"

#include <core/engine/enginedefs.h>
#include <utils/snapshot.h>

namespace Fooyin {
class SettingsManager;

/*!
 * Per-stream ReplayGain processor applied in `AudioMixer` before per-track DSP.
 *
 * Settings are read through a lock-free shared snapshot so UI/settings updates
 * can be consumed on the audio thread without blocking.
 */
class FYCORE_EXPORT ReplayGainProcessor : public TrackProcessor
{
public:
    enum class SelectionMode : uint8_t
    {
        Off = 0,
        Track,
        Album
    };

    struct RuntimeSettings
    {
        SelectionMode mode{SelectionMode::Off};
        Engine::RGProcessing processing{Engine::NoProcessing};
        double rgPreampDb{0.0};
        double nonRgPreampDb{0.0};
    };

    using SharedSettings    = Snapshot<RuntimeSettings>;
    using SharedSettingsPtr = std::shared_ptr<SharedSettings>;

    explicit ReplayGainProcessor(std::shared_ptr<const SharedSettings> settings);

    //! Create shared runtime snapshot storage for engine-wide reuse.
    static SharedSettingsPtr makeSharedSettings();
    //! Refresh shared snapshot from current user settings.
    static void refreshSharedSettings(const SettingsManager& settings, SharedSettings& sharedSettings);

    static Engine::ReplayGainOutputInfo outputInfo(const Track& track, const RuntimeSettings& settings);

    void init(const Track& track, const AudioFormat& format) override;
    [[nodiscard]] bool isActive() const override;
    void process(double* samples, size_t sampleCount) override;
    void endOfTrack() override;
    void reset() override;

private:
    void refreshSettings();
    void updateGain();
    [[nodiscard]] double calculateGain() const;

    std::shared_ptr<const SharedSettings> m_settings;
    uint64_t m_settingsEpoch;

    Track m_track;
    SelectionMode m_mode;
    Engine::RGProcessing m_processing;

    double m_rgPreampDb;
    double m_nonRgPreampDb;

    double m_linearGain;
    bool m_active;
};
} // namespace Fooyin
