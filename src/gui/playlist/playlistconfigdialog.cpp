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

#include "playlistconfigdialog.h"

#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/backgroundconfigwidget.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>

using namespace Qt::StringLiterals;

namespace Fooyin {
PlaylistConfigDialog::PlaylistConfigDialog(PlaylistWidget* playlist, const QString& title, bool previewMode,
                                           QWidget* parent)
    : WidgetConfigDialog{playlist, title, parent}
    , m_header{new QCheckBox(tr("Show header"), this)}
    , m_stickyHeaders{new QCheckBox(tr("Keep group headers in view"), this)}
    , m_stickySubheaders{new QCheckBox(tr("Keep subheaders in view"), this)}
    , m_stickyArtwork{new QCheckBox(tr("Keep artwork in view"), this)}
    , m_scrollBar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_alternatingRows{new QCheckBox(tr("Alternating row colours"), this)}
    , m_imagePadding{new QSpinBox(this)}
    , m_imagePaddingTop{new QSpinBox(this)}
    , m_artworkCornerRadius{new QSpinBox(this)}
    , m_artworkReflection{new QCheckBox(tr("Show reflection"), this)}
    , m_artworkReflectionHeight{new QSpinBox(this)}
    , m_artworkReflectionOpacity{new QSpinBox(this)}
    , m_artworkReflectionGap{new QSpinBox(this)}
    , m_background{new BackgroundConfigWidget(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_startPlaybackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
{
    m_imagePadding->setRange(0, 100);
    m_imagePaddingTop->setRange(0, 100);
    m_imagePadding->setSuffix(u" px"_s);
    m_imagePaddingTop->setSuffix(u" px"_s);
    m_artworkCornerRadius->setRange(0, 100);
    m_artworkCornerRadius->setSingleStep(5);
    m_artworkCornerRadius->setSuffix(u" %"_s);
    m_artworkCornerRadius->setSpecialValueText(tr("Square"));
    m_artworkReflectionHeight->setRange(1, 100);
    m_artworkReflectionHeight->setSuffix(u" %"_s);
    m_artworkReflectionHeight->setToolTip(tr("Reflection height as a percentage of the artwork height"));
    m_artworkReflectionOpacity->setRange(0, 100);
    m_artworkReflectionOpacity->setSuffix(u" %"_s);
    m_artworkReflectionOpacity->setToolTip(tr("Opacity at the top of the reflection, fading to transparent"));
    m_artworkReflectionGap->setRange(0, 100);
    m_artworkReflectionGap->setSuffix(u" px"_s);
    m_artworkReflectionGap->setToolTip(tr("Space between the artwork and its reflection"));

    auto* generalPage      = new QWidget(this);
    auto* appearance       = new QGroupBox(tr("Appearance"), generalPage);
    auto* appearanceLayout = new QGridLayout(appearance);

    int row{0};
    appearanceLayout->addWidget(m_header, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_scrollBar, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_alternatingRows, row++, 0, 1, 2);
    appearanceLayout->addWidget(Gui::createSectionHeader(tr("Scrolling"), this), row++, 0, 1, 2);
    appearanceLayout->addWidget(m_stickyHeaders, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_stickySubheaders, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_stickyArtwork, row++, 0, 1, 2);
    appearanceLayout->setColumnStretch(1, 1);
    appearanceLayout->setRowStretch(row, 1);

    auto* artwork       = new QGroupBox(tr("Artwork"), generalPage);
    auto* artworkLayout = new QGridLayout(artwork);

    row = 0;
    artworkLayout->addWidget(new QLabel(tr("Horizontal padding") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_imagePadding, row++, 1);
    artworkLayout->addWidget(new QLabel(tr("Top padding") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_imagePaddingTop, row++, 1);
    artworkLayout->addWidget(new QLabel(tr("Corner radius") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_artworkCornerRadius, row++, 1);
    artworkLayout->addWidget(Gui::createSectionHeader(tr("Artwork reflection"), this), row++, 0, 1, 2);
    artworkLayout->addWidget(m_artworkReflection, row++, 0, 1, 2);
    artworkLayout->addWidget(new QLabel(tr("Height") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_artworkReflectionHeight, row++, 1);
    artworkLayout->addWidget(new QLabel(tr("Opacity") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_artworkReflectionOpacity, row++, 1);
    artworkLayout->addWidget(new QLabel(tr("Gap below artwork") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_artworkReflectionGap, row++, 1);
    artworkLayout->setColumnStretch(1, 1);

    const auto addClickActions = [](QComboBox* combo) {
        TrackSelectionController::addAction(combo, tr("None"), TrackAction::None);
        TrackSelectionController::addAction(combo, tr("Play now"), TrackAction::Play);
        TrackSelectionController::addStandardActions(combo, ActionGroup::Queue);
    };
    addClickActions(m_doubleClick);
    addClickActions(m_middleClick);

    auto* clicks       = new QGroupBox(tr("Click Behaviour"), generalPage);
    auto* clicksLayout = new QGridLayout(clicks);

    row = 0;
    clicksLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), row, 0);
    clicksLayout->addWidget(m_doubleClick, row++, 1);
    clicksLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), row, 0);
    clicksLayout->addWidget(m_middleClick, row++, 1);
    clicksLayout->addWidget(m_startPlaybackOnSend, row, 0, 1, 2);
    clicksLayout->setColumnStretch(1, 1);

    m_startPlaybackOnSend->setToolTip(
        tr("After adding tracks to the front of or replacing the playback queue, start playback immediately"));

    clicks->setVisible(!previewMode);

    auto* generalLayout = new QGridLayout(generalPage);
    generalLayout->addWidget(appearance, 0, 0);
    generalLayout->addWidget(artwork, 0, 1);
    generalLayout->addWidget(clicks, 1, 0, 1, 2);
    generalLayout->setColumnStretch(0, 1);
    generalLayout->setColumnStretch(1, 1);
    generalLayout->setRowStretch(2, 1);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(generalPage, tr("General"));
    tabs->addTab(m_background, tr("Background"));
    contentLayout()->addWidget(tabs);

    QObject::connect(m_stickyHeaders, &QCheckBox::toggled, m_stickySubheaders, &QWidget::setEnabled);
    QObject::connect(m_artworkReflection, &QCheckBox::toggled, this, &PlaylistConfigDialog::updateReflectionControls);
    QObject::connect(m_doubleClick, &QComboBox::currentIndexChanged, this,
                     &PlaylistConfigDialog::updateStartPlaybackState);
    QObject::connect(m_middleClick, &QComboBox::currentIndexChanged, this,
                     &PlaylistConfigDialog::updateStartPlaybackState);
    QObject::connect(playlist, &PlaylistWidget::configChanged, this, &PlaylistConfigDialog::syncCurrentConfig);

    loadCurrentConfig();
}

void PlaylistConfigDialog::setConfig(const PlaylistWidget::ConfigData& config)
{
    m_header->setChecked(config.showHeader);
    m_stickyHeaders->setChecked(config.stickyHeaders);
    m_stickySubheaders->setChecked(config.stickySubheaders);
    m_stickySubheaders->setEnabled(config.stickyHeaders);
    m_stickyArtwork->setChecked(config.stickyArtwork);
    m_scrollBar->setChecked(config.showScrollBar);
    m_alternatingRows->setChecked(config.alternatingRows);
    m_imagePadding->setValue(config.imagePadding);
    m_imagePaddingTop->setValue(config.imagePaddingTop);
    m_artworkCornerRadius->setValue(config.artworkCornerRadius);
    m_artworkReflection->setChecked(config.artworkReflection);
    m_artworkReflectionHeight->setValue(config.artworkReflectionHeight);
    m_artworkReflectionOpacity->setValue(config.artworkReflectionOpacity);
    m_artworkReflectionGap->setValue(config.artworkReflectionGap);
    m_background->setConfig(config.background);
    TrackSelectionController::setCurrentAction(m_doubleClick, static_cast<int>(config.doubleClickAction));
    TrackSelectionController::setCurrentAction(m_middleClick, static_cast<int>(config.middleClickAction));
    m_startPlaybackOnSend->setChecked(config.startPlaybackOnSend);
    updateReflectionControls();
    updateStartPlaybackState();
}

PlaylistWidget::ConfigData PlaylistConfigDialog::config() const
{
    return {
        .showHeader               = m_header->isChecked(),
        .stickyHeaders            = m_stickyHeaders->isChecked(),
        .stickySubheaders         = m_stickySubheaders->isChecked(),
        .stickyArtwork            = m_stickyArtwork->isChecked(),
        .showScrollBar            = m_scrollBar->isChecked(),
        .alternatingRows          = m_alternatingRows->isChecked(),
        .imagePadding             = m_imagePadding->value(),
        .imagePaddingTop          = m_imagePaddingTop->value(),
        .artworkCornerRadius      = m_artworkCornerRadius->value(),
        .artworkReflection        = m_artworkReflection->isChecked(),
        .artworkReflectionHeight  = m_artworkReflectionHeight->value(),
        .artworkReflectionOpacity = m_artworkReflectionOpacity->value(),
        .artworkReflectionGap     = m_artworkReflectionGap->value(),
        .background               = m_background->config(),
        .doubleClickAction        = static_cast<TrackAction>(m_doubleClick->currentData().toInt()),
        .middleClickAction        = static_cast<TrackAction>(m_middleClick->currentData().toInt()),
        .startPlaybackOnSend      = m_startPlaybackOnSend->isChecked(),
    };
}

void PlaylistConfigDialog::mergeExternalConfig(const PlaylistWidget::ConfigData& previous,
                                               const PlaylistWidget::ConfigData& current)
{
    mergeExternalFields(
        previous, current, &PlaylistWidget::ConfigData::showHeader, &PlaylistWidget::ConfigData::showScrollBar,
        &PlaylistWidget::ConfigData::stickyHeaders, &PlaylistWidget::ConfigData::stickySubheaders,
        &PlaylistWidget::ConfigData::stickyArtwork, &PlaylistWidget::ConfigData::alternatingRows,
        &PlaylistWidget::ConfigData::imagePadding, &PlaylistWidget::ConfigData::imagePaddingTop,
        &PlaylistWidget::ConfigData::artworkCornerRadius, &PlaylistWidget::ConfigData::artworkReflection,
        &PlaylistWidget::ConfigData::artworkReflectionHeight, &PlaylistWidget::ConfigData::artworkReflectionOpacity,
        &PlaylistWidget::ConfigData::artworkReflectionGap, &PlaylistWidget::ConfigData::background,
        &PlaylistWidget::ConfigData::doubleClickAction, &PlaylistWidget::ConfigData::middleClickAction,
        &PlaylistWidget::ConfigData::startPlaybackOnSend);
}

void PlaylistConfigDialog::updateReflectionControls() const
{
    const bool enabled = m_artworkReflection->isChecked();
    m_artworkReflectionHeight->setEnabled(enabled);
    m_artworkReflectionOpacity->setEnabled(enabled);
    m_artworkReflectionGap->setEnabled(enabled);
}

void PlaylistConfigDialog::updateStartPlaybackState() const
{
    const auto supportsImmediatePlayback = [](const QComboBox* combo) {
        const auto action = static_cast<TrackAction>(combo->currentData().toInt());
        return action == TrackAction::QueueNext || action == TrackAction::SendToQueue;
    };
    m_startPlaybackOnSend->setEnabled(supportsImmediatePlayback(m_doubleClick)
                                      || supportsImmediatePlayback(m_middleClick));
}
} // namespace Fooyin
