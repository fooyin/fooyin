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
#include <core/engine/enginehandler.h>
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <gui/widgets/expandingcombobox.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSizePolicy>
#include <QSpinBox>

using namespace Qt::StringLiterals;

constexpr auto MinBufferSize                 = 500;
constexpr auto DecodeHighWatermarkMaxRatio   = 0.99;
constexpr auto DecodeHighWatermarkMaxPercent = 99;

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
    QSpinBox* m_decodeLowWatermark;
    QSpinBox* m_decodeHighWatermark;
    QLabel* m_decodeLowWatermarkHint;
    QLabel* m_decodeHighWatermarkHint;
    ExpandingComboBox* m_bitDepthBox;
};

OutputPageWidget::OutputPageWidget(EngineController* engine, SettingsManager* settings)
    : m_engine{engine}
    , m_settings{settings}
    , m_outputBox{new ExpandingComboBox(this)}
    , m_deviceBox{new ExpandingComboBox(this)}
    , m_gaplessPlayback{new QCheckBox(tr("Gapless playback"), this)}
    , m_bufferSize{new QSpinBox(this)}
    , m_decodeLowWatermark{new QSpinBox(this)}
    , m_decodeHighWatermark{new QSpinBox(this)}
    , m_decodeLowWatermarkHint{new QLabel(this)}
    , m_decodeHighWatermarkHint{new QLabel(this)}
    , m_bitDepthBox{new ExpandingComboBox(this)}
{
    auto* generalBox    = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalBox);
    auto* bufferBox     = new QGroupBox(tr("Buffer"), this);
    auto* bufferLayout  = new QGridLayout(bufferBox);

    m_gaplessPlayback->setToolTip(
        tr("Try to play consecutive tracks with no silence or disruption at the point of file change"));

    generalLayout->addWidget(m_gaplessPlayback, 0, 0, 1, 2);

    m_bufferSize->setSuffix(u" ms"_s);
    m_bufferSize->setSingleStep(100);
    m_bufferSize->setMinimum(MinBufferSize);
    m_bufferSize->setMaximum(30000);
    m_bufferSize->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    const auto setupWatermarkRatioSpinBox = [](QSpinBox* spinBox) {
        spinBox->setSingleStep(1);
        spinBox->setMinimum(5);
        spinBox->setMaximum(DecodeHighWatermarkMaxPercent);
        spinBox->setSuffix(u"%"_s);
    };

    setupWatermarkRatioSpinBox(m_decodeLowWatermark);
    setupWatermarkRatioSpinBox(m_decodeHighWatermark);
    m_decodeLowWatermark->setMinimumWidth(80);
    m_decodeHighWatermark->setMinimumWidth(80);
    m_decodeLowWatermark->setToolTip(tr("Decode starts/resumes when buffered audio drops below this watermark"));
    m_decodeHighWatermark->setToolTip(tr("Decode pauses when buffered audio reaches this watermark"));
    m_decodeLowWatermarkHint->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_decodeHighWatermarkHint->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_bitDepthBox->addItem(tr("Automatic"), static_cast<int>(SampleFormat::Unknown));
    m_bitDepthBox->addItem(tr("16-bit"), static_cast<int>(SampleFormat::S16));
    m_bitDepthBox->addItem(tr("16-bit (dithered)"), static_cast<int>(SampleFormat::S16));
    m_bitDepthBox->setItemData(m_bitDepthBox->count() - 1, true, Qt::UserRole + 1);
    m_bitDepthBox->addItem(tr("24-bit"), static_cast<int>(SampleFormat::S24In32));
    m_bitDepthBox->addItem(tr("32-bit"), static_cast<int>(SampleFormat::S32));
    m_bitDepthBox->addItem(tr("32-bit float"), static_cast<int>(SampleFormat::F32));
    m_bitDepthBox->setToolTip(tr("Override the output sample format. Devices may choose a compatible format."));
    m_bitDepthBox->resizeDropDown();

    generalLayout->addWidget(new QLabel(tr("Bit depth") + u":"_s, this), 1, 0);
    generalLayout->addWidget(m_bitDepthBox, 1, 1);
    generalLayout->setColumnStretch(2, 1);

    bufferLayout->addWidget(new QLabel(tr("Length") + u":"_s, this), 0, 0);
    bufferLayout->addWidget(m_bufferSize, 0, 1, Qt::AlignLeft);
    bufferLayout->addWidget(new QLabel(tr("Low watermark") + u":"_s, this), 1, 0);
    bufferLayout->addWidget(m_decodeLowWatermark, 1, 1, Qt::AlignLeft);
    bufferLayout->addWidget(m_decodeLowWatermarkHint, 1, 2);
    bufferLayout->addWidget(new QLabel(tr("High watermark") + u":"_s, this), 2, 0);
    bufferLayout->addWidget(m_decodeHighWatermark, 2, 1, Qt::AlignLeft);
    bufferLayout->addWidget(m_decodeHighWatermarkHint, 2, 2);
    bufferLayout->setColumnStretch(2, 1);

    auto* mainLayout = new QGridLayout(this);

    int row{0};
    mainLayout->addWidget(new QLabel(tr("Output") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_outputBox, row++, 1);
    mainLayout->addWidget(new QLabel(tr("Device") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_deviceBox, row++, 1);
    mainLayout->addWidget(generalBox, row++, 0, 1, 2);
    mainLayout->addWidget(bufferBox, row++, 0, 1, 2);
    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    const auto syncWatermarkRatioBounds = [this]() {
        const int lowRatio  = m_decodeLowWatermark->value();
        const int highRatio = m_decodeHighWatermark->value();
        if(lowRatio > highRatio) {
            m_decodeHighWatermark->setValue(lowRatio);
        }
    };

    const auto updateWatermarkHint = [this]() {
        const int bufferMs = m_bufferSize->value();
        const int lowPct   = m_decodeLowWatermark->value();
        const int highPct  = m_decodeHighWatermark->value();
        const int lowMs    = static_cast<int>(std::lround((static_cast<double>(bufferMs) * lowPct) / 100.0));
        const int highMs   = static_cast<int>(std::lround((static_cast<double>(bufferMs) * highPct) / 100.0));
        m_decodeLowWatermarkHint->setText(tr("Resume decoding below %1 ms").arg(lowMs));
        m_decodeHighWatermarkHint->setText(tr("Pause decoding at %1 ms").arg(highMs));
    };

    QObject::connect(m_outputBox, &QComboBox::currentTextChanged, this, &OutputPageWidget::setupDevices);
    QObject::connect(m_bufferSize, &QSpinBox::valueChanged, this, [updateWatermarkHint]() { updateWatermarkHint(); });
    QObject::connect(m_decodeLowWatermark, &QSpinBox::valueChanged, this,
                     [syncWatermarkRatioBounds]() { syncWatermarkRatioBounds(); });
    QObject::connect(m_decodeHighWatermark, &QSpinBox::valueChanged, this, [this](const int value) {
        if(value < m_decodeLowWatermark->value()) {
            m_decodeLowWatermark->setValue(value);
        }
    });
    QObject::connect(m_decodeLowWatermark, &QSpinBox::valueChanged, this,
                     [updateWatermarkHint]() { updateWatermarkHint(); });
    QObject::connect(m_decodeHighWatermark, &QSpinBox::valueChanged, this,
                     [updateWatermarkHint]() { updateWatermarkHint(); });

    syncWatermarkRatioBounds();
    updateWatermarkHint();
}

void OutputPageWidget::load()
{
    const auto sanitiseRatios = [](double lowRatio, double highRatio) {
        lowRatio  = std::clamp(lowRatio, 0.05, DecodeHighWatermarkMaxRatio);
        highRatio = std::clamp(highRatio, 0.05, DecodeHighWatermarkMaxRatio);
        if(lowRatio > highRatio) {
            std::swap(lowRatio, highRatio);
        }
        return std::pair{lowRatio, highRatio};
    };

    setupOutputs();
    setupDevices(m_outputBox->currentText());
    m_gaplessPlayback->setChecked(m_settings->value<Settings::Core::GaplessPlayback>());
    m_bufferSize->setValue(m_settings->value<Settings::Core::BufferLength>());

    const auto [lowWatermarkRatio, highWatermarkRatio]
        = sanitiseRatios(m_settings->value<Settings::Core::Internal::DecodeLowWatermarkRatio>(),
                         m_settings->value<Settings::Core::Internal::DecodeHighWatermarkRatio>());
    m_decodeLowWatermark->setValue(static_cast<int>(std::lround(lowWatermarkRatio * 100.0)));
    m_decodeHighWatermark->setValue(static_cast<int>(std::lround(highWatermarkRatio * 100.0)));

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
}

void OutputPageWidget::apply()
{
    const auto sanitiseRatios = [](double lowRatio, double highRatio) {
        lowRatio  = std::clamp(lowRatio, 0.05, DecodeHighWatermarkMaxRatio);
        highRatio = std::clamp(highRatio, 0.05, DecodeHighWatermarkMaxRatio);
        if(lowRatio > highRatio) {
            std::swap(lowRatio, highRatio);
        }
        return std::pair{lowRatio, highRatio};
    };

    const QString output = m_outputBox->currentText() + u"|"_s + m_deviceBox->currentData().toString();
    m_settings->set<Settings::Core::AudioOutput>(output);
    m_settings->set<Settings::Core::GaplessPlayback>(m_gaplessPlayback->isChecked());
    m_settings->set<Settings::Core::BufferLength>(m_bufferSize->value());

    const auto [lowWatermarkRatio, highWatermarkRatio]
        = sanitiseRatios(static_cast<double>(m_decodeLowWatermark->value()) / 100.0,
                         static_cast<double>(m_decodeHighWatermark->value()) / 100.0);
    m_settings->set<Settings::Core::Internal::DecodeLowWatermarkRatio>(lowWatermarkRatio);
    m_settings->set<Settings::Core::Internal::DecodeHighWatermarkRatio>(highWatermarkRatio);

    const int selectedBitDepth = m_bitDepthBox->currentData().toInt();
    bool ditherEnabled         = m_bitDepthBox->currentData(Qt::UserRole + 1).toBool();
    if(selectedBitDepth != static_cast<int>(SampleFormat::S16)) {
        ditherEnabled = false;
    }
    m_settings->set<Settings::Core::OutputBitDepth>(selectedBitDepth);
    m_settings->set<Settings::Core::OutputDither>(ditherEnabled);
}

void OutputPageWidget::reset()
{
    m_settings->reset<Settings::Core::AudioOutput>();
    m_settings->reset<Settings::Core::GaplessPlayback>();
    m_settings->reset<Settings::Core::BufferLength>();
    m_settings->reset<Settings::Core::OutputBitDepth>();
    m_settings->reset<Settings::Core::OutputDither>();
    m_settings->reset<Settings::Core::Internal::DecodeLowWatermarkRatio>();
    m_settings->reset<Settings::Core::Internal::DecodeHighWatermarkRatio>();
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
