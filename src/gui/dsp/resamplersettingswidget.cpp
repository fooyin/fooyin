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

#include "resamplersettingswidget.h"

#include "core/engine/dsp/resamplerdsp.h"
#include "core/engine/dsp/resamplersettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>

#include <algorithm>

using namespace Qt::StringLiterals;

constexpr auto SampleRates = {
    8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 176400, 192000, 352800, 384000,
};

namespace Fooyin {
ResamplerSettingsWidget::ResamplerSettingsWidget(QWidget* parent)
    : DspSettingsDialog{parent}
    , m_rateCombo{new QComboBox(this)}
    , m_soxrCheck{new QCheckBox(QObject::tr("Prefer SoXR engine (if available)"), this)}
    , m_soxrPrecisionLabel{new QLabel(QObject::tr("SoXR precision") + u":"_s, this)}
    , m_soxrPrecisionCombo{new QComboBox(this)}
    , m_filterModeCombo{new QComboBox(this)}
    , m_filteredRatesEdit{new QLineEdit(this)}
    , m_precisionPresets{{{.precision = 28.0, .label = tr("Very High (28-bit)")},
                          {.precision = 20.0, .label = tr("High (20-bit)")},
                          {.precision = 18.0, .label = tr("Medium (18-bit)")},
                          {.precision = 16.0, .label = tr("Low (16-bit)")},
                          {.precision = 15.0, .label = tr("Quick (15-bit)")}}}
{
    auto* layout = contentLayout();

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);

    for(const int sampleRate : SampleRates) {
        m_rateCombo->addItem(QString::number(sampleRate), sampleRate);
    }

    auto* rateLayout = new QHBoxLayout();
    rateLayout->setContentsMargins(0, 0, 0, 0);
    rateLayout->setSpacing(6);

    rateLayout->addWidget(m_rateCombo);
    rateLayout->addWidget(new QLabel(tr("Hz"), this));
    rateLayout->addStretch();

    form->addRow(QObject::tr("Target sample rate") + u":"_s, rateLayout);
    form->addRow(QString{}, m_soxrCheck);

    for(const auto& preset : m_precisionPresets) {
        m_soxrPrecisionCombo->addItem(preset.label, preset.precision);
    }

    form->addRow(m_soxrPrecisionLabel, m_soxrPrecisionCombo);

    m_filterModeCombo->addItem(tr("Exclude these sample rates") + u":"_s,
                               static_cast<int>(ResamplerSettings::SampleRateFilterMode::ExcludeListed));
    m_filterModeCombo->addItem(tr("Resample only these sample rates") + u":"_s,
                               static_cast<int>(ResamplerSettings::SampleRateFilterMode::OnlyListed));
    m_filteredRatesEdit->setPlaceholderText(tr("e.g. 44100, 48000, 96000"));

    form->addRow(tr("Rate filtering:"), m_filterModeCombo);
    form->addRow(tr("Filtered rates:"), m_filteredRatesEdit);

    layout->addLayout(form);

    QObject::connect(m_soxrCheck, &QCheckBox::toggled, this, &ResamplerSettingsWidget::updateSoxrControls);
}

void ResamplerSettingsWidget::loadSettings(const QByteArray& settings)
{
    ResamplerDsp dsp;
    dsp.loadSettings(settings);

    int rateIndex = m_rateCombo->findData(dsp.targetSampleRate());
    if(rateIndex < 0) {
        m_rateCombo->addItem(QString::number(dsp.targetSampleRate()), dsp.targetSampleRate());
        rateIndex = m_rateCombo->count() - 1;
    }
    m_rateCombo->setCurrentIndex(std::max(rateIndex, 0));

    m_soxrCheck->setChecked(dsp.useSoxr());

    int precisionIndex = m_soxrPrecisionCombo->findData(dsp.soxrPrecision());
    if(precisionIndex < 0) {
        precisionIndex = m_soxrPrecisionCombo->findData(ResamplerSettings::DefaultSoxrPrecision);
    }
    m_soxrPrecisionCombo->setCurrentIndex(std::max(precisionIndex, 0));

    int filterModeIndex = m_filterModeCombo->findData(static_cast<int>(dsp.sampleRateFilterMode()));
    m_filterModeCombo->setCurrentIndex(std::max(filterModeIndex, 0));
    m_filteredRatesEdit->setText(dsp.filteredRatesText());

    updateSoxrControls(dsp.useSoxr());
}

QByteArray ResamplerSettingsWidget::saveSettings() const
{
    ResamplerDsp dsp;
    dsp.setTargetSampleRate(m_rateCombo->currentData().toInt());
    dsp.setUseSoxr(m_soxrCheck->isChecked());

    double soxrPrecision     = ResamplerSettings::DefaultSoxrPrecision;
    const int precisionIndex = m_soxrPrecisionCombo->currentIndex();
    if(precisionIndex >= 0) {
        soxrPrecision = m_soxrPrecisionCombo->itemData(precisionIndex).toDouble();
    }
    dsp.setSoxrPrecision(soxrPrecision);

    const auto filterMode
        = static_cast<ResamplerSettings::SampleRateFilterMode>(m_filterModeCombo->currentData().toInt());
    dsp.setSampleRateFilterMode(filterMode);
    dsp.setFilteredRatesText(m_filteredRatesEdit->text().trimmed());

    return dsp.saveSettings();
}

void ResamplerSettingsWidget::restoreDefaults()
{
    loadSettings(QByteArray{});
}

void ResamplerSettingsWidget::updateSoxrControls(bool enabled)
{
    m_soxrPrecisionLabel->setEnabled(enabled);
    m_soxrPrecisionCombo->setEnabled(enabled);
}

QString ResamplerSettingsProvider::id() const
{
    return u"fooyin.dsp.resampler"_s;
}

DspSettingsDialog* ResamplerSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new ResamplerSettingsWidget(parent);
}
} // namespace Fooyin
