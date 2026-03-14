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

#include "dirbrowserconfigwidget.h"

#include <gui/trackselectioncontroller.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
DirBrowserConfigDialog::DirBrowserConfigDialog(DirBrowser* browser, QWidget* parent)
    : WidgetConfigDialog{browser, tr("Configure %1").arg(browser->name()), parent}
    , m_treeMode{new QRadioButton(tr("Tree"), this)}
    , m_listMode{new QRadioButton(tr("List"), this)}
    , m_showIcons{new QCheckBox(tr("Show icons"), this)}
    , m_indentList{new QCheckBox(tr("Show indent"), this)}
    , m_showHorizScrollbar{new QCheckBox(tr("Show horizontal scrollbar"), this)}
    , m_showControls{new QCheckBox(tr("Show controls"), this)}
    , m_showLocation{new QCheckBox(tr("Show location"), this)}
    , m_showSymLinks{new QCheckBox(tr("Show symlinks"), this)}
    , m_showHidden{new QCheckBox(tr("Show hidden"), this)}
    , m_doubleClick{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
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
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* browserMode       = new QGroupBox(tr("Browser Mode"), this);
    auto* browserModeLayout = new QVBoxLayout(browserMode);
    browserModeLayout->addWidget(m_treeMode);
    browserModeLayout->addWidget(m_listMode);
    browserModeLayout->addStretch();

    auto* browserFilters       = new QGroupBox(tr("Browser Filters"), this);
    auto* browserFiltersLayout = new QVBoxLayout(browserFilters);
    browserFiltersLayout->addWidget(m_showSymLinks);
    browserFiltersLayout->addWidget(m_showHidden);

    auto* displayOptions       = new QGroupBox(tr("Display Options"), this);
    auto* displayOptionsLayout = new QGridLayout(displayOptions);
    displayOptionsLayout->addWidget(m_showIcons, 0, 0);
    displayOptionsLayout->addWidget(m_indentList, 1, 0);
    displayOptionsLayout->addWidget(m_showHorizScrollbar, 2, 0);
    displayOptionsLayout->addWidget(m_showControls, 3, 0);
    displayOptionsLayout->addWidget(m_showLocation, 4, 0);

    auto* mainLayout = contentLayout();
    mainLayout->addWidget(clickBehaviour, 0, 0);
    mainLayout->addWidget(browserMode, 1, 0);
    mainLayout->addWidget(browserFilters, 2, 0);
    mainLayout->addWidget(displayOptions, 3, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    auto addTrackAction = [](QComboBox* box, const QString& text, TrackAction action) {
        box->addItem(text, static_cast<int>(action));
    };

    addTrackAction(m_doubleClick, tr("None"), TrackAction::None);
    addTrackAction(m_doubleClick, tr("Expand/Collapse/Play"), TrackAction::Play);
    addTrackAction(m_doubleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_doubleClick, tr("Replace current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_doubleClick, tr("Create new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_doubleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_doubleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_doubleClick, tr("Replace playback queue"), TrackAction::SendToQueue);

    addTrackAction(m_middleClick, tr("None"), TrackAction::None);
    addTrackAction(m_middleClick, tr("Add to current playlist"), TrackAction::AddCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Add to active playlist"), TrackAction::AddActivePlaylist);
    addTrackAction(m_middleClick, tr("Replace current playlist"), TrackAction::SendCurrentPlaylist);
    addTrackAction(m_middleClick, tr("Create new playlist"), TrackAction::SendNewPlaylist);
    addTrackAction(m_middleClick, tr("Add to playback queue"), TrackAction::AddToQueue);
    addTrackAction(m_middleClick, tr("Add to front of playback queue"), TrackAction::QueueNext);
    addTrackAction(m_middleClick, tr("Replace playback queue"), TrackAction::SendToQueue);

    QObject::connect(m_treeMode, &QRadioButton::toggled, this,
                     [this](bool checked) { m_indentList->setEnabled(!checked); });

    loadCurrentConfig();
}

DirBrowser::ConfigData DirBrowserConfigDialog::config() const
{
    return {
        .doubleClickAction  = m_doubleClick->currentData().toInt(),
        .middleClickAction  = m_middleClick->currentData().toInt(),
        .sendPlayback       = m_playbackOnSend->isChecked(),
        .showIcons          = m_showIcons->isChecked(),
        .indentList         = m_indentList->isChecked(),
        .showHorizScrollbar = m_showHorizScrollbar->isChecked(),
        .mode               = m_listMode->isChecked() ? DirBrowser::Mode::List : DirBrowser::Mode::Tree,
        .showControls       = m_showControls->isChecked(),
        .showLocation       = m_showLocation->isChecked(),
        .showSymLinks       = m_showSymLinks->isChecked(),
        .showHidden         = m_showHidden->isChecked(),
        .rootPath           = widget()->currentConfig().rootPath,
    };
}

void DirBrowserConfigDialog::setConfig(const DirBrowser::ConfigData& config)
{
    m_showIcons->setChecked(config.showIcons);
    m_indentList->setChecked(config.indentList);
    m_showHorizScrollbar->setChecked(config.showHorizScrollbar);
    m_showControls->setChecked(config.showControls);
    m_showLocation->setChecked(config.showLocation);
    m_showSymLinks->setChecked(config.showSymLinks);
    m_showHidden->setChecked(config.showHidden);
    m_playbackOnSend->setChecked(config.sendPlayback);

    if(config.mode == DirBrowser::Mode::List) {
        m_listMode->setChecked(true);
    }
    else {
        m_treeMode->setChecked(true);
    }

    if(const int index = m_doubleClick->findData(config.doubleClickAction); index >= 0) {
        m_doubleClick->setCurrentIndex(index);
    }
    if(const int index = m_middleClick->findData(config.middleClickAction); index >= 0) {
        m_middleClick->setCurrentIndex(index);
    }
}
} // namespace Fooyin
