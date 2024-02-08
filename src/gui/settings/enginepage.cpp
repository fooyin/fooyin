/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListView>

namespace Fooyin {
class EnginePageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit EnginePageWidget(SettingsManager* settings, EngineController* engine);

    void load() override;
    void apply() override;
    void reset() override;

    void setupOutputs();
    void setupDevices(const QString& output);

private:
    SettingsManager* m_settings;
    EngineController* m_engine;

    ExpandingComboBox* m_outputBox;
    ExpandingComboBox* m_deviceBox;

    QCheckBox* m_gaplessPlayback;
};

EnginePageWidget::EnginePageWidget(SettingsManager* settings, EngineController* engine)
    : m_settings{settings}
    , m_engine{engine}
    , m_outputBox{new ExpandingComboBox(this)}
    , m_deviceBox{new ExpandingComboBox(this)}
    , m_gaplessPlayback{new QCheckBox(tr("Gapless Playback"), this)}
{
    auto* outputLabel = new QLabel(tr("Output") + ":", this);
    auto* deviceLabel = new QLabel(tr("Device") + ":", this);

    auto* generalBox    = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QVBoxLayout(generalBox);

    m_gaplessPlayback->setToolTip(
        tr("Try to play consecutive tracks with no silence or disruption at the point of file change"));

    generalLayout->addWidget(m_gaplessPlayback);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(outputLabel, 0, 0);
    mainLayout->addWidget(m_outputBox, 0, 1);
    mainLayout->addWidget(deviceLabel, 1, 0);
    mainLayout->addWidget(m_deviceBox, 1, 1);
    mainLayout->addWidget(generalBox, 2, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(3, 1);

    QObject::connect(m_outputBox, &QComboBox::currentTextChanged, this, &EnginePageWidget::setupDevices);
}

void EnginePageWidget::load()
{
    setupOutputs();
    setupDevices(m_outputBox->currentText());
    m_gaplessPlayback->setChecked(m_settings->value<Settings::Core::GaplessPlayback>());
}

void EnginePageWidget::apply()
{
    const QString output = m_outputBox->currentText() + QStringLiteral("|") + m_deviceBox->currentData().toString();
    m_settings->set<Settings::Core::AudioOutput>(output);
    m_settings->set<Settings::Core::GaplessPlayback>(m_gaplessPlayback->isChecked());
}

void EnginePageWidget::reset()
{
    m_settings->reset<Settings::Core::AudioOutput>();
    m_settings->reset<Settings::Core::GaplessPlayback>();
}

void EnginePageWidget::setupOutputs()
{
    const QStringList currentOutput = m_settings->value<Settings::Core::AudioOutput>().split(QStringLiteral("|"));

    const QString outName = !currentOutput.empty() ? currentOutput.at(0) : QStringLiteral("");
    const auto outputs    = m_engine->getAllOutputs();

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

    const QStringList currentOutput = m_settings->value<Settings::Core::AudioOutput>().split(QStringLiteral("|"));

    if(currentOutput.empty()) {
        return;
    }

    const QString currentDevice = currentOutput.size() > 1 ? currentOutput.at(1) : QStringLiteral("");
    const auto outputDevices    = m_engine->getOutputDevices(output);

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

EnginePage::EnginePage(SettingsManager* settings, EngineController* engine)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Engine);
    setName(tr("Engine"));
    setCategory({tr("Engine")});
    setWidgetCreator([settings, engine] { return new EnginePageWidget(settings, engine); });
}
} // namespace Fooyin

#include "enginepage.moc"
#include "moc_enginepage.cpp"
