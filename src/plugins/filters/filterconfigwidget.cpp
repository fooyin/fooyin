/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "filterconfigwidget.h"

#include "filtercolumneditordialog.h"

#include <gui/trackselectioncontroller.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
FilterConfigDialog::FilterConfigDialog(FilterWidget* filterWidget, FilterColumnRegistry* columnRegistry,
                                       QWidget* parent)
    : WidgetConfigDialog{filterWidget, tr("Filter Settings"), parent}
    , m_columnRegistry{columnRegistry}
    , m_source{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_preservePlaybackPlaylist{new QCheckBox(tr("Preserve playback playlist"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
    , m_iconHorizontalGap{new QSpinBox(this)}
    , m_iconVerticalGap{new QSpinBox(this)}
    , m_artworkCornerRadius{new QSpinBox(this)}
    , m_alignCaptionsToArtwork{new QCheckBox(tr("Align labels to artwork"), this)}
    , m_manageColumns{new QPushButton(tr("Manage columns..."), this)}
{
    m_playbackOnSend->setToolTip(
        tr("For \"Replace current playlist\" and \"Create new playlist\", start playback immediately."));

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, 2, 0, 1, 2);
    auto* playlistClickHint
        = new QLabel(u"🛈 "_s + tr("Set to <b>Play</b> to start playback at the first matching track."), this);
    playlistClickHint->setWordWrap(true);
    clickBehaviourLayout->addWidget(playlistClickHint, 3, 0, 1, 3);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* selectionPlaylist       = new QGroupBox(tr("Filter Selection Playlist"), this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);
    selectionPlaylist->setToolTip(
        tr("In current playlist mode, matching tracks are selected directly in the playlist."));

    m_preservePlaybackPlaylist->setToolTip(
        tr("When this selection playlist is used for playback, preserve it with \"(Playback)\" appended to its "
           "name instead of replacing its tracks."));

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_preservePlaybackPlaylist, 2, 0, 1, 3);
    selectionPlaylistLayout->addWidget(new QLabel(tr("Name") + u":"_s, this), 3, 0);
    selectionPlaylistLayout->addWidget(m_playlistName, 3, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    m_alignCaptionsToArtwork->setToolTip(
        tr("Align bottom labels to the horizontal bounds of the artwork in artwork mode."));

    appearanceLayout->addWidget(m_overrideRowHeight, 0, 0, 1, 2);
    appearanceLayout->addWidget(m_rowHeight, 0, 2);
    appearanceLayout->addWidget(m_alignCaptionsToArtwork, 1, 0, 1, 3);
    appearanceLayout->setColumnStretch(3, 1);

    auto* artworkMode   = new QGroupBox(tr("Artwork Mode"), this);
    auto* artworkLayout = new QGridLayout(artworkMode);

    m_iconWidth->setSuffix(u" px"_s);
    m_iconHeight->setSuffix(u" px"_s);
    m_iconWidth->setMaximum(2048);
    m_iconHeight->setMaximum(2048);
    m_iconWidth->setSingleStep(20);
    m_iconHeight->setSingleStep(20);
    m_iconHorizontalGap->setRange(-1, 256);
    m_iconHorizontalGap->setSpecialValueText(tr("Auto"));
    m_iconHorizontalGap->setSuffix(u" px"_s);
    m_iconVerticalGap->setRange(0, 256);
    m_iconVerticalGap->setSuffix(u" px"_s);
    m_artworkCornerRadius->setRange(0, 100);
    m_artworkCornerRadius->setSuffix(u" %"_s);
    m_artworkCornerRadius->setSpecialValueText(tr("Square"));

    auto* iconSizeHint = new QLabel(u"🛈 "_s + tr("Use <b>Ctrl+Scroll</b> in the widget to resize icons."), this);

    int row{0};
    artworkLayout->addWidget(new QLabel(tr("Width") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_iconWidth, row++, 1);
    artworkLayout->addWidget(new QLabel(tr("Height") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_iconHeight, row++, 1);
    artworkLayout->addWidget(iconSizeHint, row++, 0, 1, 4);
    artworkLayout->addWidget(new QLabel(tr("Horizontal gap") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_iconHorizontalGap, row, 1);
    artworkLayout->addWidget(new QLabel(tr("Vertical gap") + u":"_s, this), row, 2);
    artworkLayout->addWidget(m_iconVerticalGap, row++, 3);
    artworkLayout->addWidget(new QLabel(tr("Corner radius") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_artworkCornerRadius, row++, 1);
    artworkLayout->setColumnStretch(4, 1);

    auto* generalGroup       = new QGroupBox(tr("General"), this);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    m_source->addItem(tr("Library"), static_cast<int>(FilterSource::Library));
    m_source->addItem(tr("Current playlist"), static_cast<int>(FilterSource::CurrentPlaylist));

    generalGroupLayout->addWidget(new QLabel(tr("Source") + u":"_s, this), 0, 0);
    generalGroupLayout->addWidget(m_source, 0, 1);
    auto* playlistSourceHint = new QLabel(
        u"🛈 "_s
            + tr("Current playlist mode uses the displayed playlist as its source and selects the matching "
                 "tracks in that playlist."),
        this);
    playlistSourceHint->setWordWrap(true);
    generalGroupLayout->addWidget(playlistSourceHint, 1, 0, 1, 3);
    generalGroupLayout->addWidget(m_manageColumns, 2, 0, 1, 3);
    generalGroupLayout->setColumnStretch(2, 1);

    auto* mainLayout = contentLayout();

    row = 0;
    mainLayout->addWidget(generalGroup, row++, 0);
    mainLayout->addWidget(clickBehaviour, row++, 0);
    mainLayout->addWidget(selectionPlaylist, row++, 0);
    mainLayout->addWidget(appearance, row++, 0);
    mainLayout->addWidget(artworkMode, row++, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    TrackSelectionController::addAction(m_doubleClick, tr("None"), TrackAction::None);
    TrackSelectionController::addAction(m_doubleClick, tr("Play"), TrackAction::Play);
    TrackSelectionController::addStandardActions(m_doubleClick);

    TrackSelectionController::addAction(m_middleClick, tr("None"), TrackAction::None);
    TrackSelectionController::addAction(m_middleClick, tr("Play"), TrackAction::Play);
    TrackSelectionController::addStandardActions(m_middleClick);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
    QObject::connect(m_playlistEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
        m_preservePlaybackPlaylist->setEnabled(checked);
    });

    const auto updateSourceModeUi = [this, selectionPlaylist]() {
        const bool playlistSource = m_source->currentData().toInt() == static_cast<int>(FilterSource::CurrentPlaylist);
        selectionPlaylist->setEnabled(!playlistSource);
    };
    QObject::connect(m_source, &QComboBox::currentIndexChanged, selectionPlaylist, updateSourceModeUi);
    updateSourceModeUi();

    QObject::connect(m_manageColumns, &QPushButton::clicked, this, [this]() {
        auto* dialog = new FilterColumnEditorDialog(m_columnRegistry, this);
        dialog->open();
    });

    QObject::connect(filterWidget, &FilterWidget::configChanged, this, &FilterConfigDialog::syncCurrentConfig);

    loadCurrentConfig();
}

FilterWidget::ConfigData FilterConfigDialog::config() const
{
    return {
        .doubleClickAction        = m_doubleClick->currentData().toInt(),
        .middleClickAction        = m_middleClick->currentData().toInt(),
        .sendPlayback             = m_playbackOnSend->isChecked(),
        .source                   = static_cast<FilterSource>(m_source->currentData().toInt()),
        .playlistEnabled          = m_playlistEnabled->isChecked(),
        .autoSwitch               = m_autoSwitch->isChecked(),
        .preservePlaybackPlaylist = m_preservePlaybackPlaylist->isChecked(),
        .playlistName             = m_playlistName->text(),
        .rowHeight                = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0,
        .iconSize                 = {m_iconWidth->value(), m_iconHeight->value()},
        .iconHorizontalGap        = m_iconHorizontalGap->value(),
        .iconVerticalGap          = m_iconVerticalGap->value(),
        .artworkCornerRadius      = m_artworkCornerRadius->value(),
        .alignCaptionsToArtwork   = m_alignCaptionsToArtwork->isChecked(),
    };
}

void FilterConfigDialog::setConfig(const FilterWidget::ConfigData& config)
{
    TrackSelectionController::setCurrentAction(m_doubleClick, config.doubleClickAction);
    TrackSelectionController::setCurrentAction(m_middleClick, config.middleClickAction);

    m_playbackOnSend->setChecked(config.sendPlayback);
    m_source->setCurrentIndex(m_source->findData(static_cast<int>(config.source)));
    m_playlistEnabled->setChecked(config.playlistEnabled);
    m_autoSwitch->setChecked(config.autoSwitch);
    m_preservePlaybackPlaylist->setChecked(config.preservePlaybackPlaylist);
    m_playlistName->setText(config.playlistName);
    m_overrideRowHeight->setChecked(config.rowHeight > 0);
    m_rowHeight->setValue(config.rowHeight > 0 ? config.rowHeight : 1);
    m_rowHeight->setEnabled(m_overrideRowHeight->isChecked());
    m_iconWidth->setValue(config.iconSize.width());
    m_iconHeight->setValue(config.iconSize.height());
    m_iconHorizontalGap->setValue(config.iconHorizontalGap);
    m_iconVerticalGap->setValue(config.iconVerticalGap);
    m_artworkCornerRadius->setValue(config.artworkCornerRadius);
    m_alignCaptionsToArtwork->setChecked(config.alignCaptionsToArtwork);
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());
    m_preservePlaybackPlaylist->setEnabled(m_playlistEnabled->isChecked());
}

void FilterConfigDialog::mergeExternalConfig(const FilterWidget::ConfigData& previous,
                                             const FilterWidget::ConfigData& current)
{
    mergeExternalFields(previous, current, &FilterWidget::ConfigData::doubleClickAction,
                        &FilterWidget::ConfigData::middleClickAction, &FilterWidget::ConfigData::sendPlayback,
                        &FilterWidget::ConfigData::source, &FilterWidget::ConfigData::playlistEnabled,
                        &FilterWidget::ConfigData::autoSwitch, &FilterWidget::ConfigData::preservePlaybackPlaylist,
                        &FilterWidget::ConfigData::playlistName, &FilterWidget::ConfigData::rowHeight,
                        &FilterWidget::ConfigData::iconSize, &FilterWidget::ConfigData::iconHorizontalGap,
                        &FilterWidget::ConfigData::iconVerticalGap, &FilterWidget::ConfigData::artworkCornerRadius,
                        &FilterWidget::ConfigData::alignCaptionsToArtwork);
}
} // namespace Fooyin::Filters
