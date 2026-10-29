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
    , m_mode{SelectionMode::Off}
    , m_processing{Engine::NoProcessing}
    , m_rgPreampDb{0.0}
    , m_nonRgPreampDb{0.0}
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

void ReplayGainProcessor::init(const Track& track, const AudioFormat& /*format*/)
{
    m_track = track;

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
        if(m_mode != SelectionMode::Off || m_processing != Engine::NoProcessing || m_rgPreampDb != 0.0
           || m_nonRgPreampDb != 0.0) {
            m_mode          = SelectionMode::Off;
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

    const auto mode            = snapshot->value.mode;
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

Engine::ReplayGainOutputInfo ReplayGainProcessor::outputInfo(const Track& track, const RuntimeSettings& settings)
{
    Engine::ReplayGainOutputInfo info;
    info.processing = settings.processing;
    info.source     = settings.mode == SelectionMode::Track ? Engine::ReplayGainSource::Track
                    : settings.mode == SelectionMode::Album ? Engine::ReplayGainSource::Album
                                                            : Engine::ReplayGainSource::None;

    if(settings.mode == SelectionMode::Off || settings.processing == Engine::NoProcessing) {
        return info;
    }

    const bool preferTrack = settings.mode == SelectionMode::Track;
    double sourceGainDb{0.0};
    double processingPeak{1.0};
    bool hasProcessingPeak{false};
    bool hasGain{false};

    if(track.hasTrackPeak()) {
        info.peak    = track.rgTrackPeak();
        info.hasPeak = true;
    }
    else if(track.hasAlbumPeak()) {
        info.peak    = track.rgAlbumPeak();
        info.hasPeak = true;
    }

    if(preferTrack) {
        if(track.hasTrackGain()) {
            sourceGainDb = track.rgTrackGain();
            hasGain      = true;
        }
        else if(track.hasAlbumGain()) {
            sourceGainDb = track.rgAlbumGain();
            hasGain      = true;
        }

        if(track.hasTrackPeak()) {
            processingPeak    = track.rgTrackPeak();
            hasProcessingPeak = true;
        }
        else if(track.hasAlbumPeak()) {
            processingPeak    = track.rgAlbumPeak();
            hasProcessingPeak = true;
        }
    }
    else {
        if(track.hasAlbumGain()) {
            sourceGainDb = track.rgAlbumGain();
            hasGain      = true;
        }
        else if(track.hasTrackGain()) {
            sourceGainDb = track.rgTrackGain();
            hasGain      = true;
        }

        if(track.hasAlbumPeak()) {
            processingPeak    = track.rgAlbumPeak();
            hasProcessingPeak = true;
        }
        else if(track.hasTrackPeak()) {
            processingPeak    = track.rgTrackPeak();
            hasProcessingPeak = true;
        }
    }

    const bool applyGain       = settings.processing.testFlag(Engine::ApplyGain);
    const bool preventClipping = settings.processing.testFlag(Engine::PreventClipping);

    info.gainDb       = applyGain ? sourceGainDb + (hasGain ? settings.rgPreampDb : settings.nonRgPreampDb) : 0.0;
    double linearGain = std::pow(10.0, info.gainDb / 20.0);

    if(preventClipping && hasProcessingPeak && processingPeak > 0.0 && linearGain * processingPeak > 1.0) {
        linearGain  = 1.0 / processingPeak;
        info.gainDb = 20.0 * std::log10(linearGain);
    }

    if(info.hasPeak) {
        info.peak *= linearGain;
    }

    return info;
}

void ReplayGainProcessor::updateGain()
{
    constexpr double ActiveGainEpsilon = std::numeric_limits<double>::epsilon() * 8.0;
    m_linearGain                       = calculateGain();
    m_active                           = std::abs(m_linearGain - 1.0) > ActiveGainEpsilon;
}

double ReplayGainProcessor::calculateGain() const
{
    const auto info = outputInfo(m_track, RuntimeSettings{
                                              .mode          = m_mode,
                                              .processing    = m_processing,
                                              .rgPreampDb    = m_rgPreampDb,
                                              .nonRgPreampDb = m_nonRgPreampDb,
                                          });
    return std::pow(10.0, info.gainDb / 20.0);
}
} // namespace Fooyin
