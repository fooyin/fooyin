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

#include <gui/widgets/colourbutton.h>
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
    , m_trackPreference{new QComboBox(this)}
    , m_showTopLabels{new QCheckBox(tr("Top labels"), this)}
    , m_showBottomLabels{new QCheckBox(tr("Bottom labels"), this)}
    , m_showLeftLabels{new QCheckBox(tr("Left labels"), this)}
    , m_showRightLabels{new QCheckBox(tr("Right labels"), this)}
    , m_showCursor{new QCheckBox(tr("Show progress cursor"), this)}
    , m_cursorWidth{new QSpinBox(this)}
    , m_channelSpacing{new QSpinBox(this)}
    , m_fftSize{new QComboBox(this)}
    , m_timeColumns{new QComboBox(this)}
    , m_windowFunction{new QComboBox(this)}
    , m_minDb{new QSpinBox(this)}
    , m_maxDb{new QSpinBox(this)}
    , m_colours{new GradientEditor(this)}
    , m_cursorColour{new ColourButton(tr("Playing"), true, this)}
    , m_seekingCursorColour{new ColourButton(tr("Seeking"), true, this)}
    , m_playedOverlayColour{new ColourButton(tr("Played"), true, this)}
    , m_unplayedOverlayColour{new ColourButton(tr("Unplayed"), true, this)}
{
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    for(const int fftSize : FftSizes) {
        m_fftSize->addItem(QString::number(fftSize), fftSize);
    }

    m_channelMode->addItem(tr("Unchanged"), static_cast<int>(ChannelMode::Unchanged));
    m_channelMode->addItem(tr("Mono"), static_cast<int>(ChannelMode::Mono));
    m_channelMode->addItem(tr("Front Only"), static_cast<int>(ChannelMode::FrontOnly));
    m_channelMode->addItem(tr("Back Only"), static_cast<int>(ChannelMode::BackOnly));

    m_presentationMode->addItem(tr("Scrolling"), static_cast<int>(PresentationMode::Scrolling));
    m_presentationMode->addItem(tr("Stationary"), static_cast<int>(PresentationMode::Stationary));
    m_presentationMode->addItem(tr("Full Track (Seekbar)"), static_cast<int>(PresentationMode::FullTrack));

    auto* trackPreferenceLabel = new QLabel(tr("Track preference") + ":"_L1, this);

    m_trackPreference->addItem(tr("Playing track"), static_cast<int>(TrackDisplayPreference::PlayingTrack));
    m_trackPreference->addItem(tr("Selected track"), static_cast<int>(TrackDisplayPreference::SelectedTrack));
    m_trackPreference->addItem(tr("Playing (or selected when stopped)"),
                               static_cast<int>(TrackDisplayPreference::PlayingTrackSelectedWhenStopped));
    m_trackPreference->addItem(tr("Playing (blank at startup)"),
                               static_cast<int>(TrackDisplayPreference::PlayingTrackBlankAtStartup));
    m_trackPreference->addItem(tr("Playing (blank when stopped)"),
                               static_cast<int>(TrackDisplayPreference::PlayingTrackBlankWhenStopped));

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
    generalLayout->addWidget(trackPreferenceLabel, row, 0);
    generalLayout->addWidget(m_trackPreference, row++, 1);
    generalLayout->setColumnStretch(2, 1);

    auto* axesGroup  = new QGroupBox(tr("Axes"), this);
    auto* axesLayout = new QGridLayout(axesGroup);

    row = 0;
    axesLayout->addWidget(m_showTopLabels, row, 0);
    axesLayout->addWidget(m_showBottomLabels, row++, 1);
    axesLayout->addWidget(m_showLeftLabels, row, 0);
    axesLayout->addWidget(m_showRightLabels, row++, 1);
    axesLayout->setColumnStretch(2, 1);

    m_cursorWidth->setRange(1, 20);
    m_cursorWidth->setSuffix(u" px"_s);

    auto* cursorGroup  = new QGroupBox(tr("Cursors"), this);
    auto* cursorLayout = new QGridLayout(cursorGroup);

    row = 0;
    cursorLayout->addWidget(m_showCursor, row++, 0, 1, 2);
    cursorLayout->addWidget(new QLabel(tr("Cursor width") + ":"_L1, this), row, 0);
    cursorLayout->addWidget(m_cursorWidth, row++, 1);
    cursorLayout->setColumnStretch(2, 1);

    for(const int columns : {256, 512, 1024, 2048, 4096, 8192}) {
        m_timeColumns->addItem(QString::number(columns), columns);
    }
    m_timeColumns->setToolTip(
        tr("Number of time columns for Full Track analysis.\nHigher values increase detail and memory usage."));

    auto* timeColumnsLabel = new QLabel(tr("Time resolution") + ":"_L1, this);

    auto* analysisGroup  = new QGroupBox(tr("Analysis"), this);
    auto* analysisLayout = new QGridLayout(analysisGroup);

    row = 0;
    analysisLayout->addWidget(new QLabel(tr("FFT size") + ":"_L1, this), row, 0);
    analysisLayout->addWidget(m_fftSize, row++, 1);
    analysisLayout->addWidget(new QLabel(tr("Window function") + ":"_L1, this), row, 0);
    analysisLayout->addWidget(m_windowFunction, row++, 1);
    analysisLayout->addWidget(timeColumnsLabel, row, 0);
    analysisLayout->addWidget(m_timeColumns, row++, 1);
    analysisLayout->setColumnStretch(2, 1);

    m_minDb->setRange(-120, 0);
    m_minDb->setSuffix(u" dB"_s);
    m_maxDb->setRange(-60, 12);
    m_maxDb->setSuffix(u" dB"_s);

    auto* scaleGroup  = new QGroupBox(tr("Scale"), this);
    auto* scaleLayout = new QGridLayout(scaleGroup);

    row = 0;
    scaleLayout->addWidget(new QLabel(tr("Minimum level") + ":"_L1, this), row, 0);
    scaleLayout->addWidget(m_minDb, row++, 1);
    scaleLayout->addWidget(new QLabel(tr("Maximum level") + ":"_L1, this), row, 0);
    scaleLayout->addWidget(m_maxDb, row++, 1);
    scaleLayout->addWidget(new QLabel(tr("Channel spacing") + ":"_L1, this), row, 0);
    scaleLayout->addWidget(m_channelSpacing, row++, 1);
    scaleLayout->setColumnStretch(2, 1);

    auto* generalPage       = new QWidget(this);
    auto* generalPageLayout = new QGridLayout(generalPage);

    row = 0;
    generalPageLayout->addWidget(generalGroup, row++, 0, 1, 2);
    generalPageLayout->addWidget(analysisGroup, row, 0);
    generalPageLayout->addWidget(scaleGroup, row++, 1);
    generalPageLayout->addWidget(axesGroup, row, 0);
    generalPageLayout->addWidget(cursorGroup, row++, 1);
    generalPageLayout->setColumnStretch(0, 1);
    generalPageLayout->setColumnStretch(1, 1);
    generalPageLayout->setRowStretch(3, 1);

    m_colours->setOrientationControlVisible(false);

    auto* spectrogramColoursGroup  = new QGroupBox(tr("Spectrogram"), this);
    auto* spectrogramColoursLayout = new QGridLayout(spectrogramColoursGroup);

    spectrogramColoursLayout->addWidget(m_colours, 0, 0);

    ColourButton::alignLabels({m_cursorColour, m_seekingCursorColour, m_playedOverlayColour, m_unplayedOverlayColour});

    auto* cursorColoursGroup  = new QGroupBox(tr("Cursors"), this);
    auto* cursorColoursLayout = new QGridLayout(cursorColoursGroup);

    row = 0;
    cursorColoursLayout->addWidget(m_cursorColour, row++, 0);
    cursorColoursLayout->addWidget(m_seekingCursorColour, row++, 0);

    auto* overlayGroup  = new QGroupBox(tr("Playback overlay"), this);
    auto* overlayLayout = new QGridLayout(overlayGroup);

    row = 0;
    overlayLayout->addWidget(m_playedOverlayColour, row++, 0);
    overlayLayout->addWidget(m_unplayedOverlayColour, row++, 0);

    auto* coloursPage       = new QWidget(this);
    auto* coloursPageLayout = new QGridLayout(coloursPage);

    row = 0;
    coloursPageLayout->addWidget(spectrogramColoursGroup, row++, 0);
    coloursPageLayout->addWidget(overlayGroup, row++, 0);
    coloursPageLayout->addWidget(cursorColoursGroup, row++, 0);
    coloursPageLayout->setRowStretch(0, 1);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(generalPage, tr("General"));
    tabs->addTab(coloursPage, tr("Colours"));

    auto* layout{contentLayout()};
    layout->addWidget(tabs, 0, 0);
    layout->setRowStretch(0, 1);

    QObject::connect(widget, &SpectrogramWidget::configChanged, this, &SpectrogramConfigDialog::syncCurrentConfig);

    const auto updateModeOptions
        = [this, axesGroup, cursorGroup, cursorColoursGroup, overlayGroup, timeColumnsLabel, trackPreferenceLabel]() {
              const bool fullTrack
                  = m_presentationMode->currentData().toInt() == static_cast<int>(PresentationMode::FullTrack);
              m_clearOnTrackChange->setEnabled(!fullTrack);
              m_trackPreference->setEnabled(fullTrack);
              trackPreferenceLabel->setEnabled(fullTrack);
              m_timeColumns->setEnabled(fullTrack);
              timeColumnsLabel->setEnabled(fullTrack);
              axesGroup->setEnabled(fullTrack);
              cursorGroup->setEnabled(fullTrack);
              cursorColoursGroup->setEnabled(fullTrack);
              overlayGroup->setEnabled(fullTrack);
          };
    QObject::connect(m_presentationMode, &QComboBox::currentIndexChanged, this, updateModeOptions);

    loadCurrentConfig();
    updateModeOptions();
}

SpectrogramConfig SpectrogramConfigDialog::config() const
{
    return {
        .channelMode           = static_cast<ChannelMode>(m_channelMode->currentData().toInt()),
        .presentationMode      = static_cast<PresentationMode>(m_presentationMode->currentData().toInt()),
        .trackPreference       = m_trackPreference->currentData().toInt(),
        .logFrequency          = m_logFrequency->isChecked(),
        .clearOnTrackChange    = m_clearOnTrackChange->isChecked(),
        .showTopLabels         = m_showTopLabels->isChecked(),
        .showBottomLabels      = m_showBottomLabels->isChecked(),
        .showLeftLabels        = m_showLeftLabels->isChecked(),
        .showRightLabels       = m_showRightLabels->isChecked(),
        .showCursor            = m_showCursor->isChecked(),
        .cursorWidth           = m_cursorWidth->value(),
        .cursorColour          = m_cursorColour->isChecked() ? m_cursorColour->colour() : QColor{},
        .seekingCursorColour   = m_seekingCursorColour->isChecked() ? m_seekingCursorColour->colour() : QColor{},
        .playedOverlayColour   = m_playedOverlayColour->isChecked() ? m_playedOverlayColour->colour() : QColor{},
        .unplayedOverlayColour = m_unplayedOverlayColour->isChecked() ? m_unplayedOverlayColour->colour() : QColor{},
        .channelSpacing        = m_channelSpacing->value(),
        .fftSize               = m_fftSize->currentData().toInt(),
        .timeColumns           = m_timeColumns->currentData().toInt(),
        .minDb                 = m_minDb->value(),
        .maxDb                 = m_maxDb->value(),
        .windowFunction        = m_windowFunction->currentData().toInt(),
        .colours               = m_colours->colours(),
    };
}

void SpectrogramConfigDialog::setConfig(const SpectrogramConfig& config)
{
    const int preferenceIndex = m_trackPreference->findData(config.trackPreference);
    m_trackPreference->setCurrentIndex(preferenceIndex >= 0 ? preferenceIndex : 0);
    m_logFrequency->setChecked(config.logFrequency);
    m_clearOnTrackChange->setChecked(config.clearOnTrackChange);
    m_channelSpacing->setValue(config.channelSpacing);
    const int columnsIndex = m_timeColumns->findData(config.timeColumns);
    m_timeColumns->setCurrentIndex(columnsIndex >= 0 ? columnsIndex : m_timeColumns->findData(1024));
    m_showCursor->setChecked(config.showCursor);
    m_cursorWidth->setValue(config.cursorWidth);

    const QColor highlight = widget()->palette().color(QPalette::Active, QPalette::Highlight);

    m_cursorColour->setColour(config.cursorColour.isValid() ? config.cursorColour : highlight);
    m_cursorColour->setChecked(config.cursorColour.isValid());
    m_seekingCursorColour->setColour(config.seekingCursorColour.isValid() ? config.seekingCursorColour
                                                                          : highlight.darker(150));
    m_seekingCursorColour->setChecked(config.seekingCursorColour.isValid());
    m_playedOverlayColour->setColour(config.playedOverlayColour.isValid() ? config.playedOverlayColour
                                                                          : QColor{Qt::transparent});
    m_playedOverlayColour->setChecked(config.playedOverlayColour.isValid());
    m_unplayedOverlayColour->setColour(config.unplayedOverlayColour.isValid() ? config.unplayedOverlayColour
                                                                              : QColor{Qt::transparent});
    m_unplayedOverlayColour->setChecked(config.unplayedOverlayColour.isValid());

    m_showTopLabels->setChecked(config.showTopLabels);
    m_showBottomLabels->setChecked(config.showBottomLabels);
    m_showLeftLabels->setChecked(config.showLeftLabels);
    m_showRightLabels->setChecked(config.showRightLabels);

    int index = m_channelMode->findData(static_cast<int>(config.channelMode));
    if(index < 0) {
        index = m_channelMode->findData(static_cast<int>(ChannelMode::Unchanged));
    }
    m_channelMode->setCurrentIndex(index);

    index = m_presentationMode->findData(static_cast<int>(config.presentationMode));
    if(index < 0) {
        index = m_presentationMode->findData(static_cast<int>(PresentationMode::Scrolling));
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

void SpectrogramConfigDialog::mergeExternalConfig(const SpectrogramConfig& previous, const SpectrogramConfig& current)
{
    mergeExternalFields(previous, current, &SpectrogramConfig::channelMode, &SpectrogramConfig::presentationMode,
                        &SpectrogramConfig::trackPreference, &SpectrogramConfig::logFrequency,
                        &SpectrogramConfig::clearOnTrackChange, &SpectrogramConfig::showTopLabels,
                        &SpectrogramConfig::showBottomLabels, &SpectrogramConfig::showLeftLabels,
                        &SpectrogramConfig::showRightLabels, &SpectrogramConfig::showCursor,
                        &SpectrogramConfig::cursorWidth, &SpectrogramConfig::cursorColour,
                        &SpectrogramConfig::seekingCursorColour, &SpectrogramConfig::playedOverlayColour,
                        &SpectrogramConfig::unplayedOverlayColour, &SpectrogramConfig::channelSpacing,
                        &SpectrogramConfig::fftSize, &SpectrogramConfig::timeColumns, &SpectrogramConfig::minDb,
                        &SpectrogramConfig::maxDb, &SpectrogramConfig::windowFunction, &SpectrogramConfig::colours);
}
} // namespace Fooyin::Spectrogram
