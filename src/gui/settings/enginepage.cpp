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

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
class EnginePageWidget : public SettingsPageWidget
{
public:
    explicit EnginePageWidget(SettingsManager* settings, EngineHandler* engineHandler);

    void apply() override;
    void reset() override;

    void setupOutputs();
    void setupDevices(const QString& output);

private:
    SettingsManager* m_settings;
    EngineHandler* m_engineHandler;

    ExpandingComboBox* m_outputBox;
    ExpandingComboBox* m_deviceBox;
};

EnginePageWidget::EnginePageWidget(SettingsManager* settings, EngineHandler* engineHandler)
    : m_settings{settings}
    , m_engineHandler{engineHandler}
    , m_outputBox{new ExpandingComboBox(this)}
    , m_deviceBox{new ExpandingComboBox(this)}
{
    setupOutputs();
    setupDevices(m_outputBox->currentText());

    auto* outputLabel = new QLabel(tr("Output:"), this);
    auto* deviceLabel = new QLabel(tr("Device:"), this);

    auto* mainLayout = new QGridLayout(this);
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
    const QString output = m_outputBox->currentText() + u"|"_s + m_deviceBox->currentData().toString();
    m_settings->set<Settings::Core::AudioOutput>(output);
}

void EnginePageWidget::reset()
{
    m_settings->reset<Settings::Core::AudioOutput>();
    setupOutputs();
    setupDevices(m_outputBox->currentText());
}

void EnginePageWidget::setupOutputs()
{
    const QStringList currentOutput = m_settings->value<Settings::Core::AudioOutput>().split(u"|"_s);

    const QString outName = !currentOutput.empty() ? currentOutput.at(0) : u""_s;
    const auto outputs    = m_engineHandler->getAllOutputs();

    m_outputBox->clear();

    for(const auto& output : outputs) {
        m_outputBox->addItem(output);
        if(output == outName) {
            m_outputBox->setCurrentIndex(m_outputBox->count() - 1);
        }
    }

    if(!outputs.empty() && outName.isEmpty()) {
        m_outputBox->setCurrentIndex(0);
    }
}

void EnginePageWidget::setupDevices(const QString& output)
{
    if(output.isEmpty()) {
        return;
    }

    m_deviceBox->clear();

    const QStringList currentOutput = m_settings->value<Settings::Core::AudioOutput>().split(u"|"_s);

    if(currentOutput.empty()) {
        return;
    }

    const QString currentDevice = currentOutput.size() > 1 ? currentOutput.at(1) : u""_s;
    const auto outputDevices    = m_engineHandler->getOutputDevices(output);

    for(const auto& [name, desc] : outputDevices) {
        m_deviceBox->addItem(desc, name);
        const int index = m_deviceBox->count() - 1;
        if(name == currentDevice) {
            m_deviceBox->setCurrentIndex(index);
        }
    }

    if(!outputDevices.empty() && currentDevice.isEmpty()) {
        m_deviceBox->setCurrentIndex(0);
    }

    m_deviceBox->resizeDropDown();
    m_deviceBox->resizeToFitCurrent();
}

EnginePage::EnginePage(SettingsManager* settings, EngineHandler* engineHandler)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Engine);
    setName(tr("Engine"));
    setCategory({tr("Engine")});
    setWidgetCreator([settings, engineHandler] { return new EnginePageWidget(settings, engineHandler); });
}
} // namespace Fooyin
