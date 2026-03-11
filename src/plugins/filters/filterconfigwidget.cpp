/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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
FilterConfigDialog::FilterConfigDialog(FilterWidget* filterWidget, ActionManager* actionManager,
                                       FilterColumnRegistry* columnRegistry, QWidget* parent)
    : WidgetConfigDialog{filterWidget, tr("Configure %1").arg(filterWidget->name()), parent}
    , m_actionManager{actionManager}
    , m_columnRegistry{columnRegistry}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback on send"), this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_keepAlive{new QCheckBox(tr("Keep alive"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
    , m_iconHorizontalGap{new QSpinBox(this)}
    , m_iconVerticalGap{new QSpinBox(this)}
    , m_manageColumns{new QPushButton(tr("Manage columns..."), this)}
{
    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, 2, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* selectionPlaylist       = new QGroupBox(tr("Filter Selection Playlist"), this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    m_keepAlive->setToolTip(tr("If this is the active playlist, keep it alive when changing selection"));

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_keepAlive, 2, 0, 1, 3);
    selectionPlaylistLayout->addWidget(new QLabel(tr("Name") + u":"_s, this), 3, 0);
    selectionPlaylistLayout->addWidget(m_playlistName, 3, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    appearanceLayout->addWidget(m_overrideRowHeight, 0, 0, 1, 2);
    appearanceLayout->addWidget(m_rowHeight, 0, 2);
    appearanceLayout->setColumnStretch(3, 1);

    auto* artworkMode   = new QGroupBox(tr("Artwork Mode"), this);
    auto* artworkLayout = new QGridLayout(artworkMode);

    m_iconWidth->setSuffix(u"px"_s);
    m_iconHeight->setSuffix(u"px"_s);
    m_iconWidth->setMaximum(2048);
    m_iconHeight->setMaximum(2048);
    m_iconWidth->setSingleStep(20);
    m_iconHeight->setSingleStep(20);
    m_iconHorizontalGap->setRange(-1, 256);
    m_iconHorizontalGap->setSpecialValueText(tr("Auto"));
    m_iconHorizontalGap->setSuffix(u"px"_s);
    m_iconVerticalGap->setRange(0, 256);
    m_iconVerticalGap->setSuffix(u"px"_s);

    auto* iconSizeHint = new QLabel(
        u"🛈 "_s + tr("Size can also be changed using %1 in the widget.").arg(u"<b>Ctrl+Scroll</b>"_s), this);

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
    artworkLayout->setColumnStretch(4, 1);

    auto* generalGroup       = new QGroupBox(tr("General"), this);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    generalGroupLayout->addWidget(m_manageColumns, 1, 0);

    auto* mainLayout = contentLayout();

    row = 0;
    mainLayout->addWidget(generalGroup, row++, 0);
    mainLayout->addWidget(clickBehaviour, row++, 0);
    mainLayout->addWidget(selectionPlaylist, row++, 0);
    mainLayout->addWidget(appearance, row++, 0);
    mainLayout->addWidget(artworkMode, row++, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action) {
        box->addItem(text, static_cast<int>(action));
    };

    addTrackAction(m_doubleClick, tr("None"), TrackAction::None);
    addTrackAction(m_doubleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_doubleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_doubleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_doubleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_doubleClick, tr("Send to playback queue"), TrackAction::SendToQueue);

    addTrackAction(m_middleClick, tr("None"), TrackAction::None);
    addTrackAction(m_middleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_middleClick, tr("Send to current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Send to new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_middleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_middleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_middleClick, tr("Send to playback queue"), TrackAction::SendToQueue);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
    QObject::connect(m_playlistEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
        m_keepAlive->setEnabled(checked);
    });
    QObject::connect(m_manageColumns, &QPushButton::clicked, this, [this]() {
        auto* dialog = new FilterColumnEditorDialog(m_actionManager, m_columnRegistry, this);
        dialog->open();
    });

    loadCurrentConfig();
}

FilterWidget::ConfigData FilterConfigDialog::config() const
{
    return {
        .doubleClickAction = m_doubleClick->currentData().toInt(),
        .middleClickAction = m_middleClick->currentData().toInt(),
        .sendPlayback      = m_playbackOnSend->isChecked(),
        .playlistEnabled   = m_playlistEnabled->isChecked(),
        .autoSwitch        = m_autoSwitch->isChecked(),
        .keepAlive         = m_keepAlive->isChecked(),
        .playlistName      = m_playlistName->text(),
        .rowHeight         = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0,
        .iconSize          = {m_iconWidth->value(), m_iconHeight->value()},
        .iconHorizontalGap = m_iconHorizontalGap->value(),
        .iconVerticalGap   = m_iconVerticalGap->value(),
    };
}

void FilterConfigDialog::setConfig(const FilterWidget::ConfigData& config)
{
    if(const int index = m_doubleClick->findData(config.doubleClickAction); index >= 0) {
        m_doubleClick->setCurrentIndex(index);
    }

    if(const int index = m_middleClick->findData(config.middleClickAction); index >= 0) {
        m_middleClick->setCurrentIndex(index);
    }

    m_playbackOnSend->setChecked(config.sendPlayback);
    m_playlistEnabled->setChecked(config.playlistEnabled);
    m_autoSwitch->setChecked(config.autoSwitch);
    m_keepAlive->setChecked(config.keepAlive);
    m_playlistName->setText(config.playlistName);
    m_overrideRowHeight->setChecked(config.rowHeight > 0);
    m_rowHeight->setValue(config.rowHeight > 0 ? config.rowHeight : 1);
    m_rowHeight->setEnabled(m_overrideRowHeight->isChecked());
    m_iconWidth->setValue(config.iconSize.width());
    m_iconHeight->setValue(config.iconSize.height());
    m_iconHorizontalGap->setValue(config.iconHorizontalGap);
    m_iconVerticalGap->setValue(config.iconVerticalGap);
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());
    m_keepAlive->setEnabled(m_playlistEnabled->isChecked());
}
} // namespace Fooyin::Filters
