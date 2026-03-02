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

#include "equalisersettingswidget.h"

#include "equaliserdsp.h"

#include <core/coresettings.h>
#include <gui/widgets/tooltip.h>

#include <QComboBox>
#include <QDataStream>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpacerItem>
#include <QTextStream>
#include <QTimerEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

constexpr auto SliderScale           = 10;
constexpr quint32 PresetStoreVersion = 1;
constexpr auto PresetsSettingKey     = "DSP/EqualiserPresets";
constexpr auto LastPresetPathKey     = "DSP/EqualiserLastPresetPath";

namespace {
constexpr std::array<const char*, 18> BandLabels = {
    "55 Hz",   "77 Hz",   "110 Hz",  "156 Hz",  "220 Hz", "311 Hz", "440 Hz", "622 Hz", "880 Hz",
    "1.2 kHz", "1.8 kHz", "2.5 kHz", "3.5 kHz", "5 kHz",  "7 kHz",  "10 kHz", "14 kHz", "20 kHz",
};

QSlider* makeGainSlider(QWidget* parent)
{
    auto* slider = new QSlider(Qt::Vertical, parent);

    slider->setRange(-20 * SliderScale, 20 * SliderScale);
    slider->setSingleStep(1);
    slider->setPageStep(5);
    slider->setTickInterval(50);
    slider->setTickPosition(QSlider::TicksLeft);
    slider->setMinimumHeight(220);
    slider->setMaximumHeight(220);

    return slider;
}

QLabel* makeBandLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);

    label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    return label;
}

QLabel* makeScaleLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);

    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    return label;
}
} // namespace

namespace Fooyin::Equaliser {
EqualiserSettingsWidget::EqualiserSettingsWidget(QWidget* parent)
    : DspSettingsDialog{parent}
    , m_presetBox{new QComboBox(this)}
    , m_loadPresetButton{new QPushButton(tr("Load"), this)}
    , m_savePresetButton{new QPushButton(tr("Save"), this)}
    , m_deletePresetButton{new QPushButton(tr("Delete"), this)}
    , m_importPresetButton{new QPushButton(tr("Import"), this)}
    , m_exportPresetButton{new QPushButton(tr("Export"), this)}
    , m_selectedBandCombo{new QComboBox(this)}
    , m_selectedBandSpin{new QDoubleSpinBox(this)}
    , m_preampSlider{makeGainSlider(this)}
    , m_bandSliders{}
{
    setWindowTitle(tr("Equaliser Settings"));

    setRestoreDefaultsVisible(false);

    auto* root = contentLayout();

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    const auto addSliderColumn = [this, row](QSlider* slider, const QString& labelText) {
        auto* col = new QVBoxLayout();
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(6);
        col->addWidget(slider, 1, Qt::AlignHCenter);
        col->addWidget(makeBandLabel(labelText, this));

        auto* colWidget = new QWidget(this);
        colWidget->setLayout(col);
        colWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        row->addWidget(colWidget, 1);
    };

    addSliderColumn(m_preampSlider, tr("Preamp"));
    QObject::connect(m_preampSlider, &QSlider::valueChanged, this, [this](int) {
        refreshTooltips();
        m_previewTimer.start(PreviewDebounceMs, this);
        if(m_preampSlider->isSliderDown()) {
            updateSliderToolTip(m_preampSlider);
        }
    });
    QObject::connect(m_preampSlider, &QSlider::sliderPressed, this, [this]() { updateSliderToolTip(m_preampSlider); });
    QObject::connect(m_preampSlider, &QSlider::sliderMoved, this, [this](int) { updateSliderToolTip(m_preampSlider); });
    QObject::connect(m_preampSlider, &QSlider::sliderReleased, this, [this]() { hideSliderToolTip(); });

    for(size_t i{0}; i < m_bandSliders.size(); ++i) {
        m_bandSliders[i] = makeGainSlider(this);
        addSliderColumn(m_bandSliders[i], tr(BandLabels[i]));
        QObject::connect(m_bandSliders[i], &QSlider::valueChanged, this, [this, i](int) {
            refreshTooltips();
            refreshSelectedBandEditor();
            m_previewTimer.start(PreviewDebounceMs, this);
            if(m_bandSliders[i]->isSliderDown()) {
                updateSliderToolTip(m_bandSliders[i]);
            }
        });
        QObject::connect(m_bandSliders[i], &QSlider::sliderPressed, this,
                         [this, i]() { updateSliderToolTip(m_bandSliders[i]); });
        QObject::connect(m_bandSliders[i], &QSlider::sliderMoved, this,
                         [this, i](int) { updateSliderToolTip(m_bandSliders[i]); });
        QObject::connect(m_bandSliders[i], &QSlider::sliderReleased, this, [this]() { hideSliderToolTip(); });
    }

    auto* scaleCol = new QVBoxLayout();
    scaleCol->setContentsMargins(0, 0, 0, 0);
    scaleCol->setSpacing(6);

    // Keep scale labels aligned to the slider travel only (220 px), so 0 dB
    // sits exactly at the visual midpoint of the slider range.
    auto* scaleTrackWidget = new QWidget(this);
    scaleTrackWidget->setFixedHeight(220);

    auto* trackLayout = new QVBoxLayout(scaleTrackWidget);
    trackLayout->setContentsMargins(0, 0, 0, 0);
    trackLayout->setSpacing(0);
    trackLayout->addWidget(makeScaleLabel(tr("+20 dB"), this), 0, Qt::AlignTop | Qt::AlignRight);
    trackLayout->addStretch(1);
    trackLayout->addWidget(makeScaleLabel(tr("+0 dB"), this), 0, Qt::AlignRight);
    trackLayout->addStretch(1);
    trackLayout->addWidget(makeScaleLabel(tr("-20 dB"), this), 0, Qt::AlignBottom | Qt::AlignRight);

    scaleCol->addWidget(scaleTrackWidget, 0, Qt::AlignTop | Qt::AlignRight);
    scaleCol->addSpacerItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Fixed));

    row->addSpacing(4);
    row->addLayout(scaleCol);
    root->addLayout(row);

    auto* controlsLayout = new QGridLayout();
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setHorizontalSpacing(6);
    controlsLayout->setVerticalSpacing(6);

    auto* zeroButton      = new QPushButton(tr("Zero all"), this);
    auto* autoButton      = new QPushButton(tr("Auto level"), this);
    auto* bandEditorLabel = new QLabel(tr("Band:"), this);
    auto* presetsLabel    = new QLabel(tr("Presets:"), this);

    QObject::connect(zeroButton, &QPushButton::clicked, this, [this]() { zeroAll(); });
    QObject::connect(autoButton, &QPushButton::clicked, this, [this]() { autoLevel(); });

    for(const auto& bandName : BandLabels) {
        m_selectedBandCombo->addItem(tr(bandName));
    }

    m_selectedBandSpin->setRange(-20.0, 20.0);
    m_selectedBandSpin->setSingleStep(0.1);
    m_selectedBandSpin->setDecimals(1);
    m_selectedBandSpin->setSuffix(tr(" dB"));

    m_presetBox->setEditable(true);
    m_presetBox->setMinimumContentsLength(24);
    m_presetBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_presetBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    controlsLayout->addWidget(zeroButton, 0, 0);
    controlsLayout->addWidget(autoButton, 0, 1);
    controlsLayout->addWidget(bandEditorLabel, 0, 2);
    controlsLayout->addWidget(m_selectedBandCombo, 0, 3);
    controlsLayout->addWidget(m_selectedBandSpin, 0, 4);
    controlsLayout->addWidget(m_importPresetButton, 0, 6);
    controlsLayout->addWidget(m_exportPresetButton, 0, 7);

    controlsLayout->addWidget(presetsLabel, 1, 0);
    controlsLayout->addWidget(m_presetBox, 1, 1, 1, 5);
    controlsLayout->addWidget(m_loadPresetButton, 1, 6);
    controlsLayout->addWidget(m_savePresetButton, 1, 7);
    controlsLayout->addWidget(m_deletePresetButton, 1, 8);

    controlsLayout->setColumnStretch(5, 1);
    root->addLayout(controlsLayout);

    QObject::connect(m_loadPresetButton, &QPushButton::clicked, this, [this]() { loadPreset(); });
    QObject::connect(m_savePresetButton, &QPushButton::clicked, this, [this]() { savePreset(); });
    QObject::connect(m_deletePresetButton, &QPushButton::clicked, this, [this]() { deletePreset(); });
    QObject::connect(m_importPresetButton, &QPushButton::clicked, this, [this]() { importPreset(); });
    QObject::connect(m_exportPresetButton, &QPushButton::clicked, this, [this]() { exportPreset(); });
    QObject::connect(m_selectedBandCombo, &QComboBox::currentIndexChanged, this,
                     [this](int) { refreshSelectedBandEditor(); });
    QObject::connect(m_selectedBandSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        const int bandIndex = m_selectedBandCombo->currentIndex();
        if(bandIndex < 0 || std::cmp_greater_equal(bandIndex, m_bandSliders.size())) {
            return;
        }
        m_bandSliders[static_cast<size_t>(bandIndex)]->setValue(gainDbToSliderValue(value));
    });
    QObject::connect(m_presetBox, &QComboBox::currentTextChanged, this, [this]() { updatePresetButtons(); });

    loadStoredPresets();
    refreshPresets();
    refreshSelectedBandEditor();
}

void EqualiserSettingsWidget::loadSettings(const QByteArray& settings)
{
    EqualiserDsp dsp;
    dsp.loadSettings(settings);

    m_preampSlider->setValue(gainDbToSliderValue(dsp.preampDb()));
    for(size_t i{0}; i < m_bandSliders.size(); ++i) {
        m_bandSliders[i]->setValue(gainDbToSliderValue(dsp.bandDb(static_cast<int>(i))));
    }

    refreshTooltips();
}

QByteArray EqualiserSettingsWidget::saveSettings() const
{
    EqualiserDsp dsp;
    dsp.setPreampDb(sliderValueToGainDb(m_preampSlider->value()));
    for(size_t i{0}; i < m_bandSliders.size(); ++i) {
        dsp.setBandDb(static_cast<int>(i), sliderValueToGainDb(m_bandSliders[i]->value()));
    }

    return dsp.saveSettings();
}

void EqualiserSettingsWidget::restoreDefaults()
{
    zeroAll();
}

void EqualiserSettingsWidget::loadStoredPresets()
{
    m_presets.clear();

    const FySettings settings;
    auto serializedData = settings.value(QLatin1String(PresetsSettingKey)).toByteArray();
    if(serializedData.isEmpty()) {
        return;
    }

    serializedData = qUncompress(serializedData);
    if(serializedData.isEmpty()) {
        return;
    }

    QDataStream stream{&serializedData, QIODevice::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 version{0};
    qint32 presetCount{0};
    stream >> version;
    stream >> presetCount;

    if(stream.status() != QDataStream::Ok || version != PresetStoreVersion || presetCount < 0) {
        return;
    }

    for(qint32 i{0}; i < presetCount; ++i) {
        PresetItem preset;
        stream >> preset.name;
        stream >> preset.settings;

        preset.name = preset.name.trimmed();
        if(stream.status() != QDataStream::Ok || preset.name.isEmpty() || preset.settings.isEmpty()) {
            m_presets.clear();
            return;
        }

        if(presetIndexByName(preset.name) < 0) {
            m_presets.push_back(std::move(preset));
        }
    }
}

void EqualiserSettingsWidget::saveStoredPresets() const
{
    FySettings settings;

    if(m_presets.empty()) {
        settings.remove(QLatin1String(PresetsSettingKey));
        return;
    }

    QByteArray serializedData;
    QDataStream stream{&serializedData, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << quint32(PresetStoreVersion);
    stream << static_cast<qint32>(m_presets.size());

    for(const auto& preset : m_presets) {
        stream << preset.name;
        stream << preset.settings;
    }

    settings.setValue(QLatin1String(PresetsSettingKey), qCompress(serializedData, 9));
}

int EqualiserSettingsWidget::presetIndexByName(const QString& name) const
{
    const QString trimmedName = name.trimmed();
    if(trimmedName.isEmpty()) {
        return -1;
    }

    for(size_t i{0}; i < m_presets.size(); ++i) {
        if(m_presets[i].name == trimmedName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void EqualiserSettingsWidget::refreshPresets()
{
    const QString currentText = m_presetBox->currentText().trimmed();

    m_presetBox->clear();
    for(const auto& preset : m_presets) {
        m_presetBox->addItem(preset.name);
    }

    if(!currentText.isEmpty()) {
        const int idx = m_presetBox->findText(currentText);
        if(idx >= 0) {
            m_presetBox->setCurrentIndex(idx);
        }
        else {
            m_presetBox->setEditText(currentText);
        }
    }

    updatePresetButtons();
}

void EqualiserSettingsWidget::updatePresetButtons()
{
    const QString name   = m_presetBox->currentText().trimmed();
    const bool hasPreset = presetIndexByName(name) >= 0;

    m_loadPresetButton->setEnabled(hasPreset);
    m_deletePresetButton->setEnabled(hasPreset);
    m_savePresetButton->setEnabled(!name.isEmpty());
}

void EqualiserSettingsWidget::loadPreset()
{
    const int presetIndex = presetIndexByName(m_presetBox->currentText());
    if(presetIndex < 0) {
        return;
    }

    EqualiserDsp dsp;
    if(!dsp.loadSettings(m_presets.at(static_cast<size_t>(presetIndex)).settings)) {
        QMessageBox::warning(this, tr("Presets"), tr("Unable to load the selected preset."));
        return;
    }

    m_preampSlider->setValue(gainDbToSliderValue(dsp.preampDb()));
    for(size_t i{0}; i < m_bandSliders.size(); ++i) {
        m_bandSliders[i]->setValue(gainDbToSliderValue(dsp.bandDb(static_cast<int>(i))));
    }

    refreshTooltips();
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserSettingsWidget::savePreset()
{
    const QString name = m_presetBox->currentText().trimmed();
    if(name.isEmpty()) {
        return;
    }

    const int existingIndex = presetIndexByName(name);
    if(existingIndex >= 0) {
        QMessageBox msg{QMessageBox::Question, tr("Preset already exists"),
                        tr("Preset \"%1\" already exists. Overwrite?").arg(name), QMessageBox::Yes | QMessageBox::No};
        if(msg.exec() != QMessageBox::Yes) {
            return;
        }

        m_presets[static_cast<size_t>(existingIndex)].settings = saveSettings();
    }
    else {
        PresetItem preset;
        preset.name     = name;
        preset.settings = saveSettings();
        m_presets.push_back(std::move(preset));
    }

    saveStoredPresets();
    refreshPresets();
    m_presetBox->setCurrentText(name);
}

void EqualiserSettingsWidget::deletePreset()
{
    const int presetIndex = presetIndexByName(m_presetBox->currentText());
    if(presetIndex < 0) {
        return;
    }

    m_presets.erase(m_presets.begin() + presetIndex);
    saveStoredPresets();
    refreshPresets();
}

void EqualiserSettingsWidget::importPreset()
{
    QSettings settings;
    const QString initialPath = settings.value(QLatin1String(LastPresetPathKey), QDir::homePath()).toString();

    const QString filePath = QFileDialog::getOpenFileName(this, tr("Import Equaliser Preset"), initialPath,
                                                          tr("Equaliser Preset (*.feq)"));
    if(filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import Equaliser Preset"),
                             tr("Unable to open \"%1\" for reading.").arg(filePath));
        return;
    }

    std::array<int, EqualiserDsp::BandCount> gains{};
    int parsedBands{0};
    int lineNumber{0};

    QTextStream stream(&file);
    while(!stream.atEnd() && parsedBands < EqualiserDsp::BandCount) {
        const QString line = stream.readLine();
        ++lineNumber;

        const QString trimmed = line.trimmed();
        if(trimmed.isEmpty()) {
            continue;
        }

        bool ok{false};
        const int value = trimmed.toInt(&ok);
        if(!ok) {
            QMessageBox::warning(this, tr("Import Equaliser Preset"),
                                 tr("Invalid value on line %1. The first %2 non-empty lines must be integers.")
                                     .arg(lineNumber)
                                     .arg(EqualiserDsp::BandCount));
            return;
        }

        gains[static_cast<size_t>(parsedBands)] = value;
        ++parsedBands;
    }

    if(parsedBands < EqualiserDsp::BandCount) {
        QMessageBox::warning(this, tr("Import Equaliser Preset"),
                             tr("Preset file has %1 band values. %2 values are required.")
                                 .arg(parsedBands)
                                 .arg(EqualiserDsp::BandCount));
        return;
    }

    for(size_t i{0}; i < m_bandSliders.size(); ++i) {
        m_bandSliders[i]->setValue(gainDbToSliderValue(static_cast<double>(gains[i])));
    }

    settings.setValue(QLatin1String(LastPresetPathKey), QFileInfo(filePath).absolutePath());

    refreshTooltips();
    m_previewTimer.start(PreviewDebounceMs, this);
}

void EqualiserSettingsWidget::exportPreset()
{
    FySettings settings;
    const QString initialPath = settings.value(QLatin1String(LastPresetPathKey), QDir::homePath()).toString();

    QString filePath = QFileDialog::getSaveFileName(this, tr("Export Equaliser Preset"), initialPath,
                                                    tr("Equaliser Preset (*.feq)"));
    if(filePath.isEmpty()) {
        return;
    }

    if(!filePath.endsWith(QStringLiteral(".feq"), Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".feq");
    }

    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export Equaliser Preset"),
                             tr("Unable to open \"%1\" for writing.").arg(filePath));
        return;
    }

    QTextStream stream(&file);
    for(auto* slider : m_bandSliders) {
        const int value = static_cast<int>(std::lround(sliderValueToGainDb(slider->value())));
        stream << value << '\n';
    }

    if(stream.status() != QTextStream::Ok) {
        QMessageBox::warning(this, tr("Export Equaliser Preset"),
                             tr("An error occurred while writing \"%1\".").arg(filePath));
        return;
    }

    settings.setValue(QLatin1String(LastPresetPathKey), QFileInfo(filePath).absolutePath());
}

void EqualiserSettingsWidget::zeroAll()
{
    m_preampSlider->setValue(0);

    for(auto* slider : m_bandSliders) {
        slider->setValue(0);
    }

    refreshTooltips();
}

void EqualiserSettingsWidget::autoLevel()
{
    double maxBandDb{0.0};

    for(auto* slider : m_bandSliders) {
        maxBandDb = std::max(maxBandDb, sliderValueToGainDb(slider->value()));
    }

    if(maxBandDb <= 0.0) {
        refreshTooltips();
        return;
    }

    for(auto* slider : m_bandSliders) {
        const double band = sliderValueToGainDb(slider->value());
        slider->setValue(gainDbToSliderValue(band - maxBandDb));
    }

    refreshTooltips();
}

int EqualiserSettingsWidget::gainDbToSliderValue(const double gainDb)
{
    const double clamped = std::clamp(gainDb, -20.0, 20.0);
    return static_cast<int>(std::lround(clamped * static_cast<double>(SliderScale)));
}

double EqualiserSettingsWidget::sliderValueToGainDb(const int sliderValue)
{
    return std::clamp(static_cast<double>(sliderValue) / static_cast<double>(SliderScale), -20.0, 20.0);
}

QString EqualiserSettingsWidget::gainTooltip(const double gainDb)
{
    const QString prefix = gainDb >= 0.0 ? QStringLiteral("+") : QString{};
    return prefix + QString::number(gainDb, 'f', 1) + tr(" dB");
}

void EqualiserSettingsWidget::refreshTooltips()
{
    m_preampSlider->setToolTip(gainTooltip(sliderValueToGainDb(m_preampSlider->value())));

    for(auto* slider : m_bandSliders) {
        slider->setToolTip(gainTooltip(sliderValueToGainDb(slider->value())));
    }
}

void EqualiserSettingsWidget::updateSliderToolTip(QSlider* slider)
{
    if(!slider) {
        return;
    }

    if(!m_sliderToolTip) {
        m_sliderToolTip = new ToolTip(this);
        m_sliderToolTip->show();
    }

    m_sliderToolTip->setText(gainTooltip(sliderValueToGainDb(slider->value())));
    const QPoint cursorPos = slider->mapFromGlobal(QCursor::pos());
    const int handleY      = std::clamp(cursorPos.y(), slider->rect().top(), slider->rect().bottom());
    const QPoint handlePos = slider->mapTo(this, QPoint(slider->rect().center().x(), handleY));

    const int tipHeight = m_sliderToolTip->height();
    const int tipWidth  = m_sliderToolTip->width();
    const int anchorY   = std::clamp(handlePos.y() + (tipHeight / 2), tipHeight, height());

    const QPoint rightAnchor = slider->mapTo(this, QPoint(slider->rect().right() + 6, anchorY));
    const QPoint leftAnchor  = slider->mapTo(this, QPoint(slider->rect().left() - 6, anchorY));

    if(rightAnchor.x() + tipWidth <= width()) {
        m_sliderToolTip->setPosition(rightAnchor, Qt::AlignLeft);
    }
    else {
        m_sliderToolTip->setPosition(leftAnchor, Qt::AlignRight);
    }
}

void EqualiserSettingsWidget::hideSliderToolTip()
{
    if(m_sliderToolTip) {
        m_sliderToolTip->hide();
    }
}

void EqualiserSettingsWidget::refreshSelectedBandEditor()
{
    const int bandIndex  = m_selectedBandCombo->currentIndex();
    const bool validBand = (bandIndex >= 0 && std::cmp_less(bandIndex, m_bandSliders.size()));

    m_selectedBandSpin->setEnabled(validBand);
    if(!validBand) {
        return;
    }

    const int sliderValue = m_bandSliders[static_cast<size_t>(bandIndex)]->value();
    const QSignalBlocker blockSpin(m_selectedBandSpin);
    m_selectedBandSpin->setValue(sliderValueToGainDb(sliderValue));
}

void EqualiserSettingsWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();
        publishPreviewSettings();
        return;
    }

    DspSettingsDialog::timerEvent(event);
}

QString EqualiserSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.equaliser");
}

DspSettingsDialog* EqualiserSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new EqualiserSettingsWidget(parent);
}
} // namespace Fooyin::Equaliser
