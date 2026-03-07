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

#include "librarytreeconfigwidget.h"

#include "librarytreegroupeditordialog.h"

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

namespace Fooyin {
LibraryTreeConfigDialog::LibraryTreeConfigDialog(LibraryTreeWidget* libraryTree, ActionManager* actionManager,
                                                 LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent)
    : WidgetConfigDialog{libraryTree, tr("Configure %1").arg(libraryTree->name()), parent}
    , m_actionManager{actionManager}
    , m_groupsRegistry{groupsRegistry}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback on send"), this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_keepAlive{new QCheckBox(tr("Keep alive"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_restoreState{new QCheckBox(tr("Restore state on startup"), this)}
    , m_animated{new QCheckBox(tr("Expand/collapse animation"), this)}
    , m_header{new QCheckBox(tr("Show header"), this)}
    , m_showScrollbar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_altColours{new QCheckBox(tr("Alternating row colours"), this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
    , m_manageGroupings{new QPushButton(tr("Manage groupings..."), this)}
{
    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, 2, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(clickBehaviourLayout->columnCount(), 1);

    auto* selectionPlaylist       = new QGroupBox(tr("Library Selection Playlist"), this);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    m_keepAlive->setToolTip(tr("If this is the active playlist, keep it alive when changing selection"));

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_keepAlive, 2, 0, 1, 3);
    selectionPlaylistLayout->addWidget(new QLabel(tr("Name") + u":"_s, this), 3, 0);
    selectionPlaylistLayout->addWidget(m_playlistName, 3, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* generalGroup       = new QGroupBox(tr("General"), this);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    generalGroupLayout->addWidget(m_restoreState);
    generalGroupLayout->addWidget(m_manageGroupings, 2, 0);

    auto* appearanceGroup       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceGroupLayout = new QGridLayout(appearanceGroup);

    auto* iconGroup       = new QGroupBox(tr("Icon"), this);
    auto* iconGroupLayout = new QGridLayout(iconGroup);

    m_iconWidth->setSuffix(u"px"_s);
    m_iconHeight->setSuffix(u"px"_s);
    m_iconWidth->setMaximum(512);
    m_iconHeight->setMaximum(512);
    m_iconWidth->setSingleStep(5);
    m_iconHeight->setSingleStep(5);

    auto* iconSizeHint = new QLabel(
        u"🛈 "_s + tr("Size can also be changed using %1 in the widget.").arg(u"<b>Ctrl+Scroll</b>"_s), this);
    iconSizeHint->setTextFormat(Qt::RichText);

    int row{0};
    iconGroupLayout->addWidget(new QLabel(tr("Width") + u":"_s, this), row, 0);
    iconGroupLayout->addWidget(m_iconWidth, row++, 1);
    iconGroupLayout->addWidget(new QLabel(tr("Height") + u":"_s, this), row, 0);
    iconGroupLayout->addWidget(m_iconHeight, row++, 1);
    iconGroupLayout->addWidget(iconSizeHint, row, 0, 1, 4);
    iconGroupLayout->setColumnStretch(2, 1);

    m_rowHeight->setMinimum(1);

    row = 0;
    appearanceGroupLayout->addWidget(m_animated, row++, 0, 1, 4);
    appearanceGroupLayout->addWidget(m_header, row++, 0, 1, 4);
    appearanceGroupLayout->addWidget(m_showScrollbar, row++, 0, 1, 4);
    appearanceGroupLayout->addWidget(m_altColours, row++, 0, 1, 4);
    appearanceGroupLayout->addWidget(m_overrideRowHeight, row, 0);
    appearanceGroupLayout->addWidget(m_rowHeight, row++, 2);
    appearanceGroupLayout->addWidget(iconGroup, row++, 0, 1, 4);

    appearanceGroupLayout->setColumnStretch(appearanceGroupLayout->columnCount() - 1, 1);
    appearanceGroupLayout->setRowStretch(appearanceGroupLayout->rowCount(), 1);

    auto* mainLayout = contentLayout();
    mainLayout->addWidget(generalGroup, 0, 0);
    mainLayout->addWidget(clickBehaviour, 1, 0);
    mainLayout->addWidget(selectionPlaylist, 2, 0);
    mainLayout->addWidget(appearanceGroup, 3, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action) {
        box->addItem(text, static_cast<int>(action));
    };

    addTrackAction(m_doubleClick, tr("None"), TrackAction::None);
    addTrackAction(m_doubleClick, tr("Expand/Collapse/Play"), TrackAction::Play);
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
    QObject::connect(m_manageGroupings, &QPushButton::clicked, this, [this]() {
        auto* dialog = new LibraryTreeGroupEditorDialog(m_actionManager, m_groupsRegistry, this);
        dialog->open();
    });

    loadCurrentConfig();
}

LibraryTreeWidget::ConfigData LibraryTreeConfigDialog::config() const
{
    return {
        .doubleClickAction = m_doubleClick->currentData().toInt(),
        .middleClickAction = m_middleClick->currentData().toInt(),
        .sendPlayback      = m_playbackOnSend->isChecked(),
        .playlistEnabled   = m_playlistEnabled->isChecked(),
        .autoSwitch        = m_autoSwitch->isChecked(),
        .keepAlive         = m_keepAlive->isChecked(),
        .playlistName      = m_playlistName->text(),
        .restoreState      = m_restoreState->isChecked(),
        .animated          = m_animated->isChecked(),
        .showHeader        = m_header->isChecked(),
        .showScrollbar     = m_showScrollbar->isChecked(),
        .alternatingRows   = m_altColours->isChecked(),
        .rowHeight         = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0,
        .iconSize          = {m_iconWidth->value(), m_iconHeight->value()},
    };
}

void LibraryTreeConfigDialog::setConfig(const LibraryTreeWidget::ConfigData& config)
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
    m_restoreState->setChecked(config.restoreState);
    m_animated->setChecked(config.animated);
    m_header->setChecked(config.showHeader);
    m_showScrollbar->setChecked(config.showScrollbar);
    m_altColours->setChecked(config.alternatingRows);
    m_overrideRowHeight->setChecked(config.rowHeight > 0);
    m_rowHeight->setValue(config.rowHeight > 0 ? config.rowHeight : 1);
    m_rowHeight->setEnabled(m_overrideRowHeight->isChecked());
    m_iconWidth->setValue(config.iconSize.width());
    m_iconHeight->setValue(config.iconSize.height());
    m_playlistName->setEnabled(m_playlistEnabled->isChecked());
    m_autoSwitch->setEnabled(m_playlistEnabled->isChecked());
    m_keepAlive->setEnabled(m_playlistEnabled->isChecked());
}
} // namespace Fooyin
