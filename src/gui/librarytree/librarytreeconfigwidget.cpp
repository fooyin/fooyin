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

#include "librarytreeconfigwidget.h"

#include "librarytreegroupeditordialog.h"
#include "librarytreescriptenvironment.h"

#include <gui/trackselectioncontroller.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

using namespace Qt::StringLiterals;

namespace Fooyin {
LibraryTreeConfigDialog::LibraryTreeConfigDialog(LibraryTreeWidget* libraryTree,
                                                 LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent)
    : WidgetConfigDialog{libraryTree, tr("Library Tree Settings"), parent}
    , m_groupsRegistry{groupsRegistry}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_keepAlive{new QCheckBox(tr("Keep alive"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_restoreState{new QCheckBox(tr("Restore state on startup"), this)}
    , m_expandOnSingleClick{new QCheckBox(tr("Single-click expands/collapses nodes"), this)}
    , m_showSummaryNode{new QCheckBox(tr("Show summary node"), this)}
    , m_summaryNodeTitle{new QLineEdit(this)}
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
    m_playbackOnSend->setToolTip(
        tr(R"(For "Replace current playlist" and "Create new playlist", start playback immediately.)"));
    m_summaryNodeTitle->setPlaceholderText(defaultLibraryTreeSummaryTitle());
    m_summaryNodeTitle->setToolTip(
        tr("Supports <right> for right-aligned text, %trackcount% for tracks, and %childcount% for child nodes."));

    auto* tabs = new QTabWidget(this);

    auto* generalTab = new QWidget(tabs);
    auto* styleTab   = new QWidget(tabs);

    tabs->addTab(generalTab, tr("General"));
    tabs->addTab(styleTab, tr("Appearance"));

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), generalTab);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    int row{0};
    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), row, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, row++, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), row, 0);
    clickBehaviourLayout->addWidget(m_middleClick, row++, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, row++, 0, 1, 2);
    clickBehaviourLayout->addWidget(m_expandOnSingleClick, row++, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(clickBehaviourLayout->columnCount(), 1);

    auto* selectionPlaylist       = new QGroupBox(tr("Library Selection Playlist"), generalTab);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    m_keepAlive->setToolTip(tr("If this is the active playlist, keep it alive when changing selection"));

    row = 0;
    selectionPlaylistLayout->addWidget(m_playlistEnabled, row++, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, row++, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_keepAlive, row++, 0, 1, 3);
    selectionPlaylistLayout->addWidget(new QLabel(tr("Name") + u":"_s, this), row, 0);
    selectionPlaylistLayout->addWidget(m_playlistName, row++, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    auto* generalGroup       = new QGroupBox(tr("General"), generalTab);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    row = 0;
    generalGroupLayout->addWidget(m_restoreState, row++, 0, 1, 3);
    generalGroupLayout->addWidget(m_showSummaryNode, row++, 0, 1, 3);
    generalGroupLayout->addWidget(new QLabel(tr("Summary title") + u":"_s, this), row, 0);
    generalGroupLayout->addWidget(m_summaryNodeTitle, row++, 1, 1, 2);
    generalGroupLayout->addWidget(m_manageGroupings, row++, 0, 1, 3);
    generalGroupLayout->setColumnStretch(2, 1);

    auto* appearanceGroup       = new QGroupBox(tr("Appearance"), styleTab);
    auto* appearanceGroupLayout = new QGridLayout(appearanceGroup);

    auto* iconGroup       = new QGroupBox(tr("Icon"), styleTab);
    auto* iconGroupLayout = new QGridLayout(iconGroup);

    m_iconWidth->setSuffix(u" px"_s);
    m_iconHeight->setSuffix(u" px"_s);
    m_iconWidth->setMaximum(512);
    m_iconHeight->setMaximum(512);
    m_iconWidth->setSingleStep(5);
    m_iconHeight->setSingleStep(5);

    auto* iconSizeHint = new QLabel(u"🛈 "_s + tr("Use <b>Ctrl+Scroll</b> in the widget to resize icons."), this);
    iconSizeHint->setTextFormat(Qt::RichText);

    row = 0;
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

    auto* generalLayout = new QGridLayout(generalTab);

    row = 0;
    generalLayout->addWidget(generalGroup, row++, 0);
    generalLayout->addWidget(clickBehaviour, row++, 0);
    generalLayout->addWidget(selectionPlaylist, row++, 0);
    generalLayout->setColumnStretch(0, 1);
    generalLayout->setRowStretch(3, 1);

    auto* styleLayout = new QGridLayout(styleTab);

    row = 0;
    styleLayout->addWidget(appearanceGroup, row++, 0);
    styleLayout->setColumnStretch(0, 1);
    styleLayout->setRowStretch(1, 1);

    auto* mainLayout{contentLayout()};
    row = 0;
    mainLayout->addWidget(tabs, row++, 0);
    mainLayout->setColumnStretch(0, 1);
    mainLayout->setRowStretch(0, 1);

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action) {
        box->addItem(text, static_cast<int>(action));
    };

    addTrackAction(m_doubleClick, tr("Expand/collapse"), TrackAction::None);
    addTrackAction(m_doubleClick, tr("Expand/collapse or play"), TrackAction::Play);
    addTrackAction(m_doubleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Add to current playlist and play if stopped"),
                   TrackAction::AddCurrentPlaylistAndPlayIfStopped);
    addTrackAction(m_doubleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_doubleClick, tr("Replace current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Create new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_doubleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_doubleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_doubleClick, tr("Replace playback queue"), TrackAction::SendToQueue);

    addTrackAction(m_middleClick, tr("None"), TrackAction::None);
    addTrackAction(m_middleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Add to current playlist and play if stopped"),
                   TrackAction::AddCurrentPlaylistAndPlayIfStopped);
    addTrackAction(m_middleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_middleClick, tr("Replace current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Create new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_middleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_middleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_middleClick, tr("Replace playback queue"), TrackAction::SendToQueue);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
    QObject::connect(m_showSummaryNode, &QCheckBox::toggled, m_summaryNodeTitle, &QWidget::setEnabled);
    QObject::connect(m_playlistEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        m_playlistName->setEnabled(checked);
        m_autoSwitch->setEnabled(checked);
        m_keepAlive->setEnabled(checked);
    });
    QObject::connect(m_manageGroupings, &QPushButton::clicked, this, [this]() {
        auto* dialog = new LibraryTreeGroupEditorDialog(m_groupsRegistry, this);
        dialog->open();
    });

    loadCurrentConfig();
}

LibraryTreeWidget::ConfigData LibraryTreeConfigDialog::config() const
{
    return {
        .doubleClickAction   = m_doubleClick->currentData().toInt(),
        .middleClickAction   = m_middleClick->currentData().toInt(),
        .sendPlayback        = m_playbackOnSend->isChecked(),
        .playlistEnabled     = m_playlistEnabled->isChecked(),
        .autoSwitch          = m_autoSwitch->isChecked(),
        .keepAlive           = m_keepAlive->isChecked(),
        .playlistName        = m_playlistName->text(),
        .restoreState        = m_restoreState->isChecked(),
        .expandOnSingleClick = m_expandOnSingleClick->isChecked(),
        .animated            = m_animated->isChecked(),
        .showHeader          = m_header->isChecked(),
        .showScrollbar       = m_showScrollbar->isChecked(),
        .alternatingRows     = m_altColours->isChecked(),
        .showSummaryNode     = m_showSummaryNode->isChecked(),
        .summaryNodeTitle    = m_summaryNodeTitle->text(),
        .rowHeight           = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0,
        .iconSize            = {m_iconWidth->value(), m_iconHeight->value()},
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
    m_expandOnSingleClick->setChecked(config.expandOnSingleClick);
    m_animated->setChecked(config.animated);
    m_header->setChecked(config.showHeader);
    m_showScrollbar->setChecked(config.showScrollbar);
    m_altColours->setChecked(config.alternatingRows);
    m_showSummaryNode->setChecked(config.showSummaryNode);
    m_summaryNodeTitle->setText(config.summaryNodeTitle);
    m_summaryNodeTitle->setEnabled(m_showSummaryNode->isChecked());
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
