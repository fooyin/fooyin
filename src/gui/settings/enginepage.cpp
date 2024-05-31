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
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <utils/expandingcombobox.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QSpinBox>

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
    QSpinBox* m_bufferSize;

    QGroupBox* m_fadingBox;
    QSpinBox* m_fadingStopIn;
    QSpinBox* m_fadingStopOut;
    // QSpinBox* m_fadingSeekIn;
    // QSpinBox* m_fadingSeekOut;
};

EnginePageWidget::EnginePageWidget(SettingsManager* settings, EngineController* engine)
    : m_settings{settings}
    , m_engine{engine}
    , m_outputBox{new ExpandingComboBox(this)}
    , m_deviceBox{new ExpandingComboBox(this)}
    , m_gaplessPlayback{new QCheckBox(tr("Gapless playback"), this)}
    , m_bufferSize{new QSpinBox(this)}
    , m_fadingBox{new QGroupBox(tr("Fading"), this)}
    , m_fadingStopIn{new QSpinBox(this)}
    , m_fadingStopOut{new QSpinBox(this)}
// , m_fadingSeekIn{new QSpinBox(this)}
// , m_fadingSeekOut{new QSpinBox(this)}
{
    auto* outputLabel = new QLabel(tr("Output") + QStringLiteral(":"), this);
    auto* deviceLabel = new QLabel(tr("Device") + QStringLiteral(":"), this);

    auto* generalBox    = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalBox);

    m_gaplessPlayback->setToolTip(
        tr("Try to play consecutive tracks with no silence or disruption at the point of file change"));

    generalLayout->addWidget(m_gaplessPlayback, 0, 0, 1, 3);

    auto* bufferLabel = new QLabel(tr("Buffer length") + QStringLiteral(":"), this);

    m_bufferSize->setSuffix(QStringLiteral(" ms"));
    m_bufferSize->setSingleStep(100);
    m_bufferSize->setMinimum(50);
    m_bufferSize->setMaximum(30000);

    generalLayout->addWidget(bufferLabel, 1, 0);
    generalLayout->addWidget(m_bufferSize, 1, 1);

    generalLayout->setColumnStretch(2, 1);

    m_fadingBox->setCheckable(true);
    auto* fadingLayout = new QGridLayout(m_fadingBox);

    auto* fadingInLabel  = new QLabel(tr("Fade In"), this);
    auto* fadingOutLabel = new QLabel(tr("Fade Out"), this);
    auto* stopPauseLabel = new QLabel(tr("Pause/Stop"), this);
    // auto* seekLabel      = new QLabel(tr("Seek"), this);

    m_fadingStopIn->setSuffix(QStringLiteral("ms"));
    m_fadingStopOut->setSuffix(QStringLiteral("ms"));
    // m_fadingSeekIn->setSuffix(QStringLiteral("ms"));
    // m_fadingSeekOut->setSuffix(QStringLiteral("ms"));

    m_fadingStopIn->setMaximum(10000);
    m_fadingStopOut->setMaximum(10000);
    // m_fadingSeekIn->setMaximum(10000);
    // m_fadingSeekOut->setMaximum(10000);

    m_fadingStopIn->setSingleStep(100);
    m_fadingStopOut->setSingleStep(100);
    // m_fadingSeekIn->setSingleStep(100);
    // m_fadingSeekOut->setSingleStep(100);

    fadingLayout->addWidget(fadingInLabel, 0, 1);
    fadingLayout->addWidget(fadingOutLabel, 0, 2);
    fadingLayout->addWidget(stopPauseLabel, 1, 0);
    // fadingLayout->addWidget(seekLabel, 2, 0);
    fadingLayout->addWidget(m_fadingStopIn, 1, 1);
    fadingLayout->addWidget(m_fadingStopOut, 1, 2);
    // fadingLayout->addWidget(m_fadingSeekIn, 2, 1);
    // fadingLayout->addWidget(m_fadingSeekOut, 2, 2);
    fadingLayout->setColumnStretch(3, 1);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(outputLabel, 0, 0);
    mainLayout->addWidget(m_outputBox, 0, 1);
    mainLayout->addWidget(deviceLabel, 1, 0);
    mainLayout->addWidget(m_deviceBox, 1, 1);
    mainLayout->addWidget(generalBox, 2, 0, 1, 2);
    mainLayout->addWidget(m_fadingBox, 3, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto matchBufferInterval = [this](const int value) {
        if(value > m_bufferSize->value()) {
            m_bufferSize->setValue(value);
        }
    };

    QObject::connect(m_outputBox, &QComboBox::currentTextChanged, this, &EnginePageWidget::setupDevices);
    QObject::connect(m_fadingStopIn, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_fadingStopOut, &QSpinBox::valueChanged, this, matchBufferInterval);
}

void EnginePageWidget::load()
{
    setupOutputs();
    setupDevices(m_outputBox->currentText());
    m_gaplessPlayback->setChecked(m_settings->value<Settings::Core::GaplessPlayback>());
    m_bufferSize->setValue(m_settings->value<Settings::Core::BufferLength>());

    m_fadingBox->setChecked(m_settings->value<Settings::Core::Internal::EngineFading>());
    const auto fadingValues = m_settings->value<Settings::Core::Internal::FadingIntervals>().value<FadingIntervals>();
    m_fadingStopIn->setValue(fadingValues.inPauseStop);
    m_fadingStopOut->setValue(fadingValues.outPauseStop);
    // m_fadingSeekIn->setValue(fadingValues.inSeek);
    // m_fadingSeekOut->setValue(fadingValues.outSeek);
}

void EnginePageWidget::apply()
{
    const QString output = m_outputBox->currentText() + QStringLiteral("|") + m_deviceBox->currentData().toString();
    m_settings->set<Settings::Core::AudioOutput>(output);
    m_settings->set<Settings::Core::GaplessPlayback>(m_gaplessPlayback->isChecked());
    m_settings->set<Settings::Core::BufferLength>(m_bufferSize->value());

    FadingIntervals fadingValues;
    fadingValues.inPauseStop  = m_fadingStopIn->value();
    fadingValues.outPauseStop = m_fadingStopOut->value();
    // fadingValues.inSeek       = m_fadingSeekIn->value();
    // fadingValues.outSeek      = m_fadingSeekOut->value();

    m_settings->set<Settings::Core::Internal::EngineFading>(m_fadingBox->isChecked());
    m_settings->set<Settings::Core::Internal::FadingIntervals>(QVariant::fromValue(fadingValues));
}

void EnginePageWidget::reset()
{
    m_settings->reset<Settings::Core::AudioOutput>();
    m_settings->reset<Settings::Core::GaplessPlayback>();
    m_settings->reset<Settings::Core::BufferLength>();
    m_settings->reset<Settings::Core::Internal::EngineFading>();
    m_settings->reset<Settings::Core::Internal::FadingIntervals>();
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
