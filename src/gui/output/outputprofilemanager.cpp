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

#include "outputprofilemanager.h"

#include "dsp/dsppresetregistry.h"

#include <core/coresettings.h>
#include <core/engine/dsp/dspchainstore.h>
#include <core/internalcoresettings.h>
#include <utils/settings/settingsmanager.h>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin {
OutputProfileManager::OutputProfileManager(EngineController* engine, DspChainStore* chainStore,
                                           DspPresetRegistry* presetRegistry, SettingsManager* settings,
                                           QObject* parent)
    : QObject{parent}
    , m_engine{engine}
    , m_chainStore{chainStore}
    , m_presetRegistry{presetRegistry}
    , m_settings{settings}
{
    m_settings->subscribe<Settings::Core::AudioOutput>(this, [this](const QString& value) {
        const QStringList parts = value.split(u'|');
        if(!parts.empty() && parts.size() >= 2) {
            emit currentOutputChanged(parts.value(0), parts.value(1));
            reapplyCurrentProfile();
        }
    });
    m_settings->subscribe<Settings::Core::Internal::OutputDeviceProfiles>(
        this, [this]() { emit profilesChanged(currentOutput()); });
}

OutputNames OutputProfileManager::outputs() const
{
    return m_engine->getAllOutputs();
}

QString OutputProfileManager::currentOutput() const
{
    return parseCurrentOutput().first;
}

QString OutputProfileManager::currentDevice() const
{
    return parseCurrentOutput().second;
}

std::vector<OutputProfileManager::DeviceEntry> OutputProfileManager::deviceEntries(const QString& output) const
{
    if(output.isEmpty()) {
        return {};
    }

    const auto stored  = profiles();
    const auto devices = m_engine->getOutputDevices(output);

    std::vector<DeviceEntry> entries;
    entries.reserve(devices.size());

    for(const auto& [device, description] : devices) {
        DeviceEntry entry{
            .output      = output,
            .device      = device,
            .description = description,
        };

        const auto it = std::ranges::find_if(stored, [&output, &device](const auto& profile) {
            return profile.output == output && profile.device == device;
        });
        if(it != stored.cend()) {
            entry.enabled     = it->enabled;
            entry.bitDepth    = it->bitDepth;
            entry.dither      = it->dither;
            entry.dspPresetId = it->dspPresetId;
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}

void OutputProfileManager::setProfiles(const QString& output, const Engine::OutputDeviceProfiles& profilesForOutput)
{
    auto merged = profiles();
    std::erase_if(merged, [&output](const auto& profile) { return profile.output == output; });

    for(const auto& profile : profilesForOutput) {
        if(profile.isValid()) {
            merged.push_back(profile);
        }
    }

    storeProfiles(merged);
}

void OutputProfileManager::clearProfiles()
{
    m_settings->reset<Settings::Core::Internal::OutputDeviceProfiles>();
    emit profilesChanged(currentOutput());
}

bool OutputProfileManager::applyProfile(const QString& output, const QString& device)
{
    if(output.isEmpty() || device.isEmpty()) {
        return false;
    }

    const auto profile  = profileFor(output, device);
    const auto bitDepth = (profile && profile->bitDepth != SampleFormat::Unknown)
                            ? profile->bitDepth
                            : static_cast<SampleFormat>(m_settings->value<Settings::Core::OutputBitDepth>());
    const bool dither
        = bitDepth == SampleFormat::S16
       && ((profile && profile->bitDepth != SampleFormat::Unknown) ? profile->dither
                                                                   : m_settings->value<Settings::Core::OutputDither>());

    const int dspPresetId         = profile ? profile->dspPresetId : -1;
    const Engine::DspChains chain = resolveChain(dspPresetId);

    m_engine->applyOutputProfile({
        .output   = output,
        .device   = device,
        .bitDepth = bitDepth,
        .dither   = dither,
        .chain    = chain,
    });

    if(m_chainStore) {
        m_chainStore->syncActiveChain(chain);
    }

    const QString setting = output + u"|"_s + device;
    m_settings->setSilently<Settings::Core::AudioOutput>(setting);
    m_settings->setSilently<Settings::Core::OutputBitDepth>(static_cast<int>(bitDepth));
    m_settings->setSilently<Settings::Core::OutputDither>(dither);

    emit currentOutputChanged(output, device);
    return true;
}

void OutputProfileManager::reapplyCurrentProfile()
{
    const auto [output, device] = parseCurrentOutput();
    const auto profile          = profileFor(output, device);
    if(!profile) {
        return;
    }

    if(profile->bitDepth == SampleFormat::Unknown && profile->dspPresetId < 0) {
        return;
    }

    applyProfile(output, device);
}

Engine::OutputDeviceProfiles OutputProfileManager::profiles() const
{
    return m_settings->value<Settings::Core::Internal::OutputDeviceProfiles>().value<Engine::OutputDeviceProfiles>();
}

void OutputProfileManager::storeProfiles(const Engine::OutputDeviceProfiles& profiles)
{
    m_settings->set<Settings::Core::Internal::OutputDeviceProfiles>(QVariant::fromValue(profiles));
}

std::optional<Engine::OutputDeviceProfile> OutputProfileManager::profileFor(const QString& output,
                                                                            const QString& device) const
{
    const auto stored = profiles();

    const auto it = std::ranges::find_if(stored, [&output, &device](const auto& profile) {
        return profile.output == output && profile.device == device;
    });
    if(it != stored.cend()) {
        return *it;
    }

    return {};
}

Engine::DspChains OutputProfileManager::resolveChain(int dspPresetId) const
{
    if(dspPresetId >= 0) {
        if(const auto preset = m_presetRegistry->itemById(dspPresetId)) {
            return preset->chain;
        }
    }

    return m_chainStore->activeChain();
}

std::pair<QString, QString> OutputProfileManager::parseCurrentOutput() const
{
    const QStringList parts = m_settings->value<Settings::Core::AudioOutput>().split(u'|');
    if(!parts.empty() && parts.size() >= 2) {
        return {parts.value(0), parts.value(1)};
    }

    const auto outputs = m_engine->getAllOutputs();
    if(!outputs.empty()) {
        const auto devices = m_engine->getOutputDevices(outputs.front());
        if(!devices.empty()) {
            return {outputs.front(), devices.front().name};
        }
    }

    return {};
}
} // namespace Fooyin

#include "moc_outputprofilemanager.cpp"
