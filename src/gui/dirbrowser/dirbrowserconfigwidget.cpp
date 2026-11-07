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

#include "dirbrowserconfigwidget.h"

#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/backgroundconfigwidget.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
DirBrowserConfigDialog::DirBrowserConfigDialog(DirBrowser* browser, QWidget* parent)
    : WidgetConfigDialog{browser, tr("Directory Browser Settings"), parent}
    , m_treeMode{new QRadioButton(tr("Tree"), this)}
    , m_listMode{new QRadioButton(tr("List"), this)}
    , m_showIcons{new QCheckBox(tr("Show icons"), this)}
    , m_indentList{new QCheckBox(tr("Show indent"), this)}
    , m_showHeader{new QCheckBox(tr("Show header"), this)}
    , m_restoreSort{new QCheckBox(tr("Remember sort column"), this)}
    , m_controlsPosition{new QComboBox(this)}
    , m_showControls{new QCheckBox(tr("Show controls"), this)}
    , m_showLocation{new QCheckBox(tr("Show location"), this)}
    , m_showSymLinks{new QCheckBox(tr("Show symlinks"), this)}
    , m_showHidden{new QCheckBox(tr("Show hidden"), this)}
    , m_doubleClick{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
    , m_playingRowFont{new FontButton(tr("Font") + u":"_s, true, this)}
    , m_playingRowColour{new ColourButton(tr("Background colour") + u":"_s, true, this)}
    , m_background{new BackgroundConfigWidget(this)}
{
    m_playbackOnSend->setToolTip(
        tr("For ‘Replace current playlist’ and ‘Create new playlist’, start playback immediately."));
    m_restoreSort->setToolTip(tr("Restore the sort column on startup and when changing directories."));

    auto* generalPage          = new QWidget(this);
    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), generalPage);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), 0, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 0, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), 1, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 1, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, 2, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(2, 1);

    auto* browserMode       = new QGroupBox(tr("Browser Mode"), generalPage);
    auto* browserModeLayout = new QVBoxLayout(browserMode);
    browserModeLayout->addWidget(m_treeMode);
    browserModeLayout->addWidget(m_listMode);
    browserModeLayout->addStretch();

    auto* browserFilters       = new QGroupBox(tr("Browser Filters"), generalPage);
    auto* browserFiltersLayout = new QVBoxLayout(browserFilters);
    browserFiltersLayout->addWidget(m_showSymLinks);
    browserFiltersLayout->addWidget(m_showHidden);

    m_controlsPosition->addItem(tr("Top"), static_cast<int>(DirBrowser::ControlsPosition::Top));
    m_controlsPosition->addItem(tr("Bottom"), static_cast<int>(DirBrowser::ControlsPosition::Bottom));

    auto* displayOptions       = new QGroupBox(tr("Display Options"), generalPage);
    auto* displayOptionsLayout = new QGridLayout(displayOptions);

    displayOptionsLayout->addWidget(m_showIcons, 0, 0, 1, 2);
    displayOptionsLayout->addWidget(m_indentList, 0, 2, 1, 2);
    displayOptionsLayout->addWidget(m_showHeader, 1, 0, 1, 2);
    displayOptionsLayout->addWidget(m_showControls, 1, 2, 1, 2);
    displayOptionsLayout->addWidget(m_showLocation, 2, 0, 1, 2);
    displayOptionsLayout->addWidget(new QLabel(tr("Path and controls position") + u":"_s, this), 2, 2);
    displayOptionsLayout->addWidget(m_controlsPosition, 2, 3);
    displayOptionsLayout->addWidget(m_restoreSort, 3, 0, 1, 4);
    displayOptionsLayout->setColumnStretch(1, 1);
    displayOptionsLayout->setColumnStretch(3, 1);

    const int playingRowLabelWidth = std::max(m_playingRowColour->labelWidthHint(), m_playingRowFont->labelWidthHint());
    m_playingRowColour->setLabelWidth(playingRowLabelWidth);
    m_playingRowFont->setLabelWidth(playingRowLabelWidth);
    m_playingRowColour->setToolTip(
        tr("Use a custom background colour for the currently playing row; transparency is supported"));
    m_playingRowFont->setToolTip(tr("Use a custom font for the currently playing row"));

    auto* playingRowGroup  = new QGroupBox(tr("Playing Row"), generalPage);
    auto* playingRowLayout = new QGridLayout(playingRowGroup);

    playingRowLayout->addWidget(m_playingRowColour, 0, 0);
    playingRowLayout->addWidget(m_playingRowFont, 1, 0);

    auto* mainLayout = new QGridLayout(generalPage);

    mainLayout->addWidget(clickBehaviour, 0, 0, 1, 2);
    mainLayout->addWidget(browserMode, 1, 0);
    mainLayout->addWidget(browserFilters, 1, 1);
    mainLayout->addWidget(displayOptions, 2, 0, 1, 2);
    mainLayout->addWidget(playingRowGroup, 3, 0, 1, 2);
    mainLayout->setColumnStretch(0, 1);
    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(4, 1);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(generalPage, tr("General"));
    tabs->addTab(m_background, tr("Background"));
    contentLayout()->addWidget(tabs);

    TrackSelectionController::addAction(m_doubleClick, tr("Expand/collapse"), TrackAction::None);
    TrackSelectionController::addAction(m_doubleClick, tr("Expand/collapse or play now"), TrackAction::Play);
    TrackSelectionController::addStandardActions(m_doubleClick);

    TrackSelectionController::addAction(m_middleClick, tr("None"), TrackAction::None);
    TrackSelectionController::addStandardActions(m_middleClick);

    QObject::connect(m_treeMode, &QRadioButton::toggled, this,
                     [this](bool checked) { m_indentList->setEnabled(!checked); });

    QObject::connect(browser, &DirBrowser::configChanged, this, &DirBrowserConfigDialog::syncCurrentConfig);

    loadCurrentConfig();
}

void DirBrowserConfigDialog::changeEvent(QEvent* event)
{
    WidgetConfigDialog::changeEvent(event);

    if(event->type() == QEvent::PaletteChange && !m_playingRowColour->isChecked()) {
        m_playingRowColour->setColour(Gui::defaultPlayingRowColour());
    }
}

DirBrowser::ConfigData DirBrowserConfigDialog::config() const
{
    return {
        .doubleClickAction = m_doubleClick->currentData().toInt(),
        .middleClickAction = m_middleClick->currentData().toInt(),
        .sendPlayback      = m_playbackOnSend->isChecked(),
        .showIcons         = m_showIcons->isChecked(),
        .indentList        = m_indentList->isChecked(),
        .showHeader        = m_showHeader->isChecked(),
        .restoreSort       = m_restoreSort->isChecked(),
        .mode              = m_listMode->isChecked() ? DirBrowser::Mode::List : DirBrowser::Mode::Tree,
        .controlsPosition  = static_cast<DirBrowser::ControlsPosition>(m_controlsPosition->currentData().toInt()),
        .showControls      = m_showControls->isChecked(),
        .showLocation      = m_showLocation->isChecked(),
        .showSymLinks      = m_showSymLinks->isChecked(),
        .showHidden        = m_showHidden->isChecked(),
        .rootPath          = widget()->currentConfig().rootPath,
        .playingRowFont    = m_playingRowFont->isChecked() ? m_playingRowFont->buttonFont().toString() : QString{},
        .playingRowColour
        = m_playingRowColour->isChecked() ? m_playingRowColour->colour().name(QColor::HexArgb) : QString{},
        .background = m_background->config(),
    };
}

void DirBrowserConfigDialog::setConfig(const DirBrowser::ConfigData& config)
{
    m_showIcons->setChecked(config.showIcons);
    m_indentList->setChecked(config.indentList);
    m_showHeader->setChecked(config.showHeader);
    m_restoreSort->setChecked(config.restoreSort);
    m_controlsPosition->setCurrentIndex(m_controlsPosition->findData(static_cast<int>(config.controlsPosition)));
    m_showControls->setChecked(config.showControls);
    m_showLocation->setChecked(config.showLocation);
    m_showSymLinks->setChecked(config.showSymLinks);
    m_showHidden->setChecked(config.showHidden);
    m_playbackOnSend->setChecked(config.sendPlayback);

    m_playingRowFont->setChecked(!config.playingRowFont.isEmpty());
    if(config.playingRowFont.isEmpty()) {
        m_playingRowFont->setButtonFont(widget()->font());
    }
    else {
        m_playingRowFont->setButtonFont(config.playingRowFont);
    }
    const QColor playingRowColour{config.playingRowColour};
    m_playingRowColour->setChecked(playingRowColour.isValid());
    m_playingRowColour->setColour(playingRowColour.isValid() ? playingRowColour : Gui::defaultPlayingRowColour());
    m_background->setConfig(config.background);

    if(config.mode == DirBrowser::Mode::List) {
        m_listMode->setChecked(true);
    }
    else {
        m_treeMode->setChecked(true);
    }

    TrackSelectionController::setCurrentAction(m_doubleClick, config.doubleClickAction);
    TrackSelectionController::setCurrentAction(m_middleClick, config.middleClickAction);
}

void DirBrowserConfigDialog::mergeExternalConfig(const DirBrowser::ConfigData& previous,
                                                 const DirBrowser::ConfigData& current)
{
    mergeExternalFields(
        previous, current, &DirBrowser::ConfigData::doubleClickAction, &DirBrowser::ConfigData::middleClickAction,
        &DirBrowser::ConfigData::sendPlayback, &DirBrowser::ConfigData::showIcons, &DirBrowser::ConfigData::indentList,
        &DirBrowser::ConfigData::showHeader, &DirBrowser::ConfigData::restoreSort, &DirBrowser::ConfigData::mode,
        &DirBrowser::ConfigData::controlsPosition, &DirBrowser::ConfigData::showControls,
        &DirBrowser::ConfigData::showLocation, &DirBrowser::ConfigData::showSymLinks,
        &DirBrowser::ConfigData::showHidden, &DirBrowser::ConfigData::rootPath, &DirBrowser::ConfigData::playingRowFont,
        &DirBrowser::ConfigData::playingRowColour, &DirBrowser::ConfigData::background);
}
} // namespace Fooyin
