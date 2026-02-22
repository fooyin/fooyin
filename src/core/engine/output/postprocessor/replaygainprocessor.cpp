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

#include "replaygainprocessor.h"

#include "internalcoresettings.h"

#include <core/coresettings.h>
#include <core/playlist/playlist.h>
#include <utils/settings/settingsmanager.h>

#include <limits>

namespace Fooyin {
ReplayGainProcessor::ReplayGainProcessor(std::shared_ptr<const SharedSettings> settings)
    : m_settings{std::move(settings)}
    , m_settingsEpoch{0}
    , m_mode{Mode::Off}
    , m_processing{Engine::NoProcessing}
    , m_rgPreampDb{0.0}
    , m_nonRgPreampDb{0.0}
    , m_trackGainDb{0.0}
    , m_albumGainDb{0.0}
    , m_trackPeak{1.0}
    , m_albumPeak{1.0}
    , m_haveTrackGain{false}
    , m_haveAlbumGain{false}
    , m_haveTrackPeak{false}
    , m_haveAlbumPeak{false}
    , m_linearGain{1.0}
    , m_active{false}
{ }

ReplayGainProcessor::SharedSettingsPtr ReplayGainProcessor::makeSharedSettings()
{
    return std::make_shared<SharedSettings>();
}

void ReplayGainProcessor::refreshSharedSettings(const SettingsManager& settings, SharedSettings& sharedSettings)
{
    sharedSettings.update([&settings](RuntimeSettings& runtimeSettings) {
        runtimeSettings.processing    = static_cast<Engine::RGProcessing>(settings.value<Settings::Core::RGMode>());
        auto gainType                 = static_cast<ReplayGainType>(settings.value<Settings::Core::RGType>());
        runtimeSettings.rgPreampDb    = static_cast<double>(settings.value<Settings::Core::RGPreAmp>());
        runtimeSettings.nonRgPreampDb = static_cast<double>(settings.value<Settings::Core::NonRGPreAmp>());

        if(gainType == ReplayGainType::PlaybackOrder) {
            const auto playMode = settings.value<Settings::Core::PlayMode>();
            gainType = (playMode & Playlist::ShuffleTracks) != 0 ? ReplayGainType::Track : ReplayGainType::Album;
        }

        runtimeSettings.mode = SelectionMode::Off;
        if(runtimeSettings.processing != Engine::NoProcessing) {
            runtimeSettings.mode = gainType == ReplayGainType::Track ? SelectionMode::Track : SelectionMode::Album;
        }
    });
}

void ReplayGainProcessor::init(const Track& track, const AudioFormat& format)
{
    m_format = format;

    m_haveTrackGain = track.hasTrackGain();
    m_haveAlbumGain = track.hasAlbumGain();
    m_haveTrackPeak = track.hasTrackPeak();
    m_haveAlbumPeak = track.hasAlbumPeak();

    m_trackGainDb = m_haveTrackGain ? static_cast<double>(track.rgTrackGain()) : 0.0;
    m_albumGainDb = m_haveAlbumGain ? static_cast<double>(track.rgAlbumGain()) : 0.0;
    m_trackPeak   = m_haveTrackPeak ? static_cast<double>(track.rgTrackPeak()) : 1.0;
    m_albumPeak   = m_haveAlbumPeak ? static_cast<double>(track.rgAlbumPeak()) : 1.0;

    refreshSettings();
    updateGain();
}

bool ReplayGainProcessor::isActive() const
{
    return m_active;
}

void ReplayGainProcessor::process(double* samples, size_t sampleCount)
{
    if(!samples || sampleCount == 0) {
        return;
    }

    refreshSettings();

    if(!m_active) {
        return;
    }

    for(size_t i{0}; i < sampleCount; ++i) {
        samples[i] *= m_linearGain;
    }
}

void ReplayGainProcessor::endOfTrack() { }

void ReplayGainProcessor::reset() { }

void ReplayGainProcessor::refreshSettings()
{
    if(!m_settings) {
        if(m_mode != Mode::Off || m_processing != Engine::NoProcessing || m_rgPreampDb != 0.0
           || m_nonRgPreampDb != 0.0) {
            m_mode          = Mode::Off;
            m_processing    = Engine::NoProcessing;
            m_rgPreampDb    = 0.0;
            m_nonRgPreampDb = 0.0;
            updateGain();
        }
        return;
    }

    const auto snapshot = m_settings->load();
    if(!snapshot || snapshot->epoch == m_settingsEpoch) {
        return;
    }
    m_settingsEpoch = snapshot->epoch;

    const auto mode = [selectionMode = snapshot->value.mode]() {
        switch(selectionMode) {
            case(SelectionMode::Track):
                return Mode::Track;
            case(SelectionMode::Album):
                return Mode::Album;
            case(SelectionMode::Off):
            default:
                return Mode::Off;
        }
    }();

    const auto processing      = snapshot->value.processing;
    const double rgPreampDb    = snapshot->value.rgPreampDb;
    const double nonRgPreampDb = snapshot->value.nonRgPreampDb;

    if(mode == m_mode && processing == m_processing && rgPreampDb == m_rgPreampDb && nonRgPreampDb == m_nonRgPreampDb) {
        return;
    }

    m_mode          = mode;
    m_processing    = processing;
    m_rgPreampDb    = rgPreampDb;
    m_nonRgPreampDb = nonRgPreampDb;

    updateGain();
}

ReplayGainProcessor::ReplayGainValues ReplayGainProcessor::extractValues(bool preferTrack) const
{
    ReplayGainValues values;

    if(preferTrack) {
        if(m_haveTrackGain) {
            values.gainDb   = m_trackGainDb;
            values.haveGain = true;
        }
        else if(m_haveAlbumGain) {
            values.gainDb   = m_albumGainDb;
            values.haveGain = true;
        }

        if(m_haveTrackPeak) {
            values.peak     = m_trackPeak;
            values.havePeak = true;
        }
        else if(m_haveAlbumPeak) {
            values.peak     = m_albumPeak;
            values.havePeak = true;
        }
    }
    else {
        if(m_haveAlbumGain) {
            values.gainDb   = m_albumGainDb;
            values.haveGain = true;
        }
        else if(m_haveTrackGain) {
            values.gainDb   = m_trackGainDb;
            values.haveGain = true;
        }

        if(m_haveAlbumPeak) {
            values.peak     = m_albumPeak;
            values.havePeak = true;
        }
        else if(m_haveTrackPeak) {
            values.peak     = m_trackPeak;
            values.havePeak = true;
        }
    }

    return values;
}

void ReplayGainProcessor::updateGain()
{
    constexpr double ActiveGainEpsilon = std::numeric_limits<double>::epsilon() * 8.0;
    m_linearGain                       = calculateGain();
    m_active                           = std::abs(m_linearGain - 1.0) > ActiveGainEpsilon;
}

double ReplayGainProcessor::calculateGain() const
{
    if(m_mode == Mode::Off || m_processing == Engine::NoProcessing) {
        return 1.0;
    }

    const bool applyGain       = m_processing.testFlag(Engine::ApplyGain);
    const bool preventClipping = m_processing.testFlag(Engine::PreventClipping);

    const bool preferTrack = (m_mode == Mode::Track);
    const auto selected    = extractValues(preferTrack);

    double gainDb         = applyGain ? selected.gainDb : 0.0;
    const double preampDb = applyGain ? (selected.haveGain ? m_rgPreampDb : m_nonRgPreampDb) : 0.0;
    gainDb += preampDb;

    double gain = std::pow(10.0, gainDb / 20.0);

    if(preventClipping) {
        const double peak = selected.havePeak ? selected.peak : 1.0;
        if(peak > 0.0 && gain * peak > 1.0) {
            gain = 1.0 / peak;
        }
    }

    return gain;
}
} // namespace Fooyin
