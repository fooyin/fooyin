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

#include "radiobrowserconfigdialog.h"

#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/backgroundconfigwidget.h>
#include <gui/widgets/hintlabel.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QTabWidget>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
RadioBrowserConfigDialog::RadioBrowserConfigDialog(RadioBrowserWidget* radioBrowser, QWidget* parent)
    : WidgetConfigDialog{radioBrowser, tr("Radio Browser Settings"), parent}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
    , m_hideBroken{new QCheckBox(tr("Hide broken stations"), this)}
    , m_sendClicks{new QCheckBox(tr("Send clicks to radio-browser.info"), this)}
    , m_showSearchBar{new QCheckBox(tr("Show search bar"), this)}
    , m_displayMode{new QComboBox(this)}
    , m_captions{new QComboBox(this)}
    , m_showHeader{new QCheckBox(tr("Show header"), this)}
    , m_showScrollbar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_alternatingRows{new QCheckBox(tr("Alternating row colours"), this)}
    , m_showStationIcons{new QCheckBox(tr("Show station icons"), this)}
    , m_showSavedStationIcons{new QCheckBox(tr("Show saved station icons"), this)}
    , m_showToolTips{new QCheckBox(tr("Show station tooltips"), this)}
    , m_separateSavedStationsViewState{new QCheckBox(tr("Remember My Stations display separately"), this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconSize{new QSpinBox(this)}
    , m_autoFitArtwork{new QCheckBox(tr("Fit icons to available width"), this)}
    , m_iconHorizontalGap{new QSpinBox(this)}
    , m_iconVerticalGap{new QSpinBox(this)}
    , m_useIconGapsForSideCaptions{new QCheckBox(tr("Use grid spacing with right captions"), this)}
    , m_iconLeftMargin{new QSpinBox(this)}
    , m_iconTopMargin{new QSpinBox(this)}
    , m_iconRightMargin{new QSpinBox(this)}
    , m_iconBottomMargin{new QSpinBox(this)}
    , m_iconCornerRadius{new QSpinBox(this)}
    , m_uniformStationIcons{new QCheckBox(tr("Use uniform station icon sizes"), this)}
    , m_background{new BackgroundConfigWidget(this)}
{
    m_displayMode->addItem(tr("Columns"), static_cast<int>(ExpandedTreeView::ViewMode::Tree));
    m_displayMode->addItem(tr("Icons"), static_cast<int>(ExpandedTreeView::ViewMode::Icon));
    m_captions->addItem(tr("Right"), static_cast<int>(ExpandedTreeView::CaptionDisplay::Right));
    m_captions->addItem(tr("Bottom"), static_cast<int>(ExpandedTreeView::CaptionDisplay::Bottom));

    TrackSelectionController::addAction(m_doubleClick, tr("None"), TrackAction::None);
    TrackSelectionController::addAction(m_doubleClick, tr("Play now"), TrackAction::Play);
    m_doubleClick->addItem(tr("Add or remove from My Stations"), RadioBrowserWidget::ToggleSavedStation);
    TrackSelectionController::addStandardActions(m_doubleClick);

    TrackSelectionController::addAction(m_middleClick, tr("None"), TrackAction::None);
    m_middleClick->addItem(tr("Add or remove from My Stations"), RadioBrowserWidget::ToggleSavedStation);
    TrackSelectionController::addStandardActions(m_middleClick);

    m_playbackOnSend->setToolTip(
        tr(R"(For ‘Replace current playlist’ and ‘Create new playlist’, start playback immediately)"));
    m_hideBroken->setToolTip(tr("Exclude stations marked as broken by radio-browser.info from search results"));
    m_sendClicks->setToolTip(
        tr("Report played stations to radio-browser.info so click counts and station statistics stay up to date"));
    m_uniformStationIcons->setToolTip(
        tr("Fit station icons into equally sized areas while preserving their proportions"));
    m_showSavedStationIcons->setToolTip(tr("Show an indicator for stations already in My Stations"));
    m_showToolTips->setToolTip(tr("Show station details when hovering over station rows and icons"));
    m_separateSavedStationsViewState->setToolTip(
        tr("Remember display mode and columns separately when browsing My Stations"));

    m_rowHeight->setRange(0, 256);
    m_rowHeight->setSuffix(u" px"_s);

    m_iconCornerRadius->setRange(0, 100);
    m_iconCornerRadius->setSingleStep(5);
    m_iconCornerRadius->setSuffix(u" %"_s);
    m_iconCornerRadius->setSpecialValueText(tr("Square"));

    m_iconSize->setRange(16, 1024);
    m_iconSize->setSuffix(u" px"_s);
    m_iconSize->setSingleStep(4);

    m_autoFitArtwork->setToolTip(
        tr("Resize artwork to fill the available width using the preferred size.\n"
           "Ctrl+Scroll changes the number of columns. Automatic horizontal gaps are treated as zero."));

    for(auto* margin : {m_iconLeftMargin, m_iconTopMargin, m_iconRightMargin, m_iconBottomMargin}) {
        margin->setRange(0, 999);
        margin->setSuffix(u" px"_s);
    }

    m_iconHorizontalGap->setRange(-1, 256);
    m_iconHorizontalGap->setSpecialValueText(tr("Auto"));
    m_iconHorizontalGap->setSuffix(u" px"_s);
    m_iconHorizontalGap->setToolTip(
        tr("Horizontal spacing between stations in icon display mode. Auto uses the view’s default spacing."));

    m_iconVerticalGap->setRange(0, 256);
    m_iconVerticalGap->setSuffix(u" px"_s);
    m_iconVerticalGap->setToolTip(tr("Vertical spacing between stations in icon display mode."));

    m_useIconGapsForSideCaptions->setToolTip(
        tr("Apply horizontal and vertical gaps when captions are displayed to the right of icons. "
           "When disabled, columns stretch to fill the available width without gaps between stations. "
           "Artwork margins still apply."));

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), this);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    int row{0};
    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, this), row, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, row++, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, this), row, 0);
    clickBehaviourLayout->addWidget(m_middleClick, row++, 1);
    clickBehaviourLayout->addWidget(m_playbackOnSend, row++, 0, 1, 2);
    clickBehaviourLayout->setColumnStretch(1, 1);

    auto* search       = new QGroupBox(tr("Search"), this);
    auto* searchLayout = new QGridLayout(search);

    row = 0;
    searchLayout->addWidget(m_showSearchBar, row++, 0, 1, 2);
    searchLayout->addWidget(m_hideBroken, row++, 0, 1, 2);
    searchLayout->addWidget(m_sendClicks, row++, 0, 1, 2);
    searchLayout->setColumnStretch(2, 1);
    searchLayout->setRowStretch(row, 1);

    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    row = 0;
    appearanceLayout->addWidget(new QLabel(tr("Display mode") + u":"_s, this), row, 0);
    appearanceLayout->addWidget(m_displayMode, row++, 1);
    appearanceLayout->addWidget(new QLabel(tr("Caption placement") + u":"_s, this), row, 0);
    appearanceLayout->addWidget(m_captions, row++, 1);
    appearanceLayout->addWidget(m_showHeader, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_showScrollbar, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_alternatingRows, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_showStationIcons, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_overrideRowHeight, row, 0);
    appearanceLayout->addWidget(m_rowHeight, row++, 1);
    appearanceLayout->addWidget(m_showSavedStationIcons, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_showToolTips, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_separateSavedStationsViewState, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_uniformStationIcons, row++, 0, 1, 2);
    appearanceLayout->setColumnStretch(2, 1);
    appearanceLayout->setRowStretch(row, 1);

    auto* iconMode   = new QGroupBox(tr("Icon Mode"), this);
    auto* iconLayout = new QGridLayout(iconMode);

    auto* gapLayout = new QGridLayout();

    gapLayout->addWidget(new QLabel(tr("Horizontal") + u":"_s, this), 0, 0);
    gapLayout->addWidget(m_iconHorizontalGap, 0, 1);
    gapLayout->addWidget(new QLabel(tr("Vertical") + u":"_s, this), 0, 2);
    gapLayout->addWidget(m_iconVerticalGap, 0, 3);
    gapLayout->setColumnStretch(4, 1);

    auto* marginsLayout = new QGridLayout();
    marginsLayout->addWidget(new QLabel(tr("Left") + u":"_s, this), 0, 0);
    marginsLayout->addWidget(m_iconLeftMargin, 0, 1);
    marginsLayout->addWidget(new QLabel(tr("Right") + u":"_s, this), 0, 2);
    marginsLayout->addWidget(m_iconRightMargin, 0, 3);
    marginsLayout->addWidget(new QLabel(tr("Top") + u":"_s, this), 1, 0);
    marginsLayout->addWidget(m_iconTopMargin, 1, 1);
    marginsLayout->addWidget(new QLabel(tr("Bottom") + u":"_s, this), 1, 2);
    marginsLayout->addWidget(m_iconBottomMargin, 1, 3);
    marginsLayout->setColumnStretch(4, 1);

    row = 0;
    iconLayout->addWidget(new QLabel(tr("Icon corner radius") + u":"_s, this), row, 0);
    iconLayout->addWidget(m_iconCornerRadius, row++, 1);
    iconLayout->addWidget(m_autoFitArtwork, row++, 0, 1, 3);
    iconLayout->addWidget(new QLabel(tr("Icon size") + u":"_s, this), row, 0);
    iconLayout->addWidget(m_iconSize, row++, 1);
    iconLayout->addWidget(new HintLabel(tr("Use <b>Ctrl+Scroll</b> in the widget to resize icons."), this), row++, 0, 1,
                          3);
    iconLayout->addWidget(Gui::createSectionHeader(tr("Gap"), this), row++, 0);
    iconLayout->addLayout(gapLayout, row++, 0, 1, 3);
    iconLayout->addWidget(m_useIconGapsForSideCaptions, row++, 0, 1, 3);
    iconLayout->addWidget(Gui::createSectionHeader(tr("Margins"), this), row++, 0);
    iconLayout->addLayout(marginsLayout, row++, 0, 1, 3);
    iconLayout->setColumnStretch(2, 1);
    iconLayout->setRowStretch(row, 1);

    auto* generalPage = new QWidget(this);
    auto* layout      = new QGridLayout(generalPage);

    row = 0;
    layout->addWidget(clickBehaviour, row++, 0);
    layout->addWidget(search, row++, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(row, 1);

    auto* appearancePage       = new QWidget(this);
    auto* appearancePageLayout = new QGridLayout(appearancePage);
    appearancePageLayout->addWidget(appearance, 0, 0);
    appearancePageLayout->addWidget(iconMode, 0, 1);
    appearancePageLayout->setColumnStretch(0, 1);
    appearancePageLayout->setColumnStretch(1, 1);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(generalPage, tr("General"));
    tabs->addTab(appearancePage, tr("Appearance"));
    tabs->addTab(m_background, tr("Background"));
    contentLayout()->addWidget(tabs);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
    QObject::connect(m_displayMode, &QComboBox::currentIndexChanged, this,
                     &RadioBrowserConfigDialog::updateAppearanceControls);
    QObject::connect(m_captions, &QComboBox::currentIndexChanged, this,
                     &RadioBrowserConfigDialog::updateAppearanceControls);

    m_sendClicks->setChecked(radioBrowser->sendClicks());
    QObject::connect(radioBrowser, &RadioBrowserWidget::configChanged, this,
                     &RadioBrowserConfigDialog::syncCurrentConfig);
    QObject::connect(radioBrowser, &RadioBrowserWidget::sendClicksChanged, m_sendClicks, &QCheckBox::setChecked);

    loadCurrentConfig();
}

void RadioBrowserConfigDialog::apply()
{
    const bool sendClicks = m_sendClicks->isChecked();
    WidgetConfigDialog::apply();
    widget()->setSendClicks(sendClicks);
    m_sendClicks->setChecked(widget()->sendClicks());
}

RadioBrowserWidget::ConfigData RadioBrowserConfigDialog::config() const
{
    RadioBrowserWidget::ConfigData config;
    config.doubleClickAction    = m_doubleClick->currentData().toInt();
    config.middleClickAction    = m_middleClick->currentData().toInt();
    config.playbackOnSend       = m_playbackOnSend->isChecked();
    config.hideBroken           = m_hideBroken->isChecked();
    config.showFilterBar        = m_showSearchBar->isChecked();
    config.background           = m_background->config();
    config.view.viewMode        = static_cast<ExpandedTreeView::ViewMode>(m_displayMode->currentData().toInt());
    config.view.captions        = static_cast<ExpandedTreeView::CaptionDisplay>(m_captions->currentData().toInt());
    config.view.showHeader      = m_showHeader->isChecked();
    config.view.showScrollbar   = m_showScrollbar->isChecked();
    config.view.alternatingRows = m_alternatingRows->isChecked();
    config.view.showIcons       = m_showStationIcons->isChecked();
    config.view.rowHeight       = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0;
    config.view.showSavedStationIcons          = m_showSavedStationIcons->isChecked();
    config.view.showToolTips                   = m_showToolTips->isChecked();
    config.view.separateSavedStationsViewState = m_separateSavedStationsViewState->isChecked();
    config.view.iconSize                       = {m_iconSize->value(), m_iconSize->value()};
    config.view.autoFitArtwork                 = m_autoFitArtwork->isChecked();
    config.view.iconHorizontalGap              = m_iconHorizontalGap->value();
    config.view.iconVerticalGap                = m_iconVerticalGap->value();
    config.view.useIconGapsForSideCaptions     = m_useIconGapsForSideCaptions->isChecked();
    config.view.iconMargins         = {m_iconLeftMargin->value(), m_iconTopMargin->value(), m_iconRightMargin->value(),
                                       m_iconBottomMargin->value()};
    config.view.iconCornerRadius    = m_iconCornerRadius->value();
    config.view.uniformStationIcons = m_uniformStationIcons->isChecked();
    return config;
}

void RadioBrowserConfigDialog::setConfig(const RadioBrowserWidget::ConfigData& config)
{
    TrackSelectionController::setCurrentAction(m_doubleClick, config.doubleClickAction);
    TrackSelectionController::setCurrentAction(m_middleClick, config.middleClickAction);

    m_playbackOnSend->setChecked(config.playbackOnSend);
    m_hideBroken->setChecked(config.hideBroken);
    m_showSearchBar->setChecked(config.showFilterBar);
    m_showSearchBar->setVisible(widget()->filterBarToggleAllowed());
    m_background->setConfig(config.background);
    m_displayMode->setCurrentIndex(m_displayMode->findData(static_cast<int>(config.view.viewMode)));
    m_captions->setCurrentIndex(m_captions->findData(static_cast<int>(config.view.captions)));
    m_showHeader->setChecked(config.view.showHeader);
    m_showScrollbar->setChecked(config.view.showScrollbar);
    m_alternatingRows->setChecked(config.view.alternatingRows);
    m_showStationIcons->setChecked(config.view.showIcons);
    m_rowHeight->setEnabled(config.view.rowHeight > 0);
    m_overrideRowHeight->setChecked(config.view.rowHeight > 0);
    m_rowHeight->setValue(config.view.rowHeight);
    m_showSavedStationIcons->setChecked(config.view.showSavedStationIcons);
    m_showToolTips->setChecked(config.view.showToolTips);
    m_separateSavedStationsViewState->setChecked(config.view.separateSavedStationsViewState);
    m_separateSavedStationsViewState->setVisible(widget()->separateSavedStationsViewStateAllowed());
    m_iconSize->setValue(std::max(config.view.iconSize.width(), config.view.iconSize.height()));
    m_autoFitArtwork->setChecked(config.view.autoFitArtwork);
    m_iconHorizontalGap->setValue(config.view.iconHorizontalGap);
    m_iconVerticalGap->setValue(config.view.iconVerticalGap);
    m_useIconGapsForSideCaptions->setChecked(config.view.useIconGapsForSideCaptions);
    m_iconLeftMargin->setValue(config.view.iconMargins.left());
    m_iconTopMargin->setValue(config.view.iconMargins.top());
    m_iconRightMargin->setValue(config.view.iconMargins.right());
    m_iconBottomMargin->setValue(config.view.iconMargins.bottom());
    m_iconCornerRadius->setValue(config.view.iconCornerRadius);
    m_uniformStationIcons->setChecked(config.view.uniformStationIcons);
    updateAppearanceControls();
}

void RadioBrowserConfigDialog::mergeExternalConfig(const RadioBrowserWidget::ConfigData& previous,
                                                   const RadioBrowserWidget::ConfigData& current)
{
    auto draft{config()};
    mergeExternalFieldValues(
        previous, current, draft, &RadioBrowserWidget::ConfigData::doubleClickAction,
        &RadioBrowserWidget::ConfigData::middleClickAction, &RadioBrowserWidget::ConfigData::playbackOnSend,
        &RadioBrowserWidget::ConfigData::hideBroken, &RadioBrowserWidget::ConfigData::showFilterBar,
        &RadioBrowserWidget::ConfigData::background);
    mergeExternalFieldValues(
        previous.view, current.view, draft.view, &RadioBrowserWidget::ConfigData::ViewConfig::viewMode,
        &RadioBrowserWidget::ConfigData::ViewConfig::captions, &RadioBrowserWidget::ConfigData::ViewConfig::showHeader,
        &RadioBrowserWidget::ConfigData::ViewConfig::showScrollbar,
        &RadioBrowserWidget::ConfigData::ViewConfig::alternatingRows,
        &RadioBrowserWidget::ConfigData::ViewConfig::showIcons,
        &RadioBrowserWidget::ConfigData::ViewConfig::showSavedStationIcons,
        &RadioBrowserWidget::ConfigData::ViewConfig::showToolTips,
        &RadioBrowserWidget::ConfigData::ViewConfig::separateSavedStationsViewState,
        &RadioBrowserWidget::ConfigData::ViewConfig::rowHeight, &RadioBrowserWidget::ConfigData::ViewConfig::iconSize,
        &RadioBrowserWidget::ConfigData::ViewConfig::autoFitArtwork,
        &RadioBrowserWidget::ConfigData::ViewConfig::iconHorizontalGap,
        &RadioBrowserWidget::ConfigData::ViewConfig::iconVerticalGap,
        &RadioBrowserWidget::ConfigData::ViewConfig::useIconGapsForSideCaptions,
        &RadioBrowserWidget::ConfigData::ViewConfig::iconMargins,
        &RadioBrowserWidget::ConfigData::ViewConfig::iconCornerRadius,
        &RadioBrowserWidget::ConfigData::ViewConfig::uniformStationIcons);
    setConfig(draft);
}

void RadioBrowserConfigDialog::updateAppearanceControls()
{
    const bool icons = m_displayMode->currentData().toInt() == static_cast<int>(ExpandedTreeView::ViewMode::Icon);
    const bool rightCaptions
        = m_captions->currentData().toInt() == static_cast<int>(ExpandedTreeView::CaptionDisplay::Right);
    m_captions->setEnabled(icons);
    m_autoFitArtwork->setEnabled(icons && !rightCaptions);
    m_useIconGapsForSideCaptions->setEnabled(icons && rightCaptions);
}
} // namespace Fooyin::RadioBrowser
