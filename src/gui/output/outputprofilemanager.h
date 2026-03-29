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

#include <core/engine/enginecontroller.h>

#include <QObject>

#include <optional>

namespace Fooyin {
class DspChainStore;
class DspPresetRegistry;
class EngineController;
class SettingsManager;

class OutputProfileManager : public QObject
{
    Q_OBJECT

public:
    struct DeviceEntry
    {
        QString output;
        QString device;
        QString description;
        bool enabled{true};
        SampleFormat bitDepth{SampleFormat::Unknown};
        bool dither{false};
        int dspPresetId{-1};
    };

    OutputProfileManager(EngineController* engine, DspChainStore* chainStore, DspPresetRegistry* presetRegistry,
                         SettingsManager* settings, QObject* parent = nullptr);

    [[nodiscard]] OutputNames outputs() const;
    [[nodiscard]] QString currentOutput() const;
    [[nodiscard]] QString currentDevice() const;
    [[nodiscard]] std::vector<DeviceEntry> deviceEntries(const QString& output) const;

    void setProfiles(const QString& output, const Engine::OutputDeviceProfiles& profiles);
    void clearProfiles();
    bool applyProfile(const QString& output, const QString& device);
    void reapplyCurrentProfile();

signals:
    void profilesChanged(const QString& output);
    void currentOutputChanged(const QString& output, const QString& device);

private:
    [[nodiscard]] Engine::OutputDeviceProfiles profiles() const;
    void storeProfiles(const Engine::OutputDeviceProfiles& profiles);
    [[nodiscard]] std::optional<Engine::OutputDeviceProfile> profileFor(const QString& output,
                                                                        const QString& device) const;
    [[nodiscard]] Engine::DspChains resolveChain(int dspPresetId) const;
    [[nodiscard]] std::pair<QString, QString> parseCurrentOutput() const;

    EngineController* m_engine;
    DspChainStore* m_chainStore;
    DspPresetRegistry* m_presetRegistry;
    SettingsManager* m_settings;
};
} // namespace Fooyin
