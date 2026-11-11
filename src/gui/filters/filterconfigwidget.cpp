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
#include "filterconstants.h"
#include "widgets/browsingconfigwidget.h"

#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/backgroundconfigwidget.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/hintlabel.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
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
    , m_playbackOnSend{new QCheckBox(this)}
    , m_playlistEnabled{new QCheckBox(this)}
    , m_autoSwitch{new QCheckBox(this)}
    , m_preservePlaybackPlaylist{new QCheckBox(this)}
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
    , m_artworkScaling{new QComboBox(this)}
    , m_iconHorizontalGap{new QSpinBox(this)}
    , m_iconVerticalGap{new QSpinBox(this)}
    , m_overrideGridMargins{new QCheckBox(tr("Custom grid margins"), this)}
    , m_gridLeftMargin{new QSpinBox(this)}
    , m_gridTopMargin{new QSpinBox(this)}
    , m_gridRightMargin{new QSpinBox(this)}
    , m_gridBottomMargin{new QSpinBox(this)}
    , m_artworkCornerRadius{new QSpinBox(this)}
    , m_artworkFrames{new QGroupBox(tr("Highlighting"), this)}
    , m_highlightHoveredCaptions{new QCheckBox(this)}
    , m_artworkHoverFrame{new QCheckBox(this)}
    , m_artworkHoverFrameWidth{new QSpinBox(this)}
    , m_artworkHoverFrameColour{new ColourButton(tr("Custom colour"), true, this)}
    , m_highlightSelectedCaptions{new QCheckBox(this)}
    , m_artworkSelectionFrame{new QCheckBox(this)}
    , m_artworkSelectionFrameWidth{new QSpinBox(this)}
    , m_artworkSelectionFrameColour{new ColourButton(tr("Custom colour"), true, this)}
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
    auto* artworkTab    = new QWidget(tabs);

    auto* backgroundTab    = new QWidget(tabs);
    auto* backgroundLayout = new QVBoxLayout(backgroundTab);
    backgroundLayout->addWidget(m_background);
    backgroundLayout->addStretch();

    tabs->addTab(generalTab, tr("General"));
    tabs->addTab(appearanceTab, tr("Display"));
    tabs->addTab(artworkTab, tr("Artwork"));
    tabs->addTab(backgroundTab, tr("Background"));
    tabs->addTab(m_expansionConfig, tr("Expansion"));
    tabs->addTab(m_coverFlowConfig, tr("Cover flow"));

    auto* clickBehaviour = new BrowsingActionConfigWidget({.doubleClick         = m_doubleClick,
                                                           .middleClick         = m_middleClick,
                                                           .startPlayback       = m_playbackOnSend,
                                                           .resultsEnabled      = m_playlistEnabled,
                                                           .expandedDoubleClick = m_expansionDoubleClick,
                                                           .expandedMiddleClick = m_expansionMiddleClick},
                                                          generalTab);
    auto* selectionPlaylist
        = new ResultsPlaylistConfigWidget(m_playlistEnabled, m_autoSwitch, m_preservePlaybackPlaylist, m_playlistName,
                                          {.doubleClick         = m_doubleClick,
                                           .middleClick         = m_middleClick,
                                           .startPlayback       = m_playbackOnSend,
                                           .resultsEnabled      = m_playlistEnabled,
                                           .expandedDoubleClick = m_expansionDoubleClick,
                                           .expandedMiddleClick = m_expansionMiddleClick},
                                          generalTab);
    int row{0};

    const auto updateSourceMode = [this, selectionPlaylist, clickBehaviour]() {
        const bool playlistSource = m_source->currentData().toInt() == static_cast<int>(FilterSource::CurrentPlaylist);
        selectionPlaylist->setEnabled(!playlistSource);
        clickBehaviour->setResultsAvailable(!playlistSource);

        if(playlistSource) {
            for(auto* combo : {m_doubleClick, m_middleClick, m_expansionDoubleClick, m_expansionMiddleClick}) {
                const auto action = static_cast<TrackAction>(combo->currentData().toInt());
                if(action == TrackAction::PlayResultsPlaylist || action == TrackAction::ReplaceResultsPlaylist) {
                    TrackSelectionController::setCurrentAction(combo, static_cast<int>(TrackAction::Play));
                }
            }
        }
    };
    QObject::connect(m_source, &QComboBox::currentIndexChanged, selectionPlaylist, updateSourceMode);
    updateSourceMode();

    auto* displayModeGroup = new QButtonGroup(this);
    for(auto* button : {m_columnsMode, m_coverFlowMode, m_artworkBottomMode, m_artworkRightMode, m_artworkOverlayMode,
                        m_artworkNoLabelsMode}) {
        displayModeGroup->addButton(button);
    }

    auto* viewMode          = new QGroupBox(tr("View mode"), appearanceTab);
    auto* displayModeLayout = new QVBoxLayout(viewMode);
    displayModeLayout->addWidget(m_columnsMode);
    displayModeLayout->addWidget(m_artworkBottomMode);
    displayModeLayout->addWidget(m_artworkRightMode);
    displayModeLayout->addWidget(m_artworkOverlayMode);
    displayModeLayout->addWidget(m_artworkNoLabelsMode);
    displayModeLayout->addWidget(m_coverFlowMode);
    displayModeLayout->addStretch();

    m_coverType->addItem(tr("Front cover"), static_cast<int>(Track::Cover::Front));
    m_coverType->addItem(tr("Back cover"), static_cast<int>(Track::Cover::Back));
    m_coverType->addItem(tr("Artist"), static_cast<int>(Track::Cover::Artist));

    m_coverSource->addItem(tr("Default"), -1);
    m_coverSource->addItem(tr("Embedded covers"), static_cast<int>(ArtworkSourcePreference::PreferEmbedded));
    m_coverSource->addItem(tr("Directory covers"), static_cast<int>(ArtworkSourcePreference::PreferDirectory));

    auto* displayOptions       = new QGroupBox(tr("Display options"), appearanceTab);
    auto* displayOptionsLayout = new QGridLayout(displayOptions);

    row = 0;
    displayOptionsLayout->addWidget(m_showSummary, row++, 0, 1, 3);
    displayOptionsLayout->addWidget(m_showHeader, row++, 0, 1, 3);
    displayOptionsLayout->addWidget(m_showScrollbar, row++, 0, 1, 3);
    displayOptionsLayout->addWidget(m_alternatingRows, row++, 0, 1, 3);
    displayOptionsLayout->addWidget(m_overrideRowHeight, row, 0, 1, 2);
    displayOptionsLayout->addWidget(m_rowHeight, row++, 2);
    displayOptionsLayout->setColumnStretch(3, 1);
    displayOptionsLayout->setRowStretch(row, 1);

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

    m_highlightHoveredCaptions->setToolTip(tr("Apply hover highlighting to captions in artwork views"));
    m_highlightSelectedCaptions->setToolTip(
        tr("Apply selection colours to caption backgrounds and text in artwork views"));

    auto* framesLayout = new QGridLayout(m_artworkFrames);
    framesLayout->setHorizontalSpacing(16);

    framesLayout->addWidget(new QLabel(tr("Captions"), m_artworkFrames), 0, 1, Qt::AlignHCenter);
    framesLayout->addWidget(new QLabel(tr("Frame"), m_artworkFrames), 0, 2, Qt::AlignHCenter);
    framesLayout->addWidget(new QLabel(tr("Width"), m_artworkFrames), 0, 3, Qt::AlignHCenter);
    framesLayout->addWidget(new QLabel(tr("Colour"), m_artworkFrames), 0, 4, Qt::AlignLeft);
    framesLayout->addWidget(new QLabel(tr("Hover"), m_artworkFrames), 1, 0, Qt::AlignHCenter);
    framesLayout->addWidget(m_highlightHoveredCaptions, 1, 1, Qt::AlignHCenter);
    framesLayout->addWidget(m_artworkHoverFrame, 1, 2, Qt::AlignHCenter);
    framesLayout->addWidget(m_artworkHoverFrameWidth, 1, 3);
    framesLayout->addWidget(m_artworkHoverFrameColour, 1, 4);
    framesLayout->addWidget(new QLabel(tr("Selection"), m_artworkFrames), 2, 0, Qt::AlignHCenter);
    framesLayout->addWidget(m_highlightSelectedCaptions, 2, 1, Qt::AlignHCenter);
    framesLayout->addWidget(m_artworkSelectionFrame, 2, 2, Qt::AlignHCenter);
    framesLayout->addWidget(m_artworkSelectionFrameWidth, 2, 3);
    framesLayout->addWidget(m_artworkSelectionFrameColour, 2, 4);
    framesLayout->setColumnStretch(4, 1);

    for(auto* width : {m_artworkHoverFrameWidth, m_artworkSelectionFrameWidth}) {
        width->setRange(1, 20);
        width->setSuffix(u" px"_s);
    }
    for(auto* colour : {m_artworkHoverFrameColour, m_artworkSelectionFrameColour}) {
        colour->setToolTip(tr("Use a custom frame colour instead of the theme’s highlight colour."));
    }

    for(auto* margin : {m_artworkLeftMargin, m_artworkTopMargin, m_artworkRightMargin, m_artworkBottomMargin,
                        m_gridLeftMargin, m_gridTopMargin, m_gridRightMargin, m_gridBottomMargin}) {
        margin->setRange(0, 999);
        margin->setSuffix(u" px"_s);
    }

    m_overrideGridMargins->setToolTip(tr("Set the space around the entire artwork grid."));

    auto* artworkGroup = new QGroupBox(tr("Image"), artworkTab);

    auto* sizeLayout = new QGridLayout();

    row = 0;
    sizeLayout->addWidget(new QLabel(tr("Width") + u":"_s, artworkGroup), row, 0);
    sizeLayout->addWidget(m_iconWidth, row, 1);
    sizeLayout->addWidget(new QLabel(tr("Height") + u":"_s, artworkGroup), row, 2);
    sizeLayout->addWidget(m_iconHeight, row++, 3);
    sizeLayout->setColumnStretch(4, 1);

    auto* gapsGroup  = new QGroupBox(tr("Gaps"), artworkTab);
    auto* gapsLayout = new QGridLayout(gapsGroup);

    row = 0;
    gapsLayout->addWidget(new QLabel(tr("Horizontal") + u":"_s, gapsGroup), row, 0);
    gapsLayout->addWidget(m_iconHorizontalGap, row++, 1);
    gapsLayout->addWidget(new QLabel(tr("Vertical") + u":"_s, gapsGroup), row, 0);
    gapsLayout->addWidget(m_iconVerticalGap, row++, 1);
    gapsLayout->setColumnStretch(2, 1);
    gapsLayout->setRowStretch(row, 1);

    auto* marginsGroup  = new QGroupBox(tr("Artwork margins"), artworkTab);
    auto* marginsLayout = new QGridLayout(marginsGroup);

    row = 0;
    marginsLayout->addWidget(m_overrideArtworkMargins, row++, 0, 1, 5);
    marginsLayout->addWidget(new QLabel(tr("Left") + u":"_s, marginsGroup), row, 0);
    marginsLayout->addWidget(m_artworkLeftMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Right") + u":"_s, marginsGroup), row, 2);
    marginsLayout->addWidget(m_artworkRightMargin, row++, 3);
    marginsLayout->addWidget(new QLabel(tr("Top") + u":"_s, marginsGroup), row, 0);
    marginsLayout->addWidget(m_artworkTopMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Bottom") + u":"_s, marginsGroup), row, 2);
    marginsLayout->addWidget(m_artworkBottomMargin, row++, 3);
    marginsLayout->setColumnStretch(4, 1);
    marginsLayout->setRowStretch(row, 1);

    auto* gridMarginsGroup  = new QGroupBox(tr("Grid margins"), artworkTab);
    auto* gridMarginsLayout = new QGridLayout(gridMarginsGroup);

    row = 0;
    gridMarginsLayout->addWidget(m_overrideGridMargins, row++, 0, 1, 5);
    gridMarginsLayout->addWidget(new QLabel(tr("Left") + u":"_s, gridMarginsGroup), row, 0);
    gridMarginsLayout->addWidget(m_gridLeftMargin, row, 1);
    gridMarginsLayout->addWidget(new QLabel(tr("Right") + u":"_s, gridMarginsGroup), row, 2);
    gridMarginsLayout->addWidget(m_gridRightMargin, row++, 3);
    gridMarginsLayout->addWidget(new QLabel(tr("Top") + u":"_s, gridMarginsGroup), row, 0);
    gridMarginsLayout->addWidget(m_gridTopMargin, row, 1);
    gridMarginsLayout->addWidget(new QLabel(tr("Bottom") + u":"_s, gridMarginsGroup), row, 2);
    gridMarginsLayout->addWidget(m_gridBottomMargin, row++, 3);
    gridMarginsLayout->setColumnStretch(4, 1);
    gridMarginsLayout->setRowStretch(row, 1);

    auto* iconSizeHint = new HintLabel(tr("Use <b>Ctrl+Scroll</b> in the widget to resize artwork. "
                                          "When fitting to the available width, it changes the number of columns."),
                                       this);
    iconSizeHint->setWordWrap(true);

    m_autoFitArtwork->setToolTip(tr("Resize artwork to fill the available width.\n"
                                    "Ctrl+Scroll changes the number of columns."));

    m_artworkScaling->addItem(tr("Fit"), static_cast<int>(ArtworkScaling::Fit));
    m_artworkScaling->addItem(tr("Fill (crop)"), static_cast<int>(ArtworkScaling::Fill));
    m_artworkScaling->addItem(tr("Stretch"), static_cast<int>(ArtworkScaling::Stretch));
    m_artworkScaling->setToolTip(tr("Fit shows the whole image and preserves its aspect ratio.\n"
                                    "Fill crops the image to fill the artwork area and preserves its aspect ratio.\n"
                                    "Stretch fills the artwork area by changing the image’s aspect ratio."));

    auto* artworkLayout = new QGridLayout(artworkGroup);

    row = 0;
    artworkLayout->addWidget(new QLabel(tr("Artwork type") + u":"_s, artworkGroup), row, 0);
    artworkLayout->addWidget(m_coverType, row++, 1, 1, 3);
    artworkLayout->addWidget(new QLabel(tr("Artwork source") + u":"_s, artworkGroup), row, 0);
    artworkLayout->addWidget(m_coverSource, row++, 1, 1, 3);
    artworkLayout->addWidget(new QLabel(tr("Artwork scaling") + u":"_s, artworkGroup), row, 0);
    artworkLayout->addWidget(m_artworkScaling, row++, 1, 1, 3);
    artworkLayout->addWidget(new QLabel(tr("Corner radius") + u":"_s, artworkGroup), row, 0);
    artworkLayout->addWidget(m_artworkCornerRadius, row++, 1);
    artworkLayout->addWidget(Gui::createSectionHeader(tr("Size"), artworkGroup), row++, 0, 1, 5);
    artworkLayout->addWidget(m_autoFitArtwork, row++, 0, 1, 5);
    artworkLayout->addLayout(sizeLayout, row++, 0, 1, 5);
    artworkLayout->addWidget(iconSizeHint, row++, 0, 1, 5);
    artworkLayout->setColumnStretch(4, 1);
    artworkLayout->setRowStretch(row, 1);

    auto* playlistSourceHint
        = new HintLabel(tr("Current playlist mode uses the displayed playlist as its source and selects the matching "
                           "tracks in that playlist."),
                        this);
    playlistSourceHint->setWordWrap(true);

    auto* generalGroup       = new QGroupBox(tr("General"), generalTab);
    auto* generalGroupLayout = new QGridLayout(generalGroup);

    m_source->addItem(tr("Library"), static_cast<int>(FilterSource::Library));
    m_source->addItem(tr("Current playlist"), static_cast<int>(FilterSource::CurrentPlaylist));

    row = 0;
    generalGroupLayout->addWidget(new QLabel(tr("Source") + u":"_s, this), row, 0);
    generalGroupLayout->addWidget(m_source, row++, 1);
    generalGroupLayout->addWidget(playlistSourceHint, row++, 0, 1, 3);
    generalGroupLayout->addWidget(m_restoreState, row++, 0, 1, 3);
    generalGroupLayout->addWidget(m_manageColumns, row++, 0, 1, 3);
    generalGroupLayout->setColumnStretch(2, 1);

    auto* generalLayout = new QGridLayout(generalTab);

    row = 0;
    generalLayout->addWidget(generalGroup, row++, 0);
    generalLayout->addWidget(clickBehaviour, row++, 0);
    generalLayout->addWidget(selectionPlaylist, row++, 0);
    generalLayout->setColumnStretch(0, 1);
    generalLayout->setRowStretch(row, 1);

    auto* spacingLayout = new QVBoxLayout();
    spacingLayout->addWidget(gapsGroup);
    spacingLayout->addWidget(marginsGroup);
    spacingLayout->addWidget(gridMarginsGroup);

    auto* appearanceTabLayout = new QGridLayout(appearanceTab);
    appearanceTabLayout->addWidget(viewMode, 0, 0);
    appearanceTabLayout->addWidget(displayOptions, 1, 0);
    appearanceTabLayout->setRowStretch(2, 1);

    auto* artworkTabLayout = new QGridLayout(artworkTab);
    artworkTabLayout->addWidget(artworkGroup, 0, 0);
    artworkTabLayout->addLayout(spacingLayout, 0, 1);
    artworkTabLayout->addWidget(m_artworkFrames, 1, 0, 1, 2);
    artworkTabLayout->setColumnStretch(0, 1);
    artworkTabLayout->setColumnStretch(1, 1);
    artworkTabLayout->setRowStretch(2, 1);

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
        BrowsingActionConfigWidget::addResultsActions(box);
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
    result.showSummary         = m_showSummary->isChecked();
    result.showHeader          = m_showHeader->isChecked();
    result.showScrollbar       = m_showScrollbar->isChecked();
    result.alternatingRows     = m_alternatingRows->isChecked();
    result.rowHeight           = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0;
    result.iconSize            = {m_iconWidth->value(), m_iconHeight->value()};
    result.autoFitArtwork      = m_autoFitArtwork->isChecked();
    result.artworkScaling      = artworkScalingFromInt(m_artworkScaling->currentData().toInt());
    result.iconHorizontalGap   = m_iconHorizontalGap->value();
    result.iconVerticalGap     = m_iconVerticalGap->value();
    result.overrideGridMargins = m_overrideGridMargins->isChecked();
    result.gridMargins         = {m_gridLeftMargin->value(), m_gridTopMargin->value(), m_gridRightMargin->value(),
                                  m_gridBottomMargin->value()};
    result.artworkCornerRadius = m_artworkCornerRadius->value();
    result.artworkHoverFrame
        = {.enabled = m_artworkHoverFrame->isChecked(),
           .width   = m_artworkHoverFrameWidth->value(),
           .colour  = m_artworkHoverFrameColour->isChecked() ? m_artworkHoverFrameColour->colour() : QColor{}};
    result.artworkSelectionFrame
        = {.enabled = m_artworkSelectionFrame->isChecked(),
           .width   = m_artworkSelectionFrameWidth->value(),
           .colour  = m_artworkSelectionFrameColour->isChecked() ? m_artworkSelectionFrameColour->colour() : QColor{}};
    result.highlightHoveredCaptions  = m_highlightHoveredCaptions->isChecked();
    result.highlightSelectedCaptions = m_highlightSelectedCaptions->isChecked();
    result.overrideArtworkMargins    = m_overrideArtworkMargins->isChecked();
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
    m_artworkScaling->setCurrentIndex(m_artworkScaling->findData(static_cast<int>(config.artworkScaling)));
    m_iconHorizontalGap->setValue(config.iconHorizontalGap);
    m_iconVerticalGap->setValue(config.iconVerticalGap);
    m_overrideGridMargins->setChecked(config.overrideGridMargins);
    for(auto* margin : {m_gridLeftMargin, m_gridTopMargin, m_gridRightMargin, m_gridBottomMargin}) {
        margin->setEnabled(config.viewMode == ExpandedTreeView::ViewMode::Icon && config.overrideGridMargins);
    }
    m_gridLeftMargin->setValue(config.gridMargins.left());
    m_gridTopMargin->setValue(config.gridMargins.top());
    m_gridRightMargin->setValue(config.gridMargins.right());
    m_gridBottomMargin->setValue(config.gridMargins.bottom());
    m_artworkCornerRadius->setValue(config.artworkCornerRadius);
    m_artworkHoverFrame->setChecked(config.artworkHoverFrame.enabled);
    m_artworkHoverFrameWidth->setValue(config.artworkHoverFrame.width);
    m_artworkHoverFrameWidth->setEnabled(config.artworkHoverFrame.enabled);
    m_artworkHoverFrameColour->setChecked(config.artworkHoverFrame.colour.isValid());
    m_artworkHoverFrameColour->setColour(config.artworkHoverFrame.colour.isValid()
                                             ? config.artworkHoverFrame.colour
                                             : palette().color(QPalette::Highlight));
    m_artworkHoverFrameColour->setEnabled(config.artworkHoverFrame.enabled);
    m_artworkSelectionFrame->setChecked(config.artworkSelectionFrame.enabled);
    m_artworkSelectionFrameWidth->setValue(config.artworkSelectionFrame.width);
    m_artworkSelectionFrameWidth->setEnabled(config.artworkSelectionFrame.enabled);
    m_artworkSelectionFrameColour->setChecked(config.artworkSelectionFrame.colour.isValid());
    m_artworkSelectionFrameColour->setColour(config.artworkSelectionFrame.colour.isValid()
                                                 ? config.artworkSelectionFrame.colour
                                                 : palette().color(QPalette::Highlight));
    m_artworkSelectionFrameColour->setEnabled(config.artworkSelectionFrame.enabled);
    m_highlightHoveredCaptions->setChecked(config.highlightHoveredCaptions);
    m_highlightSelectedCaptions->setChecked(config.highlightSelectedCaptions);
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
    m_coverFlowConfig->setArtworkFrames(config.artworkHoverFrame, config.artworkSelectionFrame);
    m_background->setConfig(config.background);
    m_expansionConfig->setConfig(config);
    m_expansionDoubleClick->setEnabled(config.expandGroups);
    m_expansionMiddleClick->setEnabled(config.expandGroups);
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
        &FilterWidget::ConfigData::autoFitArtwork, &FilterWidget::ConfigData::artworkScaling,
        &FilterWidget::ConfigData::iconVerticalGap, &FilterWidget::ConfigData::overrideGridMargins,
        &FilterWidget::ConfigData::gridMargins, &FilterWidget::ConfigData::artworkCornerRadius,
        &FilterWidget::ConfigData::artworkHoverFrame, &FilterWidget::ConfigData::artworkSelectionFrame,
        &FilterWidget::ConfigData::highlightHoveredCaptions, &FilterWidget::ConfigData::highlightSelectedCaptions,
        &FilterWidget::ConfigData::overrideArtworkMargins, &FilterWidget::ConfigData::artworkMargins,
        &FilterWidget::ConfigData::coverFlow, &FilterWidget::ConfigData::expandGroups,
        &FilterWidget::ConfigData::expansionPosition, &FilterWidget::ConfigData::expansionAnimated,
        &FilterWidget::ConfigData::expansionAnimationDuration, &FilterWidget::ConfigData::expansionAlternatingRows,
        &FilterWidget::ConfigData::expansionMatchGridMargins, &FilterWidget::ConfigData::expandedArtwork,
        &FilterWidget::ConfigData::expansionMinimumRows, &FilterWidget::ConfigData::expansionMaximumColumns,
        &FilterWidget::ConfigData::expansionArtworkOnLeft, &FilterWidget::ConfigData::expansionStretchColumns,
        &FilterWidget::ConfigData::expansionMinimumColumnWidth, &FilterWidget::ConfigData::expansionArtworkSize,
        &FilterWidget::ConfigData::expansionShowCloseButton, &FilterWidget::ConfigData::expansionArtworkReflection,
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
        const bool manualSizing = coverFlow ? !m_coverFlowConfig->settings().autoFitArtwork
                                            : !gridArtwork || !m_autoFitArtwork->isChecked();
        const auto viewMode     = m_columnsMode->isChecked() ? ExpandedTreeView::ViewMode::Tree
                                : coverFlow                  ? ExpandedTreeView::ViewMode::CoverFlow
                                                             : ExpandedTreeView::ViewMode::Icon;
        const int minIconSize   = Constants::minimumIconSize(
            viewMode, m_artworkOverlayMode->isChecked()  ? ExpandedTreeView::CaptionDisplay::Overlay
                      : m_artworkBottomMode->isChecked() ? ExpandedTreeView::CaptionDisplay::Bottom
                                                         : ExpandedTreeView::CaptionDisplay::None);

        m_iconWidth->setMinimum(minIconSize);
        m_iconHeight->setMinimum(minIconSize);
        m_iconWidth->setMaximum(Constants::maximumIconSize(viewMode));
        m_iconHeight->setMaximum(Constants::maximumIconSize(viewMode));

        m_autoFitArtwork->setEnabled(gridArtwork);
        m_iconWidth->setEnabled(manualSizing);
        m_iconHeight->setEnabled(manualSizing);
        m_iconHorizontalGap->setEnabled(gridArtwork);
        m_iconVerticalGap->setEnabled(gridArtwork);

        const bool iconMode = !m_columnsMode->isChecked() && !coverFlow;
        m_artworkScaling->setEnabled(iconMode);
        m_artworkFrames->setEnabled(!m_columnsMode->isChecked());
        const bool hasCaptions = iconMode && !m_artworkNoLabelsMode->isChecked();
        m_highlightHoveredCaptions->setEnabled(hasCaptions);
        m_highlightSelectedCaptions->setEnabled(hasCaptions);
        m_overrideGridMargins->setEnabled(iconMode);
        for(auto* margin : {m_gridLeftMargin, m_gridTopMargin, m_gridRightMargin, m_gridBottomMargin}) {
            margin->setEnabled(iconMode && m_overrideGridMargins->isChecked());
        }
        m_overrideArtworkMargins->setEnabled(!coverFlow);

        for(auto* margin : {m_artworkLeftMargin, m_artworkTopMargin, m_artworkRightMargin, m_artworkBottomMargin}) {
            margin->setEnabled(!coverFlow && m_overrideArtworkMargins->isChecked());
        }
    };

    updateAutoFitArtwork();
    QObject::connect(m_columnsMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_coverFlowMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkBottomMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkRightMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkOverlayMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkNoLabelsMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_autoFitArtwork, &QCheckBox::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_coverFlowConfig, &CoverFlowConfigWidget::settingsChanged, this, updateAutoFitArtwork);
    QObject::connect(m_overrideGridMargins, &QCheckBox::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);

    const auto updateCoverFlowArtwork = [this]() {
        m_coverFlowConfig->setArtworkSettings({m_iconWidth->value(), m_iconHeight->value()},
                                              m_artworkCornerRadius->value());
    };
    for(const auto* editor : {m_iconWidth, m_iconHeight, m_artworkCornerRadius}) {
        QObject::connect(editor, &QSpinBox::valueChanged, this, updateCoverFlowArtwork);
    }

    const auto updateArtworkFrames = [this]() {
        const auto current{config()};
        m_coverFlowConfig->setArtworkFrames(current.artworkHoverFrame, current.artworkSelectionFrame);
    };
    for(const auto* enabled : {m_artworkHoverFrame, m_artworkSelectionFrame}) {
        QObject::connect(enabled, &QCheckBox::toggled, this, updateArtworkFrames);
    }
    for(const auto* width : {m_artworkHoverFrameWidth, m_artworkSelectionFrameWidth}) {
        QObject::connect(width, &QSpinBox::valueChanged, this, updateArtworkFrames);
    }
    for(const auto* colour : {m_artworkHoverFrameColour, m_artworkSelectionFrameColour}) {
        QObject::connect(colour, &ColourButton::colourUpdated, this, updateArtworkFrames);
        QObject::connect(colour, &ColourButton::toggled, this, updateArtworkFrames);
    }

    QObject::connect(m_artworkHoverFrame, &QCheckBox::toggled, m_artworkHoverFrameWidth, &QWidget::setEnabled);
    QObject::connect(m_artworkHoverFrame, &QCheckBox::toggled, m_artworkHoverFrameColour, &QWidget::setEnabled);
    QObject::connect(m_artworkSelectionFrame, &QCheckBox::toggled, m_artworkSelectionFrameWidth, &QWidget::setEnabled);
    QObject::connect(m_artworkSelectionFrame, &QCheckBox::toggled, m_artworkSelectionFrameColour, &QWidget::setEnabled);

    for(auto* margin : {m_artworkLeftMargin, m_artworkTopMargin, m_artworkRightMargin, m_artworkBottomMargin}) {
        QObject::connect(m_overrideArtworkMargins, &QCheckBox::toggled, margin, &QWidget::setEnabled);
    }
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

} // namespace Fooyin::Filters
