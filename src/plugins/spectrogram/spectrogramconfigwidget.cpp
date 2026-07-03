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

#include "spectrogramconfigwidget.h"

#include <gui/widgets/gradienteditor.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>

using namespace Qt::StringLiterals;

namespace Fooyin::Spectrogram {
SpectrogramConfigDialog::SpectrogramConfigDialog(SpectrogramWidget* widget, QWidget* parent)
    : WidgetConfigDialog{widget, tr("Spectrogram Settings"), parent}
    , m_logFrequency{new QCheckBox(tr("Use logarithmic frequency scale"), this)}
    , m_clearOnTrackChange{new QCheckBox(tr("Clear on track change"), this)}
    , m_channelMode{new QComboBox(this)}
    , m_presentationMode{new QComboBox(this)}
    , m_channelSpacing{new QSpinBox(this)}
    , m_fftSize{new QComboBox(this)}
    , m_windowFunction{new QComboBox(this)}
    , m_minDb{new QSpinBox(this)}
    , m_maxDb{new QSpinBox(this)}
    , m_colours{new GradientEditor(this)}
{
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    for(const int fftSize : SpectrogramWidget::FftSizes) {
        m_fftSize->addItem(QString::number(fftSize), fftSize);
    }

    m_channelMode->addItem(tr("Unchanged"), static_cast<int>(SpectrogramWidget::ChannelMode::Unchanged));
    m_channelMode->addItem(tr("Mono"), static_cast<int>(SpectrogramWidget::ChannelMode::Mono));
    m_channelMode->addItem(tr("Front Only"), static_cast<int>(SpectrogramWidget::ChannelMode::FrontOnly));
    m_channelMode->addItem(tr("Back Only"), static_cast<int>(SpectrogramWidget::ChannelMode::BackOnly));

    m_presentationMode->addItem(tr("Scrolling"), static_cast<int>(SpectrogramWidget::PresentationMode::Scrolling));
    m_presentationMode->addItem(tr("Stationary"), static_cast<int>(SpectrogramWidget::PresentationMode::Stationary));

    m_windowFunction->addItem(tr("Hann"), static_cast<int>(VisualisationSession::SpectrumWindowFunction::Hann));
    m_windowFunction->addItem(tr("Blackman-Harris"),
                              static_cast<int>(VisualisationSession::SpectrumWindowFunction::BlackmanHarris));
    m_windowFunction->addItem(tr("None"), static_cast<int>(VisualisationSession::SpectrumWindowFunction::None));

    m_channelSpacing->setRange(0, 100);
    m_channelSpacing->setSuffix(u" px"_s);
    m_channelSpacing->setToolTip(tr("Vertical space between the left and right channel spectrograms."));

    int row = 0;
    generalLayout->addWidget(m_logFrequency, row++, 0, 1, 2);
    generalLayout->addWidget(m_clearOnTrackChange, row++, 0, 1, 2);
    generalLayout->addWidget(new QLabel(tr("Channels") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_channelMode, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Style") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_presentationMode, row++, 1);
    generalLayout->setColumnStretch(2, 1);

    auto* analysisGroup  = new QGroupBox(tr("Analysis"), this);
    auto* analysisLayout = new QGridLayout(analysisGroup);
    analysisLayout->addWidget(new QLabel(tr("FFT size") + ":"_L1, this), 0, 0);
    analysisLayout->addWidget(m_fftSize, 0, 1);
    analysisLayout->addWidget(new QLabel(tr("Window function") + ":"_L1, this), 1, 0);
    analysisLayout->addWidget(m_windowFunction, 1, 1);
    analysisLayout->setColumnStretch(2, 1);

    auto* scaleGroup  = new QGroupBox(tr("Scale"), this);
    auto* scaleLayout = new QGridLayout(scaleGroup);

    m_minDb->setRange(-120, 0);
    m_minDb->setSuffix(u" dB"_s);
    m_maxDb->setRange(-60, 12);
    m_maxDb->setSuffix(u" dB"_s);
    scaleLayout->addWidget(new QLabel(tr("Minimum level") + ":"_L1, this), 0, 0);
    scaleLayout->addWidget(m_minDb, 0, 1);
    scaleLayout->addWidget(new QLabel(tr("Maximum level") + ":"_L1, this), 1, 0);
    scaleLayout->addWidget(m_maxDb, 1, 1);
    scaleLayout->addWidget(new QLabel(tr("Channel spacing") + ":"_L1, this), 2, 0);
    scaleLayout->addWidget(m_channelSpacing, 2, 1);
    scaleLayout->setColumnStretch(2, 1);

    auto* generalPage       = new QWidget(this);
    auto* generalPageLayout = new QGridLayout(generalPage);
    generalPageLayout->addWidget(generalGroup, 0, 0, 1, 2);
    generalPageLayout->addWidget(analysisGroup, 1, 0);
    generalPageLayout->addWidget(scaleGroup, 1, 1);
    generalPageLayout->setColumnStretch(0, 1);
    generalPageLayout->setColumnStretch(1, 1);
    generalPageLayout->setRowStretch(2, 1);

    m_colours->setOrientationControlVisible(false);

    auto* coloursPage       = new QWidget(this);
    auto* coloursPageLayout = new QGridLayout(coloursPage);
    coloursPageLayout->addWidget(m_colours, 0, 0);
    coloursPageLayout->setRowStretch(0, 1);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(generalPage, tr("General"));
    tabs->addTab(coloursPage, tr("Colours"));

    auto* layout{contentLayout()};
    layout->addWidget(tabs, 0, 0);
    layout->setRowStretch(0, 1);

    loadCurrentConfig();
}

SpectrogramWidget::ConfigData SpectrogramConfigDialog::config() const
{
    return {
        .channelMode      = static_cast<SpectrogramWidget::ChannelMode>(m_channelMode->currentData().toInt()),
        .presentationMode = static_cast<SpectrogramWidget::PresentationMode>(m_presentationMode->currentData().toInt()),
        .logFrequency     = m_logFrequency->isChecked(),
        .clearOnTrackChange = m_clearOnTrackChange->isChecked(),
        .channelSpacing     = m_channelSpacing->value(),
        .fftSize            = m_fftSize->currentData().toInt(),
        .minDb              = m_minDb->value(),
        .maxDb              = m_maxDb->value(),
        .windowFunction     = m_windowFunction->currentData().toInt(),
        .colours            = m_colours->colours(),
    };
}

void SpectrogramConfigDialog::setConfig(const SpectrogramWidget::ConfigData& config)
{
    m_logFrequency->setChecked(config.logFrequency);
    m_clearOnTrackChange->setChecked(config.clearOnTrackChange);
    m_channelSpacing->setValue(config.channelSpacing);

    int index = m_channelMode->findData(static_cast<int>(config.channelMode));
    if(index < 0) {
        index = m_channelMode->findData(static_cast<int>(SpectrogramWidget::ChannelMode::Unchanged));
    }
    m_channelMode->setCurrentIndex(index);

    index = m_presentationMode->findData(static_cast<int>(config.presentationMode));
    if(index < 0) {
        index = m_presentationMode->findData(static_cast<int>(SpectrogramWidget::PresentationMode::Scrolling));
    }
    m_presentationMode->setCurrentIndex(index);

    index = m_fftSize->findData(config.fftSize);
    if(index < 0) {
        index = m_fftSize->findData(4096);
    }
    m_fftSize->setCurrentIndex(index);

    index = m_windowFunction->findData(config.windowFunction);
    if(index < 0) {
        index = m_windowFunction->findData(
            static_cast<int>(VisualisationSession::SpectrumWindowFunction::BlackmanHarris));
    }
    m_windowFunction->setCurrentIndex(index);

    m_minDb->setValue(config.minDb);
    m_maxDb->setValue(config.maxDb);
    m_colours->setColours(config.colours);
}
} // namespace Fooyin::Spectrogram
