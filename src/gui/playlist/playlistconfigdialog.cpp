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

#include "internalguisettings.h"
#include "widgets/pixmapfadecontroller.h"

#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/slidereditor.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
void addComboItem(QComboBox* combo, const QString& text, int data, const QString& tooltip = {})
{
    combo->addItem(text, data);
    if(!tooltip.isEmpty()) {
        combo->setItemData(combo->count() - 1, tooltip, Qt::ToolTipRole);
    }
}
} // namespace

PlaylistConfigDialog::PlaylistConfigDialog(PlaylistWidget* playlist, const QString& title, QWidget* parent)
    : WidgetConfigDialog{playlist, title, parent}
    , m_header{new QCheckBox(tr("Show header"), this)}
    , m_scrollBar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_alternatingRows{new QCheckBox(tr("Alternating row colours"), this)}
    , m_imagePadding{new QSpinBox(this)}
    , m_imagePaddingTop{new QSpinBox(this)}
    , m_artworkCornerRadius{new QSpinBox(this)}
    , m_backgroundImage{new QComboBox(this)}
    , m_backgroundCoverTypeLabel{new QLabel(tr("Artwork type") + u":"_s, this)}
    , m_backgroundCoverType{new QComboBox(this)}
    , m_backgroundTrackPreferenceLabel{new QLabel(tr("Preferred track") + u":"_s, this)}
    , m_backgroundTrackPreference{new QComboBox(this)}
    , m_customImageLabel{new QLabel(tr("File") + u":"_s, this)}
    , m_customImage{new QLineEdit(this)}
    , m_backgroundScaling{new QComboBox(this)}
    , m_backgroundPosition{new QComboBox(this)}
    , m_backgroundMaxSize{new QSpinBox(this)}
    , m_backgroundBlur{new SliderEditor(tr("Blur"), this)}
    , m_backgroundOpacity{new SliderEditor(tr("Opacity"), this)}
    , m_backgroundFadeDuration{new SliderEditor(tr("Fade length"), this)}
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

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    int row{0};
    appearanceLayout->addWidget(m_header, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_scrollBar, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_alternatingRows, row++, 0, 1, 2);
    appearanceLayout->addWidget(Gui::createSectionHeader(tr("Artwork"), this), row++, 0, 1, 2);
    appearanceLayout->addWidget(new QLabel(tr("Left/Right") + u":"_s, this), row, 0);
    appearanceLayout->addWidget(m_imagePadding, row++, 1);
    appearanceLayout->addWidget(new QLabel(tr("Top") + u":"_s, this), row, 0);
    appearanceLayout->addWidget(m_imagePaddingTop, row++, 1);
    appearanceLayout->addWidget(new QLabel(tr("Corner radius") + u":"_s, this), row, 0);
    appearanceLayout->addWidget(m_artworkCornerRadius, row++, 1);
    appearanceLayout->setColumnStretch(1, 1);

    const auto addClickActions = [](QComboBox* combo) {
        TrackSelectionController::addAction(combo, tr("None"), TrackAction::None);
        TrackSelectionController::addAction(combo, tr("Play now"), TrackAction::Play);
        TrackSelectionController::addStandardActions(combo, ActionGroup::Queue);
    };
    addClickActions(m_doubleClick);
    addClickActions(m_middleClick);

    auto* clicks       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clicksLayout = new QGridLayout(clicks);

    row = 0;
    clicksLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), row, 0);
    clicksLayout->addWidget(m_doubleClick, row++, 1);
    clicksLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), row, 0);
    clicksLayout->addWidget(m_middleClick, row++, 1);
    clicksLayout->addWidget(m_startPlaybackOnSend, row, 0, 1, 2);
    clicksLayout->setColumnStretch(1, 1);

    auto* browseAction = new QAction(this);
    Gui::setThemeIcon(browseAction, Constants::Icons::Options);
    browseAction->setToolTip(tr("Choose a custom background image file"));
    m_customImage->addAction(browseAction, QLineEdit::TrailingPosition);

    addComboItem(m_backgroundImage, tr("No background image"), static_cast<int>(PlaylistBgImage::None));
    addComboItem(m_backgroundImage, tr("Current track artwork"), static_cast<int>(PlaylistBgImage::AlbumCover),
                 tr("Use the currently playing track's artwork as the playlist background"));
    addComboItem(m_backgroundImage, tr("Custom image"), static_cast<int>(PlaylistBgImage::Custom),
                 tr("Use the selected image file as the playlist background"));
    addComboItem(m_backgroundCoverType, tr("Front"), static_cast<int>(Track::Cover::Front),
                 tr("Use the front cover for current track artwork"));
    addComboItem(m_backgroundCoverType, tr("Back"), static_cast<int>(Track::Cover::Back),
                 tr("Use the back cover for current track artwork"));
    addComboItem(m_backgroundCoverType, tr("Artist"), static_cast<int>(Track::Cover::Artist),
                 tr("Use the artist picture for current track artwork"));
    addComboItem(m_backgroundTrackPreference, tr("Playing track"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrack));
    addComboItem(m_backgroundTrackPreference, tr("Selected track"),
                 static_cast<int>(TrackDisplayPreference::SelectedTrack));
    addComboItem(m_backgroundTrackPreference, tr("Playing (or selected when stopped)"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrackSelectedWhenStopped));
    addComboItem(m_backgroundTrackPreference, tr("Playing (blank at startup)"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrackBlankAtStartup));
    addComboItem(m_backgroundTrackPreference, tr("Playing (blank when stopped)"),
                 static_cast<int>(TrackDisplayPreference::PlayingTrackBlankWhenStopped));
    addComboItem(m_backgroundScaling, tr("Scaled and cropped"), static_cast<int>(PlaylistBgScaling::ScaledAndCropped),
                 tr("Fill the playlist area while preserving proportions; edges may be cropped"));
    addComboItem(m_backgroundScaling, tr("Scaled"), static_cast<int>(PlaylistBgScaling::Scaled),
                 tr("Stretch the image to fill the playlist area; proportions may change"));
    addComboItem(m_backgroundScaling, tr("Scaled, keep proportions"),
                 static_cast<int>(PlaylistBgScaling::ScaledKeepProportions),
                 tr("Fit the whole image inside the playlist area without cropping"));
    addComboItem(m_backgroundScaling, tr("Original size"), static_cast<int>(PlaylistBgScaling::OriginalSize),
                 tr("Draw the image at its original size, optionally limited by maximum size"));
    addComboItem(m_backgroundPosition, tr("Top left"), static_cast<int>(PlaylistBgImagePosition::TopLeft));
    addComboItem(m_backgroundPosition, tr("Top"), static_cast<int>(PlaylistBgImagePosition::Top));
    addComboItem(m_backgroundPosition, tr("Top right"), static_cast<int>(PlaylistBgImagePosition::TopRight));
    addComboItem(m_backgroundPosition, tr("Left"), static_cast<int>(PlaylistBgImagePosition::Left));
    addComboItem(m_backgroundPosition, tr("Middle"), static_cast<int>(PlaylistBgImagePosition::Middle));
    addComboItem(m_backgroundPosition, tr("Right"), static_cast<int>(PlaylistBgImagePosition::Right));
    addComboItem(m_backgroundPosition, tr("Bottom left"), static_cast<int>(PlaylistBgImagePosition::BottomLeft));
    addComboItem(m_backgroundPosition, tr("Bottom"), static_cast<int>(PlaylistBgImagePosition::Bottom));
    addComboItem(m_backgroundPosition, tr("Bottom right"), static_cast<int>(PlaylistBgImagePosition::BottomRight));

    m_backgroundMaxSize->setRange(0, 4096);
    m_backgroundMaxSize->setSuffix(u" px"_s);
    m_backgroundMaxSize->setSpecialValueText(tr("Disabled"));
    m_backgroundBlur->setRange(0, 100);
    m_backgroundBlur->setSuffix(u" px"_s);
    m_backgroundOpacity->setRange(0, 100);
    m_backgroundOpacity->setSuffix(u" %"_s);
    m_backgroundFadeDuration->setRange(0, PixmapFadeController::MaxDurationMs);
    m_backgroundFadeDuration->setSingleStep(50);
    m_backgroundFadeDuration->setSuffix(u" ms"_s);
    m_backgroundFadeDuration->addSpecialValue(0, tr("Disabled"));

    m_customImage->setToolTip(tr("Path to the custom background image"));
    m_backgroundImage->setToolTip(tr("Select which image source to use for the playlist background"));
    m_backgroundCoverType->setToolTip(tr("Select which artwork type to use for current track artwork"));
    m_backgroundTrackPreference->setToolTip(tr("Select which track supplies the background artwork"));
    m_backgroundScaling->setToolTip(tr("Controls how the background image is scaled to the playlist area"));
    m_backgroundPosition->setToolTip(tr("Alignment for original-size background images"));
    m_backgroundMaxSize->setToolTip(tr("Maximum width or height for original-size background images"));
    m_backgroundBlur->setToolTip(tr("Applies blur to the background image"));
    m_backgroundOpacity->setToolTip(tr("Controls how strongly the background image is shown"));
    m_backgroundFadeDuration->setToolTip(tr("Duration for fading between background images; set to 0 to disable"));
    m_startPlaybackOnSend->setToolTip(
        tr("After adding tracks to the front of or replacing the playback queue, start playback immediately"));

    auto* background       = new QGroupBox(tr("Background Image"), this);
    auto* backgroundLayout = new QGridLayout(background);

    row = 0;
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Image source"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(new QLabel(tr("Source") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_backgroundImage, row++, 1);
    backgroundLayout->addWidget(m_backgroundCoverTypeLabel, row, 0);
    backgroundLayout->addWidget(m_backgroundCoverType, row++, 1);
    backgroundLayout->addWidget(m_backgroundTrackPreferenceLabel, row, 0);
    backgroundLayout->addWidget(m_backgroundTrackPreference, row++, 1);
    backgroundLayout->addWidget(m_customImageLabel, row, 0);
    backgroundLayout->addWidget(m_customImage, row++, 1);
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Layout"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(new QLabel(tr("Scale mode") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_backgroundScaling, row++, 1);
    backgroundLayout->addWidget(new QLabel(tr("Alignment") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_backgroundPosition, row++, 1);
    backgroundLayout->addWidget(new QLabel(tr("Maximum size") + u":"_s, this), row, 0);
    backgroundLayout->addWidget(m_backgroundMaxSize, row++, 1);
    backgroundLayout->addWidget(Gui::createSectionHeader(tr("Effects"), this), row++, 0, 1, 2);
    backgroundLayout->addWidget(m_backgroundBlur, row++, 0, 1, 2);
    backgroundLayout->addWidget(m_backgroundOpacity, row++, 0, 1, 2);
    backgroundLayout->addWidget(m_backgroundFadeDuration, row, 0, 1, 2);
    backgroundLayout->setColumnStretch(1, 1);

    auto* layout{contentLayout()};
    layout->addWidget(appearance, 0, 0);
    layout->addWidget(clicks, 1, 0);
    layout->addWidget(background, 0, 1, 2, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    layout->setRowStretch(2, 1);

    QObject::connect(browseAction, &QAction::triggered, this, &PlaylistConfigDialog::browseBackgroundImage);
    QObject::connect(m_backgroundImage, &QComboBox::currentIndexChanged, this,
                     &PlaylistConfigDialog::updateBackgroundControls);
    QObject::connect(m_backgroundScaling, &QComboBox::currentIndexChanged, this,
                     &PlaylistConfigDialog::updateBackgroundControls);
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
    m_scrollBar->setChecked(config.showScrollBar);
    m_alternatingRows->setChecked(config.alternatingRows);
    m_imagePadding->setValue(config.imagePadding);
    m_imagePaddingTop->setValue(config.imagePaddingTop);
    m_artworkCornerRadius->setValue(config.artworkCornerRadius);
    m_backgroundImage->setCurrentIndex(m_backgroundImage->findData(config.backgroundImageMode));
    m_customImage->setText(config.backgroundCustomImage);
    m_backgroundCoverType->setCurrentIndex(m_backgroundCoverType->findData(config.backgroundCoverType));
    m_backgroundTrackPreference->setCurrentIndex(
        m_backgroundTrackPreference->findData(static_cast<int>(config.backgroundTrackPreference)));
    m_backgroundScaling->setCurrentIndex(m_backgroundScaling->findData(config.backgroundScaling));
    m_backgroundPosition->setCurrentIndex(m_backgroundPosition->findData(config.backgroundPosition));
    m_backgroundMaxSize->setValue(config.backgroundMaxSize);
    m_backgroundBlur->setValue(config.backgroundBlur);
    m_backgroundOpacity->setValue(config.backgroundOpacity);
    m_backgroundFadeDuration->setValue(config.backgroundFadeDuration);
    TrackSelectionController::setCurrentAction(m_doubleClick, static_cast<int>(config.doubleClickAction));
    TrackSelectionController::setCurrentAction(m_middleClick, static_cast<int>(config.middleClickAction));
    m_startPlaybackOnSend->setChecked(config.startPlaybackOnSend);
    updateBackgroundControls();
    updateStartPlaybackState();
}

PlaylistWidget::ConfigData PlaylistConfigDialog::config() const
{
    return {
        .showHeader             = m_header->isChecked(),
        .showScrollBar          = m_scrollBar->isChecked(),
        .alternatingRows        = m_alternatingRows->isChecked(),
        .imagePadding           = m_imagePadding->value(),
        .imagePaddingTop        = m_imagePaddingTop->value(),
        .artworkCornerRadius    = m_artworkCornerRadius->value(),
        .backgroundImageMode    = m_backgroundImage->currentData().toInt(),
        .backgroundCustomImage  = m_customImage->text(),
        .backgroundCoverType    = m_backgroundCoverType->currentData().toInt(),
        .backgroundScaling      = m_backgroundScaling->currentData().toInt(),
        .backgroundPosition     = m_backgroundPosition->currentData().toInt(),
        .backgroundMaxSize      = m_backgroundMaxSize->value(),
        .backgroundBlur         = m_backgroundBlur->value(),
        .backgroundOpacity      = m_backgroundOpacity->value(),
        .backgroundFadeDuration = m_backgroundFadeDuration->value(),
        .backgroundTrackPreference
        = static_cast<TrackDisplayPreference>(m_backgroundTrackPreference->currentData().toInt()),
        .doubleClickAction   = static_cast<TrackAction>(m_doubleClick->currentData().toInt()),
        .middleClickAction   = static_cast<TrackAction>(m_middleClick->currentData().toInt()),
        .startPlaybackOnSend = m_startPlaybackOnSend->isChecked(),
    };
}

void PlaylistConfigDialog::mergeExternalConfig(const PlaylistWidget::ConfigData& previous,
                                               const PlaylistWidget::ConfigData& current)
{
    mergeExternalFields(
        previous, current, &PlaylistWidget::ConfigData::showHeader, &PlaylistWidget::ConfigData::showScrollBar,
        &PlaylistWidget::ConfigData::alternatingRows, &PlaylistWidget::ConfigData::imagePadding,
        &PlaylistWidget::ConfigData::imagePaddingTop, &PlaylistWidget::ConfigData::artworkCornerRadius,
        &PlaylistWidget::ConfigData::backgroundImageMode, &PlaylistWidget::ConfigData::backgroundCustomImage,
        &PlaylistWidget::ConfigData::backgroundCoverType, &PlaylistWidget::ConfigData::backgroundScaling,
        &PlaylistWidget::ConfigData::backgroundPosition, &PlaylistWidget::ConfigData::backgroundMaxSize,
        &PlaylistWidget::ConfigData::backgroundBlur, &PlaylistWidget::ConfigData::backgroundOpacity,
        &PlaylistWidget::ConfigData::backgroundFadeDuration, &PlaylistWidget::ConfigData::backgroundTrackPreference,
        &PlaylistWidget::ConfigData::doubleClickAction, &PlaylistWidget::ConfigData::middleClickAction,
        &PlaylistWidget::ConfigData::startPlaybackOnSend);
}

void PlaylistConfigDialog::browseBackgroundImage()
{
    const QString initialPath = m_customImage->text().isEmpty() ? QDir::homePath() : m_customImage->text();
    const QString filename = QFileDialog::getOpenFileName(this, tr("Choose Background Image"), initialPath,
                                                          tr("Images (*.png *.jpg *.jpeg *.webp *.bmp);;All files (*)"),
                                                          nullptr, QFileDialog::DontResolveSymlinks);
    if(!filename.isEmpty()) {
        m_customImage->setText(filename);
    }
}

void PlaylistConfigDialog::updateBackgroundControls() const
{
    const auto imageMode   = static_cast<PlaylistBgImage>(m_backgroundImage->currentData().toInt());
    const bool hasImage    = imageMode != PlaylistBgImage::None;
    const bool customImage = imageMode == PlaylistBgImage::Custom;
    const bool albumCover  = imageMode == PlaylistBgImage::AlbumCover;
    const bool originalSize
        = static_cast<PlaylistBgScaling>(m_backgroundScaling->currentData().toInt()) == PlaylistBgScaling::OriginalSize;

    m_backgroundCoverTypeLabel->setVisible(albumCover);
    m_backgroundCoverType->setVisible(albumCover);
    m_backgroundTrackPreferenceLabel->setVisible(albumCover);
    m_backgroundTrackPreference->setVisible(albumCover);
    m_customImageLabel->setVisible(customImage);
    m_customImage->setVisible(customImage);
    m_backgroundScaling->setEnabled(hasImage);
    m_backgroundPosition->setEnabled(hasImage && originalSize);
    m_backgroundMaxSize->setEnabled(hasImage && originalSize);
    m_backgroundBlur->setEnabled(hasImage);
    m_backgroundOpacity->setEnabled(hasImage);
    m_backgroundFadeDuration->setEnabled(hasImage);
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
