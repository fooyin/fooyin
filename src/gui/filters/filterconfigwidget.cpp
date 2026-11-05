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

#include "coverflowconfigwidget.h"
#include "expansionconfigwidget.h"
#include "filtercolumneditordialog.h"

#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/backgroundconfigwidget.h>
#include <gui/widgets/hintlabel.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
FilterConfigDialog::FilterConfigDialog(FilterWidget* filterWidget, FilterColumnRegistry* columnRegistry,
                                       QWidget* parent)
    : WidgetConfigDialog{filterWidget, tr("Filter Settings"), parent}
    , m_filterWidget{filterWidget}
    , m_expansionConfig{new ExpansionConfigWidget(filterWidget, this)}
    , m_coverFlowConfig{new CoverFlowConfigWidget(this)}
    , m_columnRegistry{columnRegistry}
    , m_source{new QComboBox(this)}
    , m_middleClick{new QComboBox(this)}
    , m_doubleClick{new QComboBox(this)}
    , m_expansionMiddleClick{new QComboBox(this)}
    , m_expansionDoubleClick{new QComboBox(this)}
    , m_playbackOnSend{new QCheckBox(tr("Start playback immediately"), this)}
    , m_playlistEnabled{new QCheckBox(tr("Enabled"), this)}
    , m_autoSwitch{new QCheckBox(tr("Switch when changed"), this)}
    , m_preservePlaybackPlaylist{new QCheckBox(tr("Preserve playback playlist"), this)}
    , m_playlistName{new QLineEdit(this)}
    , m_restoreState{new QCheckBox(tr("Restore state on startup"), this)}
    , m_columnsMode{new QRadioButton(tr("Columns"), this)}
    , m_coverFlowMode{new QRadioButton(tr("Cover flow"), this)}
    , m_artworkBottomMode{new QRadioButton(tr("Artwork (bottom labels)"), this)}
    , m_artworkRightMode{new QRadioButton(tr("Artwork (right labels)"), this)}
    , m_artworkOverlayMode{new QRadioButton(tr("Artwork (overlay labels)"), this)}
    , m_artworkNoLabelsMode{new QRadioButton(tr("Artwork (no labels)"), this)}
    , m_coverType{new QComboBox(this)}
    , m_coverSource{new QComboBox(this)}
    , m_showSummary{new QCheckBox(tr("Show summary item"), this)}
    , m_showHeader{new QCheckBox(tr("Show header"), this)}
    , m_showScrollbar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_alternatingRows{new QCheckBox(tr("Alternating row colours"), this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
    , m_autoFitArtwork{new QCheckBox(tr("Fit artwork to available width"), this)}
    , m_iconHorizontalGap{new QSpinBox(this)}
    , m_iconVerticalGap{new QSpinBox(this)}
    , m_artworkCornerRadius{new QSpinBox(this)}
    , m_overrideArtworkMargins{new QCheckBox(tr("Custom margins"), this)}
    , m_artworkLeftMargin{new QSpinBox(this)}
    , m_artworkTopMargin{new QSpinBox(this)}
    , m_artworkRightMargin{new QSpinBox(this)}
    , m_artworkBottomMargin{new QSpinBox(this)}
    , m_background{new BackgroundConfigWidget(this)}
    , m_manageColumns{new QPushButton(tr("Manage columns…"), this)}
{
    auto* tabs = new QTabWidget(this);

    auto* generalTab    = new QWidget(tabs);
    auto* appearanceTab = new QWidget(tabs);

    auto* backgroundTab    = new QWidget(tabs);
    auto* backgroundLayout = new QVBoxLayout(backgroundTab);
    backgroundLayout->addWidget(m_background);
    backgroundLayout->addStretch();

    tabs->addTab(generalTab, tr("General"));
    tabs->addTab(appearanceTab, tr("Appearance"));
    tabs->addTab(m_coverFlowConfig, tr("Cover flow"));
    tabs->addTab(m_expansionConfig, tr("Expansion"));
    tabs->addTab(backgroundTab, tr("Background"));

    m_playbackOnSend->setToolTip(
        tr("For ‘Replace current playlist’ and ‘Create new playlist’, start playback immediately."));

    auto* playlistClickHint
        = new HintLabel(tr("Set to <b>Play</b> to start playback at the first matching track."), this);
    playlistClickHint->setWordWrap(true);

    auto* clickBehaviour       = new QGroupBox(tr("Click Behaviour"), generalTab);
    auto* clickBehaviourLayout = new QGridLayout(clickBehaviour);

    clickBehaviourLayout->addWidget(new QLabel(tr("Filter items"), clickBehaviour), 0, 1);
    clickBehaviourLayout->addWidget(new QLabel(tr("Expanded tracks"), clickBehaviour), 0, 2);
    clickBehaviourLayout->addWidget(new QLabel(tr("Double-click") + u":"_s, clickBehaviour), 1, 0);
    clickBehaviourLayout->addWidget(m_doubleClick, 1, 1);
    clickBehaviourLayout->addWidget(m_expansionDoubleClick, 1, 2);
    clickBehaviourLayout->addWidget(new QLabel(tr("Middle-click") + u":"_s, clickBehaviour), 2, 0);
    clickBehaviourLayout->addWidget(m_middleClick, 2, 1);
    clickBehaviourLayout->addWidget(m_expansionMiddleClick, 2, 2);
    clickBehaviourLayout->addWidget(m_playbackOnSend, 3, 0, 1, 3);
    clickBehaviourLayout->addWidget(playlistClickHint, 4, 0, 1, 3);
    clickBehaviourLayout->setColumnStretch(1, 1);
    clickBehaviourLayout->setColumnStretch(2, 1);

    m_preservePlaybackPlaylist->setToolTip(
        tr("When this selection playlist is used for playback, preserve it with ‘(Playback)’ appended to its "
           "name instead of replacing its tracks."));

    auto* selectionPlaylist       = new QGroupBox(tr("Filter Selection Playlist"), generalTab);
    auto* selectionPlaylistLayout = new QGridLayout(selectionPlaylist);

    selectionPlaylist->setToolTip(
        tr("In current playlist mode, matching tracks are selected directly in the playlist."));

    selectionPlaylistLayout->addWidget(m_playlistEnabled, 0, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_autoSwitch, 1, 0, 1, 3);
    selectionPlaylistLayout->addWidget(m_preservePlaybackPlaylist, 2, 0, 1, 3);
    selectionPlaylistLayout->addWidget(new QLabel(tr("Name") + u":"_s, this), 3, 0);
    selectionPlaylistLayout->addWidget(m_playlistName, 3, 1, 1, 2);
    selectionPlaylistLayout->setColumnStretch(2, 1);

    const auto updateSourceMode = [this, selectionPlaylist]() {
        const bool playlistSource = m_source->currentData().toInt() == static_cast<int>(FilterSource::CurrentPlaylist);
        selectionPlaylist->setEnabled(!playlistSource);
    };
    QObject::connect(m_source, &QComboBox::currentIndexChanged, selectionPlaylist, updateSourceMode);
    updateSourceMode();

    auto* displayModeGroup = new QButtonGroup(this);
    for(auto* button : {m_columnsMode, m_coverFlowMode, m_artworkBottomMode, m_artworkRightMode, m_artworkOverlayMode,
                        m_artworkNoLabelsMode}) {
        displayModeGroup->addButton(button);
    }

    auto* displayModeLayout = new QVBoxLayout();
    displayModeLayout->addWidget(m_columnsMode);
    displayModeLayout->addWidget(m_artworkBottomMode);
    displayModeLayout->addWidget(m_artworkRightMode);
    displayModeLayout->addWidget(m_artworkOverlayMode);
    displayModeLayout->addWidget(m_artworkNoLabelsMode);
    displayModeLayout->addWidget(m_coverFlowMode);

    m_coverType->addItem(tr("Front cover"), static_cast<int>(Track::Cover::Front));
    m_coverType->addItem(tr("Back cover"), static_cast<int>(Track::Cover::Back));
    m_coverType->addItem(tr("Artist"), static_cast<int>(Track::Cover::Artist));

    m_coverSource->addItem(tr("Default"), -1);
    m_coverSource->addItem(tr("Embedded covers"), static_cast<int>(ArtworkSourcePreference::PreferEmbedded));
    m_coverSource->addItem(tr("Directory covers"), static_cast<int>(ArtworkSourcePreference::PreferDirectory));

    auto* appearance       = new QGroupBox(tr("Display"), appearanceTab);
    auto* appearanceLayout = new QGridLayout(appearance);

    int row{0};
    appearanceLayout->addWidget(Gui::createSectionHeader(tr("View mode"), appearance), row++, 0, 1, 4);
    appearanceLayout->addLayout(displayModeLayout, row++, 0, 1, 4);
    appearanceLayout->addWidget(Gui::createSectionHeader(tr("Options"), appearance), row++, 0, 1, 4);
    appearanceLayout->addWidget(m_showSummary, row++, 0, 1, 3);
    appearanceLayout->addWidget(m_showHeader, row++, 0, 1, 3);
    appearanceLayout->addWidget(m_showScrollbar, row++, 0, 1, 3);
    appearanceLayout->addWidget(m_alternatingRows, row++, 0, 1, 3);
    appearanceLayout->addWidget(m_overrideRowHeight, row, 0, 1, 2);
    appearanceLayout->addWidget(m_rowHeight, row++, 2);
    appearanceLayout->setColumnStretch(3, 1);

    auto* artworkMode   = new QGroupBox(tr("Artwork Mode"), appearanceTab);
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
    m_artworkCornerRadius->setSingleStep(5);
    m_artworkCornerRadius->setSuffix(u" %"_s);
    m_artworkCornerRadius->setSpecialValueText(tr("Square"));

    for(auto* margin : {m_artworkLeftMargin, m_artworkTopMargin, m_artworkRightMargin, m_artworkBottomMargin}) {
        margin->setRange(0, 999);
        margin->setSuffix(u" px"_s);
    }

    auto* sizeLayout = new QGridLayout();
    sizeLayout->addWidget(new QLabel(tr("Width") + u":"_s, artworkMode), 0, 0);
    sizeLayout->addWidget(m_iconWidth, 0, 1);
    sizeLayout->addWidget(new QLabel(tr("Height") + u":"_s, artworkMode), 0, 2);
    sizeLayout->addWidget(m_iconHeight, 0, 3);
    sizeLayout->setColumnStretch(4, 1);

    auto* gapsLayout = new QGridLayout();
    gapsLayout->addWidget(new QLabel(tr("Horizontal") + u":"_s, artworkMode), 0, 0);
    gapsLayout->addWidget(m_iconHorizontalGap, 0, 1);
    gapsLayout->addWidget(new QLabel(tr("Vertical") + u":"_s, artworkMode), 0, 2);
    gapsLayout->addWidget(m_iconVerticalGap, 0, 3);
    gapsLayout->setColumnStretch(4, 1);

    auto* marginsLayout = new QGridLayout();
    marginsLayout->addWidget(new QLabel(tr("Left") + u":"_s, artworkMode), 0, 0);
    marginsLayout->addWidget(m_artworkLeftMargin, 0, 1);
    marginsLayout->addWidget(new QLabel(tr("Right") + u":"_s, artworkMode), 0, 2);
    marginsLayout->addWidget(m_artworkRightMargin, 0, 3);
    marginsLayout->addWidget(new QLabel(tr("Top") + u":"_s, artworkMode), 1, 0);
    marginsLayout->addWidget(m_artworkTopMargin, 1, 1);
    marginsLayout->addWidget(new QLabel(tr("Bottom") + u":"_s, artworkMode), 1, 2);
    marginsLayout->addWidget(m_artworkBottomMargin, 1, 3);
    marginsLayout->setColumnStretch(4, 1);

    auto* iconSizeHint = new HintLabel(
        tr("Use <b>Ctrl+Scroll</b> in the widget to resize icons when manual sizing is enabled."), this);

    m_autoFitArtwork->setToolTip(
        tr("Resize artwork to fill the available width using the preferred size.\n"
           "Ctrl+Scroll changes the number of columns. Automatic horizontal gaps are treated as zero."));

    row = 0;
    artworkLayout->addWidget(new QLabel(tr("Artwork type") + u":"_s, artworkMode), row, 0);
    artworkLayout->addWidget(m_coverType, row++, 1, 1, 3);
    artworkLayout->addWidget(new QLabel(tr("Artwork source") + u":"_s, artworkMode), row, 0);
    artworkLayout->addWidget(m_coverSource, row++, 1, 1, 3);
    artworkLayout->addWidget(new QLabel(tr("Corner radius") + u":"_s, artworkMode), row, 0);
    artworkLayout->addWidget(m_artworkCornerRadius, row++, 1);
    artworkLayout->addWidget(Gui::createSectionHeader(tr("Size"), artworkMode), row++, 0, 1, 5);
    artworkLayout->addWidget(m_autoFitArtwork, row++, 0, 1, 5);
    artworkLayout->addLayout(sizeLayout, row++, 0, 1, 5);
    artworkLayout->addWidget(iconSizeHint, row++, 0, 1, 5);
    artworkLayout->addWidget(Gui::createSectionHeader(tr("Gaps"), artworkMode), row++, 0, 1, 5);
    artworkLayout->addLayout(gapsLayout, row++, 0, 1, 5);
    artworkLayout->addWidget(Gui::createSectionHeader(tr("Margins"), artworkMode), row++, 0, 1, 5);
    artworkLayout->addWidget(m_overrideArtworkMargins, row++, 0, 1, 5);
    artworkLayout->addLayout(marginsLayout, row++, 0, 1, 5);
    artworkLayout->setColumnStretch(4, 1);

    auto* playlistSourceHint
        = new HintLabel(tr("Current playlist mode uses the displayed playlist as its source and selects the matching "
                           "tracks in that playlist."),
                        this);
    playlistSourceHint->setWordWrap(true);

    auto* generalGroup       = new QGroupBox(tr("General"), generalTab);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    m_source->addItem(tr("Library"), static_cast<int>(FilterSource::Library));
    m_source->addItem(tr("Current playlist"), static_cast<int>(FilterSource::CurrentPlaylist));

    generalGroupLayout->addWidget(new QLabel(tr("Source") + u":"_s, this), 0, 0);
    generalGroupLayout->addWidget(m_source, 0, 1);
    generalGroupLayout->addWidget(playlistSourceHint, 1, 0, 1, 3);
    generalGroupLayout->addWidget(m_restoreState, 2, 0, 1, 3);
    generalGroupLayout->addWidget(m_manageColumns, 3, 0, 1, 3);
    generalGroupLayout->setColumnStretch(2, 1);

    auto* generalLayout = new QGridLayout(generalTab);

    row = 0;
    generalLayout->addWidget(generalGroup, row++, 0);
    generalLayout->addWidget(clickBehaviour, row++, 0);
    generalLayout->addWidget(selectionPlaylist, row++, 0);
    generalLayout->setColumnStretch(0, 1);
    generalLayout->setRowStretch(row, 1);

    auto* appearanceTabLayout = new QGridLayout(appearanceTab);

    appearanceTabLayout->addWidget(appearance, 0, 0, Qt::AlignTop);
    appearanceTabLayout->addWidget(artworkMode, 0, 1, Qt::AlignTop);
    appearanceTabLayout->setColumnStretch(0, 1);
    appearanceTabLayout->setColumnStretch(1, 1);
    appearanceTabLayout->setRowStretch(1, 1);

    auto* mainLayout{contentLayout()};
    mainLayout->addWidget(tabs, 0, 0);
    mainLayout->setColumnStretch(0, 1);
    mainLayout->setRowStretch(0, 1);

    for(auto* box : {m_doubleClick, m_middleClick, m_expansionDoubleClick, m_expansionMiddleClick}) {
        if(box == m_expansionDoubleClick || box == m_expansionMiddleClick) {
            box->addItem(tr("Use filter action"), -1);
        }
        TrackSelectionController::addAction(box, tr("None"), TrackAction::None);
        TrackSelectionController::addAction(box, tr("Play now"), TrackAction::Play);
        TrackSelectionController::addStandardActions(box);
    }

    setupConnections();

    loadCurrentConfig();
}

FilterWidget::ConfigData FilterConfigDialog::config() const
{
    const auto captions   = m_artworkRightMode->isChecked()    ? ExpandedTreeView::CaptionDisplay::Right
                          : m_artworkOverlayMode->isChecked()  ? ExpandedTreeView::CaptionDisplay::Overlay
                          : m_artworkNoLabelsMode->isChecked() ? ExpandedTreeView::CaptionDisplay::None
                                                               : ExpandedTreeView::CaptionDisplay::Bottom;
    const int coverSource = m_coverSource->currentData().toInt();

    FilterWidget::ConfigData result{};
    result.doubleClickAction          = m_doubleClick->currentData().toInt();
    result.middleClickAction          = m_middleClick->currentData().toInt();
    result.expansionDoubleClickAction = m_expansionDoubleClick->currentData().toInt();
    result.expansionMiddleClickAction = m_expansionMiddleClick->currentData().toInt();
    result.sendPlayback               = m_playbackOnSend->isChecked();
    result.source                     = static_cast<FilterSource>(m_source->currentData().toInt());
    result.playlistEnabled            = m_playlistEnabled->isChecked();
    result.autoSwitch                 = m_autoSwitch->isChecked();
    result.preservePlaybackPlaylist   = m_preservePlaybackPlaylist->isChecked();
    result.playlistName               = m_playlistName->text();
    result.restoreState               = m_restoreState->isChecked();
    result.viewMode                   = m_columnsMode->isChecked()   ? ExpandedTreeView::ViewMode::Tree
                                      : m_coverFlowMode->isChecked() ? ExpandedTreeView::ViewMode::CoverFlow
                                                                     : ExpandedTreeView::ViewMode::Icon;
    result.captionDisplay             = captions;
    result.coverType                  = static_cast<Track::Cover>(m_coverType->currentData().toInt());
    result.coverSource
        = coverSource < 0 ? std::nullopt : std::optional{static_cast<ArtworkSourcePreference>(coverSource)};
    result.showSummary            = m_showSummary->isChecked();
    result.showHeader             = m_showHeader->isChecked();
    result.showScrollbar          = m_showScrollbar->isChecked();
    result.alternatingRows        = m_alternatingRows->isChecked();
    result.rowHeight              = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0;
    result.iconSize               = {m_iconWidth->value(), m_iconHeight->value()};
    result.autoFitArtwork         = m_autoFitArtwork->isChecked();
    result.iconHorizontalGap      = m_iconHorizontalGap->value();
    result.iconVerticalGap        = m_iconVerticalGap->value();
    result.artworkCornerRadius    = m_artworkCornerRadius->value();
    result.overrideArtworkMargins = m_overrideArtworkMargins->isChecked();
    result.artworkMargins = {m_artworkLeftMargin->value(), m_artworkTopMargin->value(), m_artworkRightMargin->value(),
                             m_artworkBottomMargin->value()};
    result.coverFlow      = m_coverFlowConfig->settings();
    result.background     = m_background->config();

    m_expansionConfig->updateConfig(result);
    return result;
}

void FilterConfigDialog::setConfig(const FilterWidget::ConfigData& config)
{
    TrackSelectionController::setCurrentAction(m_doubleClick, config.doubleClickAction);
    TrackSelectionController::setCurrentAction(m_middleClick, config.middleClickAction);
    TrackSelectionController::setCurrentAction(m_expansionDoubleClick, config.expansionDoubleClickAction);
    TrackSelectionController::setCurrentAction(m_expansionMiddleClick, config.expansionMiddleClickAction);

    m_playbackOnSend->setChecked(config.sendPlayback);
    m_source->setCurrentIndex(m_source->findData(static_cast<int>(config.source)));
    m_playlistEnabled->setChecked(config.playlistEnabled);
    m_autoSwitch->setChecked(config.autoSwitch);
    m_preservePlaybackPlaylist->setChecked(config.preservePlaybackPlaylist);
    m_playlistName->setText(config.playlistName);
    m_restoreState->setChecked(config.restoreState);

    if(config.viewMode == ExpandedTreeView::ViewMode::Tree) {
        m_columnsMode->setChecked(true);
    }
    else if(config.viewMode == ExpandedTreeView::ViewMode::CoverFlow) {
        m_coverFlowMode->setChecked(true);
    }
    else if(config.captionDisplay == ExpandedTreeView::CaptionDisplay::Right) {
        m_artworkRightMode->setChecked(true);
    }
    else if(config.captionDisplay == ExpandedTreeView::CaptionDisplay::Overlay) {
        m_artworkOverlayMode->setChecked(true);
    }
    else if(config.captionDisplay == ExpandedTreeView::CaptionDisplay::None) {
        m_artworkNoLabelsMode->setChecked(true);
    }
    else {
        m_artworkBottomMode->setChecked(true);
    }

    m_coverType->setCurrentIndex(m_coverType->findData(static_cast<int>(config.coverType)));
    m_coverSource->setCurrentIndex(
        m_coverSource->findData(config.coverSource ? static_cast<int>(*config.coverSource) : -1));
    m_showSummary->setChecked(config.showSummary);
    m_showHeader->setChecked(config.showHeader);
    m_showScrollbar->setChecked(config.showScrollbar);
    m_alternatingRows->setChecked(config.alternatingRows);
    m_overrideRowHeight->setChecked(config.rowHeight > 0);
    m_rowHeight->setValue(config.rowHeight > 0 ? config.rowHeight : 1);
    m_rowHeight->setEnabled(m_overrideRowHeight->isChecked());
    m_iconWidth->setValue(config.iconSize.width());
    m_iconHeight->setValue(config.iconSize.height());
    m_autoFitArtwork->setChecked(config.autoFitArtwork);
    m_iconHorizontalGap->setValue(config.iconHorizontalGap);
    m_iconVerticalGap->setValue(config.iconVerticalGap);
    m_artworkCornerRadius->setValue(config.artworkCornerRadius);
    m_overrideArtworkMargins->setChecked(config.overrideArtworkMargins);
    for(auto* margin : {m_artworkLeftMargin, m_artworkTopMargin, m_artworkRightMargin, m_artworkBottomMargin}) {
        margin->setEnabled(config.overrideArtworkMargins);
    }
    m_artworkLeftMargin->setValue(config.artworkMargins.left());
    m_artworkTopMargin->setValue(config.artworkMargins.top());
    m_artworkRightMargin->setValue(config.artworkMargins.right());
    m_artworkBottomMargin->setValue(config.artworkMargins.bottom());
    m_coverFlowConfig->setSettings(config.coverFlow);
    m_coverFlowConfig->setArtworkSettings(config.iconSize, config.artworkCornerRadius);
    m_background->setConfig(config.background);
    m_expansionConfig->setConfig(config);
    m_expansionDoubleClick->setEnabled(config.expandGroups);
    m_expansionMiddleClick->setEnabled(config.expandGroups);

    updatePlaylistControls();
}

void FilterConfigDialog::mergeExternalConfig(const FilterWidget::ConfigData& previous,
                                             const FilterWidget::ConfigData& current)
{
    mergeExternalFields(
        previous, current, &FilterWidget::ConfigData::doubleClickAction, &FilterWidget::ConfigData::middleClickAction,
        &FilterWidget::ConfigData::expansionDoubleClickAction, &FilterWidget::ConfigData::expansionMiddleClickAction,
        &FilterWidget::ConfigData::sendPlayback, &FilterWidget::ConfigData::source,
        &FilterWidget::ConfigData::playlistEnabled, &FilterWidget::ConfigData::autoSwitch,
        &FilterWidget::ConfigData::preservePlaybackPlaylist, &FilterWidget::ConfigData::playlistName,
        &FilterWidget::ConfigData::restoreState, &FilterWidget::ConfigData::viewMode,
        &FilterWidget::ConfigData::captionDisplay, &FilterWidget::ConfigData::coverType,
        &FilterWidget::ConfigData::coverSource, &FilterWidget::ConfigData::showSummary,
        &FilterWidget::ConfigData::showHeader, &FilterWidget::ConfigData::showScrollbar,
        &FilterWidget::ConfigData::alternatingRows, &FilterWidget::ConfigData::rowHeight,
        &FilterWidget::ConfigData::iconSize, &FilterWidget::ConfigData::iconHorizontalGap,
        &FilterWidget::ConfigData::autoFitArtwork, &FilterWidget::ConfigData::iconVerticalGap,
        &FilterWidget::ConfigData::artworkCornerRadius, &FilterWidget::ConfigData::overrideArtworkMargins,
        &FilterWidget::ConfigData::artworkMargins, &FilterWidget::ConfigData::coverFlow,
        &FilterWidget::ConfigData::expandGroups, &FilterWidget::ConfigData::expansionAnimated,
        &FilterWidget::ConfigData::expansionAnimationDuration, &FilterWidget::ConfigData::expansionAlternatingRows,
        &FilterWidget::ConfigData::expandedArtwork, &FilterWidget::ConfigData::expansionMinimumRows,
        &FilterWidget::ConfigData::expansionMaximumColumns, &FilterWidget::ConfigData::expansionArtworkOnLeft,
        &FilterWidget::ConfigData::expansionStretchColumns, &FilterWidget::ConfigData::expansionMinimumColumnWidth,
        &FilterWidget::ConfigData::expansionArtworkSize, &FilterWidget::ConfigData::expansionShowCloseButton,
        &FilterWidget::ConfigData::expansionArtworkReflection,
        &FilterWidget::ConfigData::expansionArtworkReflectionHeight,
        &FilterWidget::ConfigData::expansionArtworkReflectionOpacity,
        &FilterWidget::ConfigData::expansionArtworkReflectionGap,
        &FilterWidget::ConfigData::expansionArtworkAutoMargins, &FilterWidget::ConfigData::expansionArtworkMargins,
        &FilterWidget::ConfigData::expansionScripts, &FilterWidget::ConfigData::background,
        &FilterWidget::ConfigData::expansionBackground, &FilterWidget::ConfigData::expansionBackgroundColour,
        &FilterWidget::ConfigData::expansionPlayingRowFont, &FilterWidget::ConfigData::expansionPlayingRowColour);
}

void FilterConfigDialog::setupConnections()
{
    const auto updateAutoFitArtwork = [this]() {
        const bool coverFlow    = m_coverFlowMode->isChecked();
        const bool gridArtwork  = m_artworkBottomMode->isChecked() || m_artworkOverlayMode->isChecked()
                               || m_artworkNoLabelsMode->isChecked();
        const bool manualSizing = !coverFlow || !m_coverFlowConfig->settings().autoFitArtwork;

        m_autoFitArtwork->setEnabled(gridArtwork);
        m_iconWidth->setEnabled(manualSizing);
        m_iconHeight->setEnabled(manualSizing);
        m_iconHorizontalGap->setEnabled(gridArtwork);
        m_iconVerticalGap->setEnabled(gridArtwork);
        m_overrideArtworkMargins->setEnabled(!coverFlow);

        for(auto* margin : {m_artworkLeftMargin, m_artworkTopMargin, m_artworkRightMargin, m_artworkBottomMargin}) {
            margin->setEnabled(!coverFlow && m_overrideArtworkMargins->isChecked());
        }
    };

    updateAutoFitArtwork();
    QObject::connect(m_coverFlowMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkBottomMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkRightMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkOverlayMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkNoLabelsMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_coverFlowConfig, &CoverFlowConfigWidget::settingsChanged, this, updateAutoFitArtwork);

    const auto updateCoverFlowArtwork = [this]() {
        m_coverFlowConfig->setArtworkSettings({m_iconWidth->value(), m_iconHeight->value()},
                                              m_artworkCornerRadius->value());
    };
    for(auto* editor : {m_iconWidth, m_iconHeight, m_artworkCornerRadius}) {
        QObject::connect(editor, &QSpinBox::valueChanged, this, updateCoverFlowArtwork);
    }

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
    for(auto* margin : {m_artworkLeftMargin, m_artworkTopMargin, m_artworkRightMargin, m_artworkBottomMargin}) {
        QObject::connect(m_overrideArtworkMargins, &QCheckBox::toggled, margin, &QWidget::setEnabled);
    }
    QObject::connect(m_playlistEnabled, &QCheckBox::toggled, this, &FilterConfigDialog::updatePlaylistControls);
    QObject::connect(m_manageColumns, &QPushButton::clicked, this, [this]() {
        auto* dialog = new FilterColumnEditorDialog(m_columnRegistry, this);
        dialog->open();
    });

    QObject::connect(m_expansionConfig, &ExpansionConfigWidget::expansionEnabledChanged, m_expansionDoubleClick,
                     &QWidget::setEnabled);
    QObject::connect(m_expansionConfig, &ExpansionConfigWidget::expansionEnabledChanged, m_expansionMiddleClick,
                     &QWidget::setEnabled);
    QObject::connect(m_filterWidget, &FilterWidget::configChanged, this, &FilterConfigDialog::syncCurrentConfig);
    QObject::connect(m_filterWidget, &FilterWidget::expansionLayoutChanged, this,
                     &FilterConfigDialog::syncCurrentConfig);
}

void FilterConfigDialog::updatePlaylistControls()
{
    const bool enabled = m_playlistEnabled->isChecked();
    m_playlistName->setEnabled(enabled);
    m_autoSwitch->setEnabled(enabled);
    m_preservePlaybackPlaylist->setEnabled(enabled);
}

} // namespace Fooyin::Filters
