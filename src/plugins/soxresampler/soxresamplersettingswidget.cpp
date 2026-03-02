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

#include "soxresamplersettingswidget.h"

#include "soxresamplerdsp.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>

#include <algorithm>
#include <array>
#include <cmath>

using namespace Qt::StringLiterals;

constexpr auto PassbandMinPercent = 850;
constexpr auto PassbandMaxPercent = 999;

namespace {
constexpr std::array<int, 15> SampleRateOptions = {
    8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 176400, 192000, 352800, 384000,
};
} // namespace

namespace Fooyin::Soxr {
SoxResamplerSettingsWidget::SoxResamplerSettingsWidget(QWidget* parent)
    : DspSettingsDialog{parent}
    , m_rateCombo{new QComboBox(this)}
    , m_qualityCombo{new QComboBox(this)}
    , m_passbandSlider{new QSlider(Qt::Horizontal, this)}
    , m_passbandValueLabel{new QLabel(this)}
    , m_fullBandwidthCheck{new QCheckBox(tr("Allow imaging on upsampling"), this)}
    , m_phaseResponseCombo{new QComboBox(this)}
    , m_filterModeCombo{new QComboBox(this)}
    , m_filteredRatesEdit{new QLineEdit(this)}
{
    auto* layout = contentLayout();

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(6);

    for(const int sampleRate : SampleRateOptions) {
        m_rateCombo->addItem(QString::number(sampleRate), sampleRate);
    }

    auto* rateLayout = new QHBoxLayout();
    rateLayout->setContentsMargins(0, 0, 0, 0);
    rateLayout->setSpacing(6);
    rateLayout->addWidget(m_rateCombo);
    rateLayout->addWidget(new QLabel(tr("Hz"), this));
    rateLayout->addStretch();
    form->addRow(tr("Target sample rate") + u":"_s, rateLayout);

    m_qualityCombo->addItem(tr("Very High (168 dB)"), static_cast<int>(SoxResamplerDSP::Quality::VeryHigh));
    m_qualityCombo->addItem(tr("High (120 dB)"), static_cast<int>(SoxResamplerDSP::Quality::High));
    m_qualityCombo->addItem(tr("Medium (108 dB)"), static_cast<int>(SoxResamplerDSP::Quality::Medium));
    m_qualityCombo->addItem(tr("Low (96 dB)"), static_cast<int>(SoxResamplerDSP::Quality::Low));
    m_qualityCombo->addItem(tr("Quick (90 dB)"), static_cast<int>(SoxResamplerDSP::Quality::Quick));
    form->addRow(tr("Quality") + u":"_s, m_qualityCombo);

    m_passbandSlider->setRange(PassbandMinPercent, PassbandMaxPercent);
    m_passbandSlider->setSingleStep(1);
    m_passbandSlider->setPageStep(5);
    m_passbandSlider->setTickPosition(QSlider::TicksBelow);
    m_passbandSlider->setTickInterval(25);

    auto* passbandLayout = new QHBoxLayout();
    passbandLayout->setContentsMargins(0, 0, 0, 0);
    passbandLayout->setSpacing(6);
    passbandLayout->addWidget(m_passbandSlider);
    passbandLayout->addWidget(m_passbandValueLabel);
    form->addRow(tr("Bandwidth") + u":"_s, passbandLayout);

    form->addRow(QString{}, m_fullBandwidthCheck);

    m_phaseResponseCombo->addItem(tr("Linear"), static_cast<int>(SoxResamplerDSP::PhaseResponse::Linear));
    m_phaseResponseCombo->addItem(tr("Intermediate"), static_cast<int>(SoxResamplerDSP::PhaseResponse::Intermediate));
    m_phaseResponseCombo->addItem(tr("Minimum"), static_cast<int>(SoxResamplerDSP::PhaseResponse::Minimum));
    form->addRow(tr("Phase response") + u":"_s, m_phaseResponseCombo);

    m_filterModeCombo->addItem(tr("Exclude these sample rates") + u":"_s,
                               static_cast<int>(SoxResamplerDSP::SampleRateFilterMode::ExcludeListed));
    m_filterModeCombo->addItem(tr("Resample only these sample rates") + u":"_s,
                               static_cast<int>(SoxResamplerDSP::SampleRateFilterMode::OnlyListed));
    form->addRow(tr("Rate filtering") + u":"_s, m_filterModeCombo);

    m_filteredRatesEdit->setPlaceholderText(tr("e.g. 44100, 48000, 96000"));
    form->addRow(tr("Filtered rates") + u":"_s, m_filteredRatesEdit);

    layout->addLayout(form);

    QObject::connect(m_passbandSlider, &QSlider::valueChanged, this, [this](int /*value*/) { updatePassbandLabel(); });
}

void SoxResamplerSettingsWidget::loadSettings(const QByteArray& settings)
{
    SoxResamplerDSP dsp;
    dsp.loadSettings(settings);

    int rateIndex = m_rateCombo->findData(dsp.targetSampleRate());
    if(rateIndex < 0) {
        m_rateCombo->addItem(QString::number(dsp.targetSampleRate()), dsp.targetSampleRate());
        rateIndex = m_rateCombo->count() - 1;
    }
    m_rateCombo->setCurrentIndex(std::max(rateIndex, 0));

    const int qualityIndex = m_qualityCombo->findData(static_cast<int>(dsp.quality()));
    m_qualityCombo->setCurrentIndex(std::max(qualityIndex, 0));

    m_fullBandwidthCheck->setChecked(dsp.fullBandwidthUpsampling());
    const int passbandTenths = static_cast<int>(std::lround(dsp.passbandPercent() * 10.0));
    m_passbandSlider->setValue(passbandTenths);

    const int phaseIndex = m_phaseResponseCombo->findData(static_cast<int>(dsp.phaseResponse()));
    m_phaseResponseCombo->setCurrentIndex(std::max(phaseIndex, 0));

    const int filterModeIndex = m_filterModeCombo->findData(static_cast<int>(dsp.sampleRateFilterMode()));
    m_filterModeCombo->setCurrentIndex(std::max(filterModeIndex, 0));

    m_filteredRatesEdit->setText(dsp.excludedSampleRatesText());
    updatePassbandLabel();
}

QByteArray SoxResamplerSettingsWidget::saveSettings() const
{
    SoxResamplerDSP dsp;

    dsp.setTargetSampleRate(m_rateCombo->currentData().toInt());

    const auto quality = static_cast<SoxResamplerDSP::Quality>(m_qualityCombo->currentData().toInt());
    dsp.setQuality(quality);

    dsp.setFullBandwidthUpsampling(m_fullBandwidthCheck->isChecked());
    dsp.setPassbandPercent(static_cast<double>(m_passbandSlider->value()) / 10.0);

    const auto phase = static_cast<SoxResamplerDSP::PhaseResponse>(m_phaseResponseCombo->currentData().toInt());
    dsp.setPhaseResponse(phase);

    const auto filterMode
        = static_cast<SoxResamplerDSP::SampleRateFilterMode>(m_filterModeCombo->currentData().toInt());
    dsp.setSampleRateFilterMode(filterMode);
    dsp.setExcludedSampleRatesText(m_filteredRatesEdit->text().trimmed());

    return dsp.saveSettings();
}

void SoxResamplerSettingsWidget::restoreDefaults()
{
    loadSettings(QByteArray{});
}

void SoxResamplerSettingsWidget::updatePassbandLabel()
{
    const double value = static_cast<double>(m_passbandSlider->value()) / 10.0;
    m_passbandValueLabel->setText(QString::number(value, 'f', 2) + tr(" %"));
}

QString SoxResamplerSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.soxresampler");
}

DspSettingsDialog* SoxResamplerSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SoxResamplerSettingsWidget(parent);
}
} // namespace Fooyin::Soxr
