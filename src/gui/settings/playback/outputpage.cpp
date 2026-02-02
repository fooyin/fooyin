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

#include "outputpage.h"

#include <core/coresettings.h>
#include <core/engine/enginedefs.h>
#include <core/engine/enginehandler.h>
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <gui/widgets/expandingcombobox.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
class OutputPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit OutputPageWidget(EngineController* engine, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

    void setupOutputs();
    void setupDevices(const QString& output);

private:
    EngineController* m_engine;
    SettingsManager* m_settings;

    ExpandingComboBox* m_outputBox;
    ExpandingComboBox* m_deviceBox;

    QCheckBox* m_gaplessPlayback;
    QSpinBox* m_bufferSize;
    ExpandingComboBox* m_bitDepthBox;

    QGroupBox* m_fadingBox;
    QSpinBox* m_fadingPauseIn;
    QSpinBox* m_fadingPauseOut;
    QSpinBox* m_fadingStopIn;
    QSpinBox* m_fadingStopOut;
    QComboBox* m_pauseFadeCurve;
    QComboBox* m_stopFadeCurve;

    QGroupBox* m_crossfadeBox;
    QSpinBox* m_crossfadeManualIn;
    QSpinBox* m_crossfadeManualOut;
    QSpinBox* m_crossfadeAutoIn;
    QSpinBox* m_crossfadeAutoOut;
    QSpinBox* m_crossfadeSeekIn;
    QSpinBox* m_crossfadeSeekOut;
};

OutputPageWidget::OutputPageWidget(EngineController* engine, SettingsManager* settings)
    : m_engine{engine}
    , m_settings{settings}
    , m_outputBox{new ExpandingComboBox(this)}
    , m_deviceBox{new ExpandingComboBox(this)}
    , m_gaplessPlayback{new QCheckBox(tr("Gapless playback"), this)}
    , m_bufferSize{new QSpinBox(this)}
    , m_bitDepthBox{new ExpandingComboBox(this)}
    , m_fadingBox{new QGroupBox(tr("Fading"), this)}
    , m_fadingPauseIn{new QSpinBox(this)}
    , m_fadingPauseOut{new QSpinBox(this)}
    , m_fadingStopIn{new QSpinBox(this)}
    , m_fadingStopOut{new QSpinBox(this)}
    , m_pauseFadeCurve{new QComboBox(this)}
    , m_stopFadeCurve{new QComboBox(this)}
    , m_crossfadeBox{new QGroupBox(tr("Crossfading"), this)}
    , m_crossfadeManualIn{new QSpinBox(this)}
    , m_crossfadeManualOut{new QSpinBox(this)}
    , m_crossfadeAutoIn{new QSpinBox(this)}
    , m_crossfadeAutoOut{new QSpinBox(this)}
    , m_crossfadeSeekIn{new QSpinBox(this)}
    , m_crossfadeSeekOut{new QSpinBox(this)}
{
    auto* generalBox    = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalBox);

    m_gaplessPlayback->setToolTip(
        tr("Try to play consecutive tracks with no silence or disruption at the point of file change"));

    generalLayout->addWidget(m_gaplessPlayback, 0, 0, 1, 3);

    m_bufferSize->setSuffix(u" ms"_s);
    m_bufferSize->setSingleStep(100);
    m_bufferSize->setMinimum(50);
    m_bufferSize->setMaximum(30000);

    m_bitDepthBox->addItem(tr("Disabled"), static_cast<int>(SampleFormat::Unknown));
    m_bitDepthBox->addItem(tr("16-bit"), static_cast<int>(SampleFormat::S16));
    m_bitDepthBox->addItem(tr("16-bit (dithered)"), static_cast<int>(SampleFormat::S16));
    m_bitDepthBox->setItemData(m_bitDepthBox->count() - 1, true, Qt::UserRole + 1);
    m_bitDepthBox->addItem(tr("24-bit"), static_cast<int>(SampleFormat::S24In32));
    m_bitDepthBox->addItem(tr("32-bit"), static_cast<int>(SampleFormat::S32));
    m_bitDepthBox->addItem(tr("32-bit float"), static_cast<int>(SampleFormat::F32));
    m_bitDepthBox->setToolTip(tr("Override the output sample format. Devices may choose a compatible format."));
    m_bitDepthBox->resizeDropDown();

    generalLayout->addWidget(new QLabel(tr("Buffer length") + u":"_s, this), 1, 0);
    generalLayout->addWidget(m_bufferSize, 1, 1);
    generalLayout->addWidget(new QLabel(tr("Override bit depth") + u":"_s, this), 2, 0);
    generalLayout->addWidget(m_bitDepthBox, 2, 1);

    generalLayout->setColumnStretch(2, 1);

    auto* fadingLayout = new QGridLayout(m_fadingBox);

    m_fadingBox->setCheckable(true);

    m_fadingPauseIn->setSuffix(u"ms"_s);
    m_fadingPauseOut->setSuffix(u"ms"_s);
    m_fadingStopIn->setSuffix(u"ms"_s);
    m_fadingStopOut->setSuffix(u"ms"_s);
    m_crossfadeManualIn->setSuffix(u"ms"_s);
    m_crossfadeManualOut->setSuffix(u"ms"_s);
    m_crossfadeAutoIn->setSuffix(u"ms"_s);
    m_crossfadeAutoOut->setSuffix(u"ms"_s);
    m_crossfadeSeekIn->setSuffix(u"ms"_s);
    m_crossfadeSeekOut->setSuffix(u"ms"_s);

    m_fadingPauseIn->setMaximum(10000);
    m_fadingPauseOut->setMaximum(10000);
    m_fadingStopIn->setMaximum(10000);
    m_fadingStopOut->setMaximum(10000);

    m_fadingPauseIn->setSingleStep(100);
    m_fadingPauseOut->setSingleStep(100);
    m_fadingStopIn->setSingleStep(100);
    m_fadingStopOut->setSingleStep(100);

    const auto addFadeCurveItems = [](QComboBox* combo) {
        combo->addItem(tr("Linear"), static_cast<int>(Engine::FadeCurve::Linear));
        combo->addItem(tr("Ease Out (Sine)"), static_cast<int>(Engine::FadeCurve::Sine));
        combo->addItem(tr("Ease In (Cosine)"), static_cast<int>(Engine::FadeCurve::Cosine));
        combo->addItem(tr("Ease Out (Exponential)"), static_cast<int>(Engine::FadeCurve::Logarithmic));
        combo->addItem(tr("Ease In (Exponential)"), static_cast<int>(Engine::FadeCurve::Exponential));
        combo->addItem(tr("Smooth S-Curve"), static_cast<int>(Engine::FadeCurve::SmootherStep));
    };

    addFadeCurveItems(m_pauseFadeCurve);
    addFadeCurveItems(m_stopFadeCurve);

    int row{0};
    fadingLayout->addWidget(new QLabel(tr("Fade In"), this), row, 1);
    fadingLayout->addWidget(new QLabel(tr("Fade Out"), this), row, 2);
    fadingLayout->addWidget(new QLabel(tr("Curve"), this), row++, 3);

    fadingLayout->addWidget(new QLabel(tr("Pause"), this), row, 0);
    fadingLayout->addWidget(m_fadingPauseIn, row, 1);
    fadingLayout->addWidget(m_fadingPauseOut, row, 2);
    fadingLayout->addWidget(m_pauseFadeCurve, row++, 3);

    fadingLayout->addWidget(new QLabel(tr("Stop"), this), row, 0);
    fadingLayout->addWidget(m_fadingStopIn, row, 1);
    fadingLayout->addWidget(m_fadingStopOut, row, 2);
    fadingLayout->addWidget(m_stopFadeCurve, row++, 3);

    fadingLayout->setColumnStretch(4, 1);

    auto* crossmixLayout = new QGridLayout(m_crossfadeBox);

    m_crossfadeBox->setCheckable(true);

    m_crossfadeManualIn->setSuffix(u"ms"_s);
    m_crossfadeManualOut->setSuffix(u"ms"_s);
    m_crossfadeAutoIn->setSuffix(u"ms"_s);
    m_crossfadeAutoOut->setSuffix(u"ms"_s);

    m_crossfadeManualIn->setMaximum(30000);
    m_crossfadeManualOut->setMaximum(30000);
    m_crossfadeAutoIn->setMaximum(30000);
    m_crossfadeAutoOut->setMaximum(30000);
    m_crossfadeSeekIn->setMaximum(10000);
    m_crossfadeSeekOut->setMaximum(10000);

    m_crossfadeManualIn->setSingleStep(100);
    m_crossfadeManualOut->setSingleStep(100);
    m_crossfadeAutoIn->setSingleStep(100);
    m_crossfadeAutoOut->setSingleStep(100);
    m_crossfadeSeekIn->setSingleStep(100);
    m_crossfadeSeekOut->setSingleStep(100);

    row = 0;
    crossmixLayout->addWidget(new QLabel(tr("Fade In"), this), row, 1);
    crossmixLayout->addWidget(new QLabel(tr("Fade Out"), this), row++, 2);

    crossmixLayout->addWidget(new QLabel(tr("Seek"), this), row, 0);
    crossmixLayout->addWidget(m_crossfadeSeekIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeSeekOut, row++, 2);

    crossmixLayout->addWidget(new QLabel(tr("Manual track change"), this), row, 0);
    crossmixLayout->addWidget(m_crossfadeManualIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeManualOut, row++, 2);

    crossmixLayout->addWidget(new QLabel(tr("Auto track change"), this), row, 0);
    crossmixLayout->addWidget(m_crossfadeAutoIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeAutoOut, row++, 2);

    crossmixLayout->setColumnStretch(3, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(new QLabel(tr("Output") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_outputBox, row++, 1);
    mainLayout->addWidget(new QLabel(tr("Device") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_deviceBox, row++, 1);
    mainLayout->addWidget(generalBox, row++, 0, 1, 2);
    mainLayout->addWidget(m_fadingBox, row++, 0, 1, 2);
    mainLayout->addWidget(m_crossfadeBox, row++, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto matchBufferInterval = [this](const int value) {
        if(value > m_bufferSize->value()) {
            m_bufferSize->setValue(value);
        }
    };

    QObject::connect(m_outputBox, &QComboBox::currentTextChanged, this, &OutputPageWidget::setupDevices);
    QObject::connect(m_fadingPauseIn, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_fadingPauseOut, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_fadingStopIn, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_fadingStopOut, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_crossfadeManualIn, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_crossfadeManualOut, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_crossfadeAutoIn, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_crossfadeAutoOut, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_crossfadeSeekIn, &QSpinBox::valueChanged, this, matchBufferInterval);
    QObject::connect(m_crossfadeSeekOut, &QSpinBox::valueChanged, this, matchBufferInterval);
}

void OutputPageWidget::load()
{
    setupOutputs();
    setupDevices(m_outputBox->currentText());
    m_gaplessPlayback->setChecked(m_settings->value<Settings::Core::GaplessPlayback>());
    m_bufferSize->setValue(m_settings->value<Settings::Core::BufferLength>());

    const int bitDepthSetting = m_settings->value<Settings::Core::OutputBitDepth>();
    const bool ditherSetting
        = bitDepthSetting == static_cast<int>(SampleFormat::S16) && m_settings->value<Settings::Core::OutputDither>();
    int bitDepthIndex{-1};
    const int bitDepthItemCount = m_bitDepthBox->count();
    for(int i{0}; i < bitDepthItemCount; ++i) {
        if(m_bitDepthBox->itemData(i).toInt() == bitDepthSetting
           && m_bitDepthBox->itemData(i, Qt::UserRole + 1).toBool() == ditherSetting) {
            bitDepthIndex = i;
            break;
        }
    }
    m_bitDepthBox->setCurrentIndex(bitDepthIndex >= 0 ? bitDepthIndex : 0);
    m_bitDepthBox->resizeToFitCurrent();

    m_fadingBox->setChecked(m_settings->value<Settings::Core::Internal::EngineFading>());
    const auto fadingValues = m_settings->value<Settings::Core::Internal::FadingValues>().value<Engine::FadingValues>();
    m_fadingPauseIn->setValue(fadingValues.pause.in);
    m_fadingPauseOut->setValue(fadingValues.pause.out);
    m_pauseFadeCurve->setCurrentIndex(static_cast<int>(fadingValues.pause.curve));
    m_fadingStopIn->setValue(fadingValues.stop.in);
    m_fadingStopOut->setValue(fadingValues.stop.out);
    m_stopFadeCurve->setCurrentIndex(static_cast<int>(fadingValues.stop.curve));

    m_crossfadeBox->setChecked(m_settings->value<Settings::Core::Internal::EngineCrossfading>());
    const auto crossfadingValues
        = m_settings->value<Settings::Core::Internal::CrossfadingValues>().value<Engine::CrossfadingValues>();
    m_crossfadeManualIn->setValue(crossfadingValues.manualChange.in);
    m_crossfadeManualOut->setValue(crossfadingValues.manualChange.out);
    m_crossfadeAutoIn->setValue(crossfadingValues.autoChange.in);
    m_crossfadeAutoOut->setValue(crossfadingValues.autoChange.out);
    m_crossfadeSeekIn->setValue(crossfadingValues.seek.in);
    m_crossfadeSeekOut->setValue(crossfadingValues.seek.out);
}

void OutputPageWidget::apply()
{
    const QString output = m_outputBox->currentText() + u"|"_s + m_deviceBox->currentData().toString();
    m_settings->set<Settings::Core::AudioOutput>(output);
    m_settings->set<Settings::Core::GaplessPlayback>(m_gaplessPlayback->isChecked());
    m_settings->set<Settings::Core::BufferLength>(m_bufferSize->value());

    const int selectedBitDepth = m_bitDepthBox->currentData().toInt();
    bool ditherEnabled         = m_bitDepthBox->currentData(Qt::UserRole + 1).toBool();
    if(selectedBitDepth != static_cast<int>(SampleFormat::S16)) {
        ditherEnabled = false;
    }
    m_settings->set<Settings::Core::OutputBitDepth>(selectedBitDepth);
    m_settings->set<Settings::Core::OutputDither>(ditherEnabled);

    Engine::FadingValues fadingValues;
    fadingValues.pause.in    = m_fadingPauseIn->value();
    fadingValues.pause.out   = m_fadingPauseOut->value();
    fadingValues.pause.curve = static_cast<Engine::FadeCurve>(m_pauseFadeCurve->currentData().toInt());
    fadingValues.stop.in     = m_fadingStopIn->value();
    fadingValues.stop.out    = m_fadingStopOut->value();
    fadingValues.stop.curve  = static_cast<Engine::FadeCurve>(m_stopFadeCurve->currentData().toInt());

    Engine::CrossfadingValues crossfadingValues;
    crossfadingValues.manualChange.in  = m_crossfadeManualIn->value();
    crossfadingValues.manualChange.out = m_crossfadeManualOut->value();
    crossfadingValues.autoChange.in    = m_crossfadeAutoIn->value();
    crossfadingValues.autoChange.out   = m_crossfadeAutoOut->value();
    crossfadingValues.seek.in          = m_crossfadeSeekIn->value();
    crossfadingValues.seek.out         = m_crossfadeSeekOut->value();

    m_settings->set<Settings::Core::Internal::EngineFading>(m_fadingBox->isChecked());
    m_settings->set<Settings::Core::Internal::FadingValues>(QVariant::fromValue(fadingValues));
    m_settings->set<Settings::Core::Internal::EngineCrossfading>(m_crossfadeBox->isChecked());
    m_settings->set<Settings::Core::Internal::CrossfadingValues>(QVariant::fromValue(crossfadingValues));
}

void OutputPageWidget::reset()
{
    m_settings->reset<Settings::Core::AudioOutput>();
    m_settings->reset<Settings::Core::GaplessPlayback>();
    m_settings->reset<Settings::Core::BufferLength>();
    m_settings->reset<Settings::Core::OutputBitDepth>();
    m_settings->reset<Settings::Core::OutputDither>();
    m_settings->reset<Settings::Core::Internal::EngineFading>();
    m_settings->reset<Settings::Core::Internal::FadingValues>();
    m_settings->reset<Settings::Core::Internal::EngineCrossfading>();
    m_settings->reset<Settings::Core::Internal::CrossfadingValues>();
}

void OutputPageWidget::setupOutputs()
{
    const QStringList currentOutput = m_settings->value<Settings::Core::AudioOutput>().split(u"|"_s);

    const QString outName = !currentOutput.empty() ? currentOutput.at(0) : QString{};
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

void OutputPageWidget::setupDevices(const QString& output)
{
    if(output.isEmpty()) {
        return;
    }

    m_deviceBox->clear();

    const QStringList currentOutput = m_settings->value<Settings::Core::AudioOutput>().split(u"|"_s);

    if(currentOutput.empty()) {
        return;
    }

    const QString currentDevice = currentOutput.size() > 1 ? currentOutput.at(1) : QString{};
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

OutputPage::OutputPage(EngineController* engine, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Output);
    setName(tr("General"));
    setCategory({tr("Playback"), tr("Output")});
    setWidgetCreator([engine, settings] { return new OutputPageWidget(engine, settings); });
}
} // namespace Fooyin

#include "moc_outputpage.cpp"
#include "outputpage.moc"
