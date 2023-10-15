/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "enginepage.h"

#include <core/coresettings.h>
#include <core/engine/enginehandler.h>
#include <gui/guiconstants.h>
#include <utils/expandingcombobox.h>
#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QListView>

namespace Fy::Gui::Settings {
class EnginePageWidget : public Utils::SettingsPageWidget
{
public:
    explicit EnginePageWidget(Utils::SettingsManager* settings, Core::Engine::EngineHandler* engineHandler);

    void apply() override;
    void reset() override;

    void setupDevices(const QString& output);

private:
    Utils::SettingsManager* m_settings;
    Core::Engine::EngineHandler* m_engineHandler;

    Utils::ExpandingComboBox* m_outputBox;
    Utils::ExpandingComboBox* m_deviceBox;
};

EnginePageWidget::EnginePageWidget(Utils::SettingsManager* settings, Core::Engine::EngineHandler* engineHandler)
    : m_settings{settings}
    , m_engineHandler{engineHandler}
    , m_outputBox{new Utils::ExpandingComboBox(this)}
    , m_deviceBox{new Utils::ExpandingComboBox(this)}
{
    const QString currentOutput = m_settings->value<Core::Settings::AudioOutput>();
    const auto outputs          = m_engineHandler->getAllOutputs();

    for(const auto& output : outputs) {
        m_outputBox->addItem(output);
        if(output == currentOutput) {
            m_outputBox->setCurrentIndex(m_outputBox->count() - 1);
        }
    }

    setupDevices(currentOutput);
    m_deviceBox->resizeToFitCurrent();

    auto* outputLabel = new QLabel("Output:", this);
    auto* deviceLabel = new QLabel("Device:", this);

    auto mainLayout = new QGridLayout(this);
    mainLayout->addWidget(outputLabel, 0, 0);
    mainLayout->addWidget(m_outputBox, 0, 1);
    mainLayout->addWidget(deviceLabel, 1, 0);
    mainLayout->addWidget(m_deviceBox, 1, 1);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(2, 1);

    QObject::connect(m_outputBox, &QComboBox::currentTextChanged, this, &EnginePageWidget::setupDevices);
}

void EnginePageWidget::apply()
{
    const QString output = m_outputBox->currentText();
    m_settings->set<Core::Settings::AudioOutput>(output);

    const QString device = m_deviceBox->currentData().toString();
    m_settings->set<Core::Settings::OutputDevice>(device);
}

void EnginePageWidget::reset()
{
    m_settings->reset<Core::Settings::AudioOutput>();
    m_settings->reset<Core::Settings::OutputDevice>();
}

void EnginePageWidget::setupDevices(const QString& output)
{
    if(output.isEmpty()) {
        return;
    }

    m_deviceBox->clear();

    const QString currentDevice = m_settings->value<Core::Settings::OutputDevice>();
    const auto outputDevices    = m_engineHandler->getOutputDevices(output);

    for(const auto& [name, desc] : outputDevices) {
        m_deviceBox->addItem(desc, name);
        const int index = m_deviceBox->count() - 1;
        if(name == currentDevice) {
            m_deviceBox->setCurrentIndex(index);
        }
    }
    m_deviceBox->resizeDropDown();
}

EnginePage::EnginePage(Utils::SettingsManager* settings, Core::Engine::EngineHandler* engineHandler)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Engine);
    setName(tr("Engine"));
    setCategory({tr("Engine")});
    setWidgetCreator([settings, engineHandler] {
        return new EnginePageWidget(settings, engineHandler);
    });
}
} // namespace Fy::Gui::Settings
