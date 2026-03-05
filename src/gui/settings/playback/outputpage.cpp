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
#include <QHBoxLayout>
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
    QLabel* m_decodeWatermarkHint;
    ExpandingComboBox* m_bitDepthBox;

    QGroupBox* m_fadingBox;
    QCheckBox* m_fadingPauseEnabled;
    QCheckBox* m_fadingStopEnabled;
    QSpinBox* m_fadingPauseIn;
    QSpinBox* m_fadingPauseOut;
    QSpinBox* m_fadingStopIn;
    QSpinBox* m_fadingStopOut;
    QComboBox* m_pauseFadeCurve;
    QComboBox* m_stopFadeCurve;

    QGroupBox* m_crossfadeBox;
    QCheckBox* m_crossfadeSeekEnabled;
    QCheckBox* m_crossfadeManualEnabled;
    QCheckBox* m_crossfadeAutoEnabled;
    QSpinBox* m_crossfadeManualIn;
    QSpinBox* m_crossfadeManualOut;
    QSpinBox* m_crossfadeAutoIn;
    QSpinBox* m_crossfadeAutoOut;
    QComboBox* m_crossfadeAutoSwitchPolicy;
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
    , m_decodeLowWatermark{new QSpinBox(this)}
    , m_decodeHighWatermark{new QSpinBox(this)}
    , m_decodeWatermarkHint{new QLabel(this)}
    , m_bitDepthBox{new ExpandingComboBox(this)}
    , m_fadingBox{new QGroupBox(tr("Fading"), this)}
    , m_fadingPauseEnabled{new QCheckBox(tr("Pause"), this)}
    , m_fadingStopEnabled{new QCheckBox(tr("Stop"), this)}
    , m_fadingPauseIn{new QSpinBox(this)}
    , m_fadingPauseOut{new QSpinBox(this)}
    , m_fadingStopIn{new QSpinBox(this)}
    , m_fadingStopOut{new QSpinBox(this)}
    , m_pauseFadeCurve{new QComboBox(this)}
    , m_stopFadeCurve{new QComboBox(this)}
    , m_crossfadeBox{new QGroupBox(tr("Crossfading"), this)}
    , m_crossfadeSeekEnabled{new QCheckBox(tr("Seek"), this)}
    , m_crossfadeManualEnabled{new QCheckBox(tr("Manual track change"), this)}
    , m_crossfadeAutoEnabled{new QCheckBox(tr("Auto track change"), this)}
    , m_crossfadeManualIn{new QSpinBox(this)}
    , m_crossfadeManualOut{new QSpinBox(this)}
    , m_crossfadeAutoIn{new QSpinBox(this)}
    , m_crossfadeAutoOut{new QSpinBox(this)}
    , m_crossfadeAutoSwitchPolicy{new QComboBox(this)}
    , m_crossfadeSeekIn{new QSpinBox(this)}
    , m_crossfadeSeekOut{new QSpinBox(this)}
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
    m_decodeLowWatermark->setPrefix(tr("Low "));
    m_decodeHighWatermark->setPrefix(tr("High "));
    m_decodeLowWatermark->setMinimumWidth(95);
    m_decodeHighWatermark->setMinimumWidth(95);
    m_decodeLowWatermark->setToolTip(tr("Decode starts/resumes when buffered audio drops below this watermark"));
    m_decodeHighWatermark->setToolTip(tr("Decode pauses when buffered audio reaches this watermark"));
    m_decodeWatermarkHint->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto* decodeWatermarkLayout = new QHBoxLayout();
    decodeWatermarkLayout->setContentsMargins(0, 0, 0, 0);
    decodeWatermarkLayout->setSpacing(8);
    decodeWatermarkLayout->addWidget(m_decodeLowWatermark);
    decodeWatermarkLayout->addSpacing(10);
    decodeWatermarkLayout->addWidget(m_decodeHighWatermark);
    decodeWatermarkLayout->addStretch(1);

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

    bufferLayout->addWidget(new QLabel(tr("Buffer length") + u":"_s, this), 0, 0);
    bufferLayout->addWidget(m_bufferSize, 0, 1, Qt::AlignLeft);
    bufferLayout->addWidget(new QLabel(tr("Watermarks") + u":"_s, this), 1, 0);
    bufferLayout->addLayout(decodeWatermarkLayout, 1, 1);
    bufferLayout->addWidget(m_decodeWatermarkHint, 2, 1, 1, 2);
    bufferLayout->setColumnStretch(2, 1);

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

    fadingLayout->addWidget(m_fadingPauseEnabled, row, 0);
    fadingLayout->addWidget(m_fadingPauseIn, row, 1);
    fadingLayout->addWidget(m_fadingPauseOut, row, 2);
    fadingLayout->addWidget(m_pauseFadeCurve, row++, 3);

    fadingLayout->addWidget(m_fadingStopEnabled, row, 0);
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

    m_crossfadeAutoSwitchPolicy->addItem(tr("Overlap start"),
                                         static_cast<int>(Engine::CrossfadeSwitchPolicy::OverlapStart));
    m_crossfadeAutoSwitchPolicy->addItem(tr("Boundary"), static_cast<int>(Engine::CrossfadeSwitchPolicy::Boundary));
    m_crossfadeAutoSwitchPolicy->setToolTip(
        tr("Controls when the UI switches tracks during automatic crossfade transitions"));

    row = 0;
    crossmixLayout->addWidget(new QLabel(tr("Fade In"), this), row, 1);
    crossmixLayout->addWidget(new QLabel(tr("Fade Out"), this), row++, 2);

    crossmixLayout->addWidget(m_crossfadeSeekEnabled, row, 0);
    crossmixLayout->addWidget(m_crossfadeSeekIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeSeekOut, row++, 2);

    crossmixLayout->addWidget(m_crossfadeManualEnabled, row, 0);
    crossmixLayout->addWidget(m_crossfadeManualIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeManualOut, row++, 2);

    crossmixLayout->addWidget(m_crossfadeAutoEnabled, row, 0);
    crossmixLayout->addWidget(m_crossfadeAutoIn, row, 1);
    crossmixLayout->addWidget(m_crossfadeAutoOut, row++, 2);
    crossmixLayout->addWidget(new QLabel(tr("Auto switch policy"), this), row, 0);
    crossmixLayout->addWidget(m_crossfadeAutoSwitchPolicy, row++, 1, 1, 2);
    crossmixLayout->setColumnStretch(3, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(new QLabel(tr("Output") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_outputBox, row++, 1);
    mainLayout->addWidget(new QLabel(tr("Device") + u":"_s, this), row, 0);
    mainLayout->addWidget(m_deviceBox, row++, 1);
    mainLayout->addWidget(generalBox, row++, 0, 1, 2);
    mainLayout->addWidget(bufferBox, row++, 0, 1, 2);
    mainLayout->addWidget(m_fadingBox, row++, 0, 1, 2);
    mainLayout->addWidget(m_crossfadeBox, row++, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto syncBufferBounds = [this]() {
        const bool fading    = m_fadingBox->isChecked();
        const bool crossfade = m_crossfadeBox->isChecked();

        auto enabledValue = [](bool groupOn, QAbstractButton* enabled, QSpinBox* spin) {
            return (groupOn && enabled->isChecked()) ? spin->value() : 0;
        };

        const std::array<int, 11> values{
            MinBufferSize,
            enabledValue(fading, m_fadingPauseEnabled, m_fadingPauseIn),
            enabledValue(fading, m_fadingPauseEnabled, m_fadingPauseOut),
            enabledValue(fading, m_fadingStopEnabled, m_fadingStopIn),
            enabledValue(fading, m_fadingStopEnabled, m_fadingStopOut),
            enabledValue(crossfade, m_crossfadeManualEnabled, m_crossfadeManualIn),
            enabledValue(crossfade, m_crossfadeManualEnabled, m_crossfadeManualOut),
            enabledValue(crossfade, m_crossfadeAutoEnabled, m_crossfadeAutoIn),
            enabledValue(crossfade, m_crossfadeAutoEnabled, m_crossfadeAutoOut),
            enabledValue(crossfade, m_crossfadeSeekEnabled, m_crossfadeSeekIn),
            enabledValue(crossfade, m_crossfadeSeekEnabled, m_crossfadeSeekOut),
        };

        const int minBufferSize = std::ranges::max(values);

        m_bufferSize->setMinimum(minBufferSize);
        if(m_bufferSize->value() < minBufferSize) {
            m_bufferSize->setValue(minBufferSize);
        }
    };

    auto updateRowStates = [this]() {
        m_fadingPauseIn->setEnabled(m_fadingPauseEnabled->isChecked());
        m_fadingPauseOut->setEnabled(m_fadingPauseEnabled->isChecked());
        m_pauseFadeCurve->setEnabled(m_fadingPauseEnabled->isChecked());
        m_fadingStopIn->setEnabled(m_fadingStopEnabled->isChecked());
        m_fadingStopOut->setEnabled(m_fadingStopEnabled->isChecked());
        m_stopFadeCurve->setEnabled(m_fadingStopEnabled->isChecked());

        m_crossfadeSeekIn->setEnabled(m_crossfadeSeekEnabled->isChecked());
        m_crossfadeSeekOut->setEnabled(m_crossfadeSeekEnabled->isChecked());
        m_crossfadeManualIn->setEnabled(m_crossfadeManualEnabled->isChecked());
        m_crossfadeManualOut->setEnabled(m_crossfadeManualEnabled->isChecked());
        m_crossfadeAutoIn->setEnabled(m_crossfadeAutoEnabled->isChecked());
        m_crossfadeAutoOut->setEnabled(m_crossfadeAutoEnabled->isChecked());
        m_crossfadeAutoSwitchPolicy->setEnabled(m_crossfadeAutoEnabled->isChecked());
    };

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
        m_decodeWatermarkHint->setText(tr("Low %1 ms, High %2 ms").arg(lowMs).arg(highMs));
    };

    const auto updateState = [syncBufferBounds, updateRowStates]() {
        updateRowStates();
        syncBufferBounds();
    };

    QObject::connect(m_outputBox, &QComboBox::currentTextChanged, this, &OutputPageWidget::setupDevices);
    QObject::connect(m_fadingPauseIn, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_fadingPauseOut, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_fadingStopIn, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_fadingStopOut, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_crossfadeManualIn, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_crossfadeManualOut, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_crossfadeAutoIn, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_crossfadeAutoOut, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_crossfadeSeekIn, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_crossfadeSeekOut, &QSpinBox::valueChanged, this, [syncBufferBounds]() { syncBufferBounds(); });
    QObject::connect(m_bufferSize, &QSpinBox::valueChanged, this, [updateWatermarkHint]() { updateWatermarkHint(); });

    QObject::connect(m_fadingPauseEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_fadingStopEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_crossfadeSeekEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_crossfadeManualEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_crossfadeAutoEnabled, &QCheckBox::toggled, this, [updateState]() { updateState(); });
    QObject::connect(m_decodeLowWatermark, &QSpinBox::valueChanged, this,
                     [syncWatermarkRatioBounds]() { syncWatermarkRatioBounds(); });
    QObject::connect(m_decodeHighWatermark, &QSpinBox::valueChanged, this, [this](int value) {
        if(value < m_decodeLowWatermark->value()) {
            m_decodeLowWatermark->setValue(value);
        }
    });
    QObject::connect(m_decodeLowWatermark, &QSpinBox::valueChanged, this,
                     [updateWatermarkHint]() { updateWatermarkHint(); });
    QObject::connect(m_decodeHighWatermark, &QSpinBox::valueChanged, this,
                     [updateWatermarkHint]() { updateWatermarkHint(); });

    updateRowStates();
    syncBufferBounds();
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

    m_fadingBox->setChecked(m_settings->value<Settings::Core::Internal::EngineFading>());
    const auto fadingValues = m_settings->value<Settings::Core::Internal::FadingValues>().value<Engine::FadingValues>();
    const auto loadFadeSpec = [](const Engine::FadeSpec& spec, QCheckBox* enabled, QSpinBox* in, QSpinBox* out,
                                 QComboBox* curve = nullptr) {
        enabled->setChecked(spec.enabled);
        in->setValue(spec.in);
        out->setValue(spec.out);
        if(curve) {
            curve->setCurrentIndex(static_cast<int>(spec.curve));
        }
    };

    loadFadeSpec(fadingValues.pause, m_fadingPauseEnabled, m_fadingPauseIn, m_fadingPauseOut, m_pauseFadeCurve);
    loadFadeSpec(fadingValues.stop, m_fadingStopEnabled, m_fadingStopIn, m_fadingStopOut, m_stopFadeCurve);

    m_crossfadeBox->setChecked(m_settings->value<Settings::Core::Internal::EngineCrossfading>());
    const auto crossfadingValues
        = m_settings->value<Settings::Core::Internal::CrossfadingValues>().value<Engine::CrossfadingValues>();

    loadFadeSpec(crossfadingValues.manualChange, m_crossfadeManualEnabled, m_crossfadeManualIn, m_crossfadeManualOut);
    loadFadeSpec(crossfadingValues.autoChange, m_crossfadeAutoEnabled, m_crossfadeAutoIn, m_crossfadeAutoOut);
    loadFadeSpec(crossfadingValues.seek, m_crossfadeSeekEnabled, m_crossfadeSeekIn, m_crossfadeSeekOut);

    const auto policy = static_cast<Engine::CrossfadeSwitchPolicy>(
        m_settings->value<Settings::Core::Internal::CrossfadeSwitchPolicy>());
    const int policyIndex
        = m_crossfadeAutoSwitchPolicy->findData(static_cast<int>(policy), Qt::UserRole, Qt::MatchExactly);
    m_crossfadeAutoSwitchPolicy->setCurrentIndex(policyIndex >= 0 ? policyIndex : 0);
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

    const auto saveFadeSpec = [](Engine::FadeSpec& spec, const QCheckBox* enabled, const QSpinBox* in,
                                 const QSpinBox* out, const QComboBox* curve = nullptr) {
        spec.enabled = enabled->isChecked();
        spec.in      = in->value();
        spec.out     = out->value();
        if(curve) {
            spec.curve = static_cast<Engine::FadeCurve>(curve->currentData().toInt());
        }
    };

    Engine::FadingValues fadingValues;
    saveFadeSpec(fadingValues.pause, m_fadingPauseEnabled, m_fadingPauseIn, m_fadingPauseOut, m_pauseFadeCurve);
    saveFadeSpec(fadingValues.stop, m_fadingStopEnabled, m_fadingStopIn, m_fadingStopOut, m_stopFadeCurve);

    Engine::CrossfadingValues crossfadingValues;
    saveFadeSpec(crossfadingValues.manualChange, m_crossfadeManualEnabled, m_crossfadeManualIn, m_crossfadeManualOut);
    saveFadeSpec(crossfadingValues.autoChange, m_crossfadeAutoEnabled, m_crossfadeAutoIn, m_crossfadeAutoOut);
    saveFadeSpec(crossfadingValues.seek, m_crossfadeSeekEnabled, m_crossfadeSeekIn, m_crossfadeSeekOut);

    m_settings->set<Settings::Core::Internal::EngineFading>(m_fadingBox->isChecked());
    m_settings->set<Settings::Core::Internal::FadingValues>(QVariant::fromValue(fadingValues));
    m_settings->set<Settings::Core::Internal::EngineCrossfading>(m_crossfadeBox->isChecked());
    m_settings->set<Settings::Core::Internal::CrossfadingValues>(QVariant::fromValue(crossfadingValues));
    m_settings->set<Settings::Core::Internal::CrossfadeSwitchPolicy>(
        m_crossfadeAutoSwitchPolicy->currentData().toInt());
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
    m_settings->reset<Settings::Core::Internal::EngineFading>();
    m_settings->reset<Settings::Core::Internal::FadingValues>();
    m_settings->reset<Settings::Core::Internal::EngineCrossfading>();
    m_settings->reset<Settings::Core::Internal::CrossfadingValues>();
    m_settings->reset<Settings::Core::Internal::CrossfadeSwitchPolicy>();
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
