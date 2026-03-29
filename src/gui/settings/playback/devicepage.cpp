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

#include "devicepage.h"

#include "dsp/dsppresetregistry.h"
#include "output/outputprofilemanager.h"
#include "outputdevicesdelegate.h"
#include "outputdevicesmodel.h"

#include <gui/guiconstants.h>
#include <gui/widgets/expandingcombobox.h>
#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTableView>

#include <algorithm>
#include <map>
#include <optional>

using namespace Qt::StringLiterals;

namespace {
std::optional<Fooyin::Engine::OutputDeviceProfile>
profileForDevice(const Fooyin::Engine::OutputDeviceProfiles& profiles, const QString& device)
{
    const auto it = std::ranges::find(profiles, device, &Fooyin::Engine::OutputDeviceProfile::device);
    if(it != profiles.cend()) {
        return *it;
    }

    return {};
}
} // namespace

namespace Fooyin {
class DevicePageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    DevicePageWidget(OutputProfileManager* profileManager, DspPresetRegistry* presetRegistry);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void storeCurrentOutput();
    void loadOutput(const QString& output);
    void updateColumnWidths();

    OutputProfileManager* m_profileManager;
    ExpandingComboBox* m_outputBox;
    QTableView* m_devices;
    OutputDevicesModel* m_model;

    std::map<QString, Engine::OutputDeviceProfiles> m_profilesByOutput;
    std::map<QString, Engine::OutputDeviceProfiles> m_loadedProfilesByOutput;
};

DevicePageWidget::DevicePageWidget(OutputProfileManager* profileManager, DspPresetRegistry* presetRegistry)
    : m_profileManager{profileManager}
    , m_outputBox{new ExpandingComboBox(this)}
    , m_devices{new QTableView(this)}
    , m_model{new OutputDevicesModel(presetRegistry, this)}
{
    m_devices->setModel(m_model);
    m_devices->setItemDelegateForColumn(OutputDevicesModel::DspColumn,
                                        new DspPresetDelegate(presetRegistry, m_devices));
    m_devices->setItemDelegateForColumn(OutputDevicesModel::BitsColumn, new BitdepthDelegate(m_devices));
    m_devices->horizontalHeader()->setStretchLastSection(false);
    m_devices->horizontalHeader()->setSectionResizeMode(OutputDevicesModel::DeviceColumn, QHeaderView::Stretch);
    m_devices->horizontalHeader()->setSectionResizeMode(OutputDevicesModel::DspColumn, QHeaderView::Interactive);
    m_devices->horizontalHeader()->setSectionResizeMode(OutputDevicesModel::BitsColumn, QHeaderView::Interactive);
    m_devices->verticalHeader()->hide();

    auto* hint
        = new QLabel(u"🛈 "_s + tr("Only checked devices will appear in the output device selector widget."), this);
    hint->setWordWrap(true);

    auto* layout = new QGridLayout(this);

    int row{0};
    layout->addWidget(new QLabel(tr("Output") + u":"_s, this), row, 0);
    layout->addWidget(m_outputBox, row++, 1);
    layout->addWidget(m_devices, row++, 0, 1, 2);
    layout->addWidget(hint, row++, 0, 1, 2);
    layout->setColumnStretch(1, 1);
    layout->setRowStretch(1, 1);

    QObject::connect(m_outputBox, &QComboBox::currentTextChanged, this, [this](const QString& output) {
        storeCurrentOutput();
        loadOutput(output);
    });

    updateColumnWidths();
}

void DevicePageWidget::load()
{
    m_profilesByOutput.clear();
    m_loadedProfilesByOutput.clear();
    m_model->setEntries({});

    const QSignalBlocker blocker{m_outputBox};
    m_outputBox->clear();

    const auto outputs = m_profileManager->outputs();
    for(const auto& output : outputs) {
        m_outputBox->addItem(output);

        const auto entries = m_profileManager->deviceEntries(output);

        Engine::OutputDeviceProfiles profiles;
        profiles.reserve(entries.size());

        for(const auto& entry : entries) {
            profiles.push_back({
                .output      = output,
                .device      = entry.device,
                .enabled     = entry.enabled,
                .bitDepth    = entry.bitDepth,
                .dither      = entry.dither,
                .dspPresetId = entry.dspPresetId,
            });
        }

        m_loadedProfilesByOutput.emplace(output, profiles);
        m_profilesByOutput.emplace(output, std::move(profiles));
    }

    if(m_outputBox->count() <= 0) {
        return;
    }

    int currentIndex = m_outputBox->findText(m_profileManager->currentOutput());
    currentIndex     = std::max(currentIndex, 0);

    m_outputBox->setCurrentIndex(currentIndex);
    loadOutput(m_outputBox->currentText());
}

void DevicePageWidget::apply()
{
    storeCurrentOutput();

    const QString currentOutput = m_profileManager->currentOutput();
    const QString currentDevice = m_profileManager->currentDevice();

    const auto loadedCurrentProfiles = m_loadedProfilesByOutput.find(currentOutput);
    const auto currentProfiles       = m_profilesByOutput.find(currentOutput);

    const auto loadedCurrentProfile = (loadedCurrentProfiles != m_loadedProfilesByOutput.cend())
                                        ? profileForDevice(loadedCurrentProfiles->second, currentDevice)
                                        : std::optional<Engine::OutputDeviceProfile>{};
    const auto currentProfile       = (currentProfiles != m_profilesByOutput.cend())
                                        ? profileForDevice(currentProfiles->second, currentDevice)
                                        : std::optional<Engine::OutputDeviceProfile>{};

    const auto oldBitDepth = loadedCurrentProfile ? loadedCurrentProfile->bitDepth : SampleFormat::Unknown;
    const auto newBitDepth = currentProfile ? currentProfile->bitDepth : SampleFormat::Unknown;
    const bool oldDither   = loadedCurrentProfile ? loadedCurrentProfile->dither : false;
    const bool newDither   = currentProfile ? currentProfile->dither : false;
    const int oldPreset    = loadedCurrentProfile ? loadedCurrentProfile->dspPresetId : -1;
    const int newPreset    = currentProfile ? currentProfile->dspPresetId : -1;

    const bool audioSettingsChanged = oldBitDepth != newBitDepth || oldDither != newDither || oldPreset != newPreset;

    bool profilesChanged{false};
    for(const auto& [output, profiles] : m_profilesByOutput) {
        const auto loadedIt = m_loadedProfilesByOutput.find(output);
        const auto loadedProfiles
            = (loadedIt != m_loadedProfilesByOutput.cend()) ? loadedIt->second : Engine::OutputDeviceProfiles{};

        if(profiles == loadedProfiles) {
            continue;
        }

        m_profileManager->setProfiles(output, profiles);
        profilesChanged = true;
    }

    if(profilesChanged) {
        m_loadedProfilesByOutput = m_profilesByOutput;
    }

    if(audioSettingsChanged) {
        m_profileManager->reapplyCurrentProfile();
    }
}

void DevicePageWidget::reset()
{
    m_profileManager->clearProfiles();
    load();
}

void DevicePageWidget::storeCurrentOutput()
{
    const QString output = m_outputBox->currentText();
    if(output.isEmpty()) {
        return;
    }

    m_profilesByOutput[output] = m_model->profiles(output);
}

void DevicePageWidget::loadOutput(const QString& output)
{
    if(output.isEmpty()) {
        m_model->setEntries({});
        return;
    }

    auto entries = m_profileManager->deviceEntries(output);
    if(const auto it = m_profilesByOutput.find(output); it != m_profilesByOutput.end()) {
        std::map<QString, Engine::OutputDeviceProfile> overrides;
        for(const auto& profile : it->second) {
            overrides.emplace(profile.device, profile);
        }

        for(auto& entry : entries) {
            if(const auto overrideIt = overrides.find(entry.device); overrideIt != overrides.end()) {
                const auto& profile = overrideIt->second;
                entry.enabled       = profile.enabled;
                entry.bitDepth      = profile.bitDepth;
                entry.dither        = profile.dither;
                entry.dspPresetId   = profile.dspPresetId;
            }
        }
    }

    m_model->setEntries(entries);
    updateColumnWidths();
}

void DevicePageWidget::updateColumnWidths()
{
    m_devices->resizeColumnToContents(OutputDevicesModel::DspColumn);
    m_devices->resizeColumnToContents(OutputDevicesModel::BitsColumn);
}

DevicePage::DevicePage(OutputProfileManager* profileManager, DspPresetRegistry* presetRegistry,
                       SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Id{Constants::Page::OutputDevices});
    setName(tr("Devices"));
    setCategory({tr("Playback"), tr("Output"), tr("Devices")});
    setWidgetCreator([profileManager, presetRegistry] { return new DevicePageWidget(profileManager, presetRegistry); });
}
} // namespace Fooyin

#include "devicepage.moc"
#include "moc_devicepage.cpp"
