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

#include "coverwidgetconfigwidget.h"

#include <gui/widgets/doubleslidereditor.h>
#include <gui/widgets/slidereditor.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>

using namespace Qt::StringLiterals;

namespace Fooyin {
CoverWidgetConfigDialog::CoverWidgetConfigDialog(CoverWidget* coverWidget, QWidget* parent)
    : WidgetConfigDialog{coverWidget, tr("Cover Widget Settings"), parent}
    , m_coverTypeGroup{new QButtonGroup(this)}
    , m_alignmentGroup{new QButtonGroup(this)}
    , m_trackPreference{new QComboBox(this)}
    , m_style{new QComboBox(this)}
    , m_artworkScaling{new QComboBox(this)}
    , m_spinDisc{new QCheckBox(tr("Spin disc"), this)}
    , m_spinSpeed{new SliderEditor(this)}
    , m_spinRampDuration{new DoubleSliderEditor(this)}
    , m_fadeEnabled{new QCheckBox(tr("Fade cover changes"), this)}
    , m_fadeDuration{new SliderEditor(tr("Fade length"), this)}
    , m_doubleClick{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
{
    auto* displayGroup  = new QGroupBox(tr("Display"), this);
    auto* displayLayout = new QGridLayout(displayGroup);

    auto* frontButton  = new QRadioButton(tr("Front"), displayGroup);
    auto* backButton   = new QRadioButton(tr("Back"), displayGroup);
    auto* artistButton = new QRadioButton(tr("Artist"), displayGroup);

    auto* coverTypeLayout = new QHBoxLayout();
    coverTypeLayout->addWidget(frontButton);
    coverTypeLayout->addWidget(backButton);
    coverTypeLayout->addWidget(artistButton);
    coverTypeLayout->addStretch();

    m_coverTypeGroup->addButton(frontButton, static_cast<int>(Track::Cover::Front));
    m_coverTypeGroup->addButton(backButton, static_cast<int>(Track::Cover::Back));
    m_coverTypeGroup->addButton(artistButton, static_cast<int>(Track::Cover::Artist));

    auto* centreButton = new QRadioButton(tr("Centre"), displayGroup);
    auto* leftButton   = new QRadioButton(tr("Left"), displayGroup);
    auto* rightButton  = new QRadioButton(tr("Right"), displayGroup);

    auto* alignmentLayout = new QHBoxLayout();
    alignmentLayout->addWidget(centreButton);
    alignmentLayout->addWidget(leftButton);
    alignmentLayout->addWidget(rightButton);
    alignmentLayout->addStretch();

    m_alignmentGroup->addButton(centreButton, Qt::AlignCenter);
    m_alignmentGroup->addButton(leftButton, Qt::AlignLeft);
    m_alignmentGroup->addButton(rightButton, Qt::AlignRight);

    m_trackPreference->addItem(tr("Playing track"), static_cast<int>(TrackDisplayPreference::PlayingTrack));
    m_trackPreference->addItem(tr("Selected track"), static_cast<int>(TrackDisplayPreference::SelectedTrack));
    m_trackPreference->addItem(tr("Playing (or selected when stopped)"),
                               static_cast<int>(TrackDisplayPreference::PlayingTrackSelectedWhenStopped));
    m_trackPreference->addItem(tr("Playing (no track at startup)"),
                               static_cast<int>(TrackDisplayPreference::PlayingTrackBlankAtStartup));
    m_trackPreference->addItem(tr("Playing (no track when stopped)"),
                               static_cast<int>(TrackDisplayPreference::PlayingTrackBlankWhenStopped));

    m_artworkScaling->addItem(tr("Fit"), static_cast<int>(ArtworkScaling::Fit));
    m_artworkScaling->addItem(tr("Fill (crop)"), static_cast<int>(ArtworkScaling::Fill));
    m_artworkScaling->addItem(tr("Stretch"), static_cast<int>(ArtworkScaling::Stretch));
    m_artworkScaling->setToolTip(tr("Fit shows the whole image and preserves its aspect ratio.\n"
                                    "Fill crops the image to fill the artwork area and preserves its aspect ratio.\n"
                                    "Stretch fills the artwork area by changing the image’s aspect ratio."));

    m_style->addItem(tr("Cover"), static_cast<int>(CoverStyle::Cover));
    m_style->addItem(tr("Disc"), static_cast<int>(CoverStyle::Disc));

    m_spinDisc->setToolTip(tr("Rotate the disc while its track is playing"));

    m_spinSpeed->setRange(0, CoverWidget::MaxSpinSpeedRpm);
    m_spinSpeed->setSingleStep(1);
    m_spinSpeed->setSuffix(tr(" rpm"));

    m_spinRampDuration->setRange(0.0, CoverWidget::MaxSpinRampDurationMs / 1000.0);
    m_spinRampDuration->setSingleStep(0.1);
    m_spinRampDuration->setSuffix(tr(" s"));
    m_spinRampDuration->setToolTip(tr("Time taken to accelerate to the configured spin speed or decelerate to rest.\n"
                                      "Set to 0 for instant speed changes."));

    int row{0};
    displayLayout->addWidget(new QLabel(tr("Cover type") + u":"_s, displayGroup), row, 0);
    displayLayout->addLayout(coverTypeLayout, row++, 1);
    displayLayout->addWidget(new QLabel(tr("Alignment") + u":"_s, displayGroup), row, 0);
    displayLayout->addLayout(alignmentLayout, row++, 1);
    displayLayout->addWidget(new QLabel(tr("Preferred track") + u":"_s, displayGroup), row, 0);
    displayLayout->addWidget(m_trackPreference, row++, 1);
    auto* artworkScalingLabel = new QLabel(tr("Artwork scaling") + u":"_s, displayGroup);
    displayLayout->addWidget(artworkScalingLabel, row, 0);
    displayLayout->addWidget(m_artworkScaling, row++, 1);
    displayLayout->addWidget(new QLabel(tr("Style") + u":"_s, displayGroup), row, 0);
    displayLayout->addWidget(m_style, row++, 1);
    displayLayout->addWidget(m_spinDisc, row++, 1);
    auto* spinSpeedLabel = new QLabel(tr("Spin speed") + u":"_s, displayGroup);
    displayLayout->addWidget(spinSpeedLabel, row, 0);
    displayLayout->addWidget(m_spinSpeed, row++, 1);
    auto* spinRampLabel = new QLabel(tr("Ramp time") + u":"_s, displayGroup);
    displayLayout->addWidget(spinRampLabel, row, 0);
    displayLayout->addWidget(m_spinRampDuration, row++, 1);
    displayLayout->setColumnStretch(1, 1);

    const auto updateStyleControls = [this, artworkScalingLabel, spinSpeedLabel, spinRampLabel]() {
        const bool disc     = m_style->currentData().toInt() == static_cast<int>(CoverStyle::Disc);
        const bool spinning = disc && m_spinDisc->isChecked();
        m_artworkScaling->setEnabled(!disc);
        artworkScalingLabel->setEnabled(!disc);
        m_spinDisc->setEnabled(disc);
        m_spinSpeed->setEnabled(spinning);
        spinSpeedLabel->setEnabled(spinning);
        m_spinRampDuration->setEnabled(spinning);
        spinRampLabel->setEnabled(spinning);
    };
    QObject::connect(m_style, &QComboBox::currentIndexChanged, this, updateStyleControls);
    QObject::connect(m_spinDisc, &QCheckBox::toggled, this, updateStyleControls);
    updateStyleControls();

    auto* fadeGroup  = new QGroupBox(tr("Cover change"), this);
    auto* fadeLayout = new QGridLayout(fadeGroup);

    m_fadeDuration->setRange(50, 3000);
    m_fadeDuration->setSingleStep(50);
    m_fadeDuration->setSuffix(u" ms"_s);

    row = 0;
    fadeLayout->addWidget(m_fadeEnabled, row++, 0);
    fadeLayout->addWidget(m_fadeDuration, row++, 0);
    fadeLayout->setRowStretch(row, 1);

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    const auto addActions = [](QComboBox* box) {
        box->addItem(tr("None"), static_cast<int>(CoverAction::None));
        box->addItem(tr("View full size"), static_cast<int>(CoverAction::ViewFullSize));
        box->addItem(tr("Next available artwork type"), static_cast<int>(CoverAction::NextArtworkType));
        box->addItem(tr("Show track"), static_cast<int>(CoverAction::ShowTrack));
        box->addItem(tr("Open containing folder"), static_cast<int>(CoverAction::OpenContainingFolder));
        box->addItem(tr("Open properties"), static_cast<int>(CoverAction::OpenProperties));
    };
    addActions(m_doubleClick);
    addActions(m_middleClick);

    row = 0;
    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, clickBehaviour), row, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, row++, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, clickBehaviour), row, 0);
    clickBehaviourLayout->addWidget(m_middleClick, row++, 1);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* layout{contentLayout()};

    row = 0;
    layout->addWidget(displayGroup, row++, 0);
    layout->addWidget(fadeGroup, row++, 0);
    layout->addWidget(clickBehaviour, row++, 0);
    layout->setRowStretch(row, 1);
    layout->setColumnStretch(0, 1);

    QObject::connect(coverWidget, &CoverWidget::configChanged, this, &CoverWidgetConfigDialog::syncCurrentConfig);

    loadCurrentConfig();
}

void CoverWidgetConfigDialog::setConfig(const CoverWidget::ConfigData& config)
{
    if(auto* button = m_coverTypeGroup->button(static_cast<int>(config.coverType))) {
        button->setChecked(true);
    }
    if(auto* button = m_alignmentGroup->button(config.coverAlignment)) {
        button->setChecked(true);
    }

    m_artworkScaling->setCurrentIndex(m_artworkScaling->findData(static_cast<int>(config.artworkScaling)));
    m_style->setCurrentIndex(m_style->findData(static_cast<int>(config.style)));
    m_spinDisc->setChecked(config.spinDisc);
    m_spinSpeed->setValue(config.spinSpeedRpm);
    m_spinRampDuration->setValue(config.spinRampDurationMs / 1000.0);
    m_trackPreference->setCurrentIndex(m_trackPreference->findData(static_cast<int>(config.trackPreference)));
    m_fadeEnabled->setChecked(config.fadeCoverChanges);
    m_fadeDuration->setValue(config.fadeDurationMs);
    m_doubleClick->setCurrentIndex(m_doubleClick->findData(static_cast<int>(config.doubleClickAction)));
    m_middleClick->setCurrentIndex(m_middleClick->findData(static_cast<int>(config.middleClickAction)));
}

CoverWidget::ConfigData CoverWidgetConfigDialog::config() const
{
    return {
        .coverType          = static_cast<Track::Cover>(m_coverTypeGroup->checkedId()),
        .coverAlignment     = static_cast<Qt::Alignment>(m_alignmentGroup->checkedId()),
        .artworkScaling     = artworkScalingFromInt(m_artworkScaling->currentData().toInt()),
        .style              = static_cast<CoverStyle>(m_style->currentData().toInt()),
        .spinDisc           = m_spinDisc->isChecked(),
        .spinSpeedRpm       = m_spinSpeed->value(),
        .spinRampDurationMs = static_cast<int>(std::llround(m_spinRampDuration->value() * 1000.0)),
        .trackPreference    = static_cast<TrackDisplayPreference>(m_trackPreference->currentData().toInt()),
        .fadeCoverChanges   = m_fadeEnabled->isChecked(),
        .fadeDurationMs     = m_fadeDuration->value(),
        .doubleClickAction  = static_cast<CoverAction>(m_doubleClick->currentData().toInt()),
        .middleClickAction  = static_cast<CoverAction>(m_middleClick->currentData().toInt()),
    };
}

void CoverWidgetConfigDialog::mergeExternalConfig(const CoverWidget::ConfigData& previous,
                                                  const CoverWidget::ConfigData& current)
{
    mergeExternalFields(previous, current, &CoverWidget::ConfigData::coverType,
                        &CoverWidget::ConfigData::coverAlignment, &CoverWidget::ConfigData::artworkScaling,
                        &CoverWidget::ConfigData::style, &CoverWidget::ConfigData::spinDisc,
                        &CoverWidget::ConfigData::spinSpeedRpm, &CoverWidget::ConfigData::spinRampDurationMs,
                        &CoverWidget::ConfigData::trackPreference, &CoverWidget::ConfigData::fadeCoverChanges,
                        &CoverWidget::ConfigData::fadeDurationMs, &CoverWidget::ConfigData::doubleClickAction,
                        &CoverWidget::ConfigData::middleClickAction);
}
} // namespace Fooyin
