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
#include "filterexpansionwidget.h"
#include "trackgenerator.h"

#include <gui/guiutils.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/backgroundconfigwidget.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <gui/widgets/hintlabel.h>
#include <gui/widgets/scriptlineedit.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScopedValueRollback>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimerEvent>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
namespace {
class TabWidget : public QTabWidget
{
public:
    using QTabWidget::QTabWidget;

    [[nodiscard]] QSize minimumSizeHint() const override
    {
        QSize hint = QTabWidget::sizeHint();

        if(tabPosition() == North || tabPosition() == South) {
            QStyleOptionTabWidgetFrame opt;
            initStyleOption(&opt);

            opt.rect = QRect(QPoint{}, hint);

            const QRect tabRect     = style()->subElementRect(QStyle::SE_TabWidgetTabBar, &opt, this);
            const int requiredWidth = tabBar()->sizeHint().width();
            const int shortfall     = requiredWidth - tabRect.width();

            if(shortfall > 0) {
                hint.rwidth() += shortfall;
            }
        }

        return hint;
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        return minimumSizeHint();
    }
};
} // namespace

FilterConfigDialog::FilterConfigDialog(FilterWidget* filterWidget, FilterColumnRegistry* columnRegistry,
                                       QWidget* parent)
    : WidgetConfigDialog{filterWidget, tr("Filter Settings"), parent}
    , m_filterWidget{filterWidget}
    , m_scriptsTab{new QWidget(this)}
    , m_headerScript{new ScriptTextEdit(this)}
    , m_trackScript{new ScriptTextEdit(this)}
    , m_footerScript{new ScriptTextEdit(this)}
    , m_groupByScript{new ScriptTextEdit(this)}
    , m_subheaderScript{new ScriptTextEdit(this)}
    , m_preview{new FilterExpansionWidget(nullptr, nullptr, this)}
    , m_previewTracks{std::make_shared<const TrackList>(TrackGenerator::generateTracks({.albumCount = 1}))}
    , m_previewSubscription{nullptr}
    , m_previewGeneration{0}
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
    , m_artworkLeftMargin{new QSpinBox(this)}
    , m_artworkTopMargin{new QSpinBox(this)}
    , m_artworkRightMargin{new QSpinBox(this)}
    , m_artworkBottomMargin{new QSpinBox(this)}
    , m_background{new BackgroundConfigWidget(this)}
    , m_expansionBackground{new BackgroundConfigWidget(this)}
    , m_expansionBackgroundColour{new ColourButton(tr("Background"), true, this)}
    , m_expansionPlayingRowFont{new FontButton(tr("Font") + u":"_s, true, this)}
    , m_expansionPlayingRowColour{new ColourButton(tr("Background colour") + u":"_s, true, this)}
    , m_expandGroups{new QCheckBox(tr("Expand selected group"), this)}
    , m_expansionAnimated{new QCheckBox(tr("Animate expansion and collapse"), this)}
    , m_expansionAnimationDuration{new QSpinBox(this)}
    , m_expansionAlternatingRows{new QCheckBox(tr("Alternating row colours"), this)}
    , m_expandedArtwork{new QCheckBox(tr("Show artwork in expanded groups"), this)}
    , m_expansionCloseButton{new QCheckBox(tr("Show close button"), this)}
    , m_expansionArtworkReflection{new QCheckBox(tr("Show artwork reflection"), this)}
    , m_expansionArtworkReflectionHeight{new QSpinBox(this)}
    , m_expansionArtworkReflectionOpacity{new QSpinBox(this)}
    , m_expansionArtworkReflectionGap{new QSpinBox(this)}
    , m_expansionArtworkAutoMargins{new QCheckBox(tr("Remove margins when artwork fills the height"), this)}
    , m_expansionArtworkLeftMargin{new QSpinBox(this)}
    , m_expansionArtworkTopMargin{new QSpinBox(this)}
    , m_expansionArtworkRightMargin{new QSpinBox(this)}
    , m_expansionArtworkBottomMargin{new QSpinBox(this)}
    , m_expansionArtworkPosition{new QComboBox(this)}
    , m_expansionArtworkSize{new QSpinBox(this)}
    , m_expansionMinimumRows{new QSpinBox(this)}
    , m_expansionMaximumColumns{new QSpinBox(this)}
    , m_expansionColumnSizing{new QComboBox(this)}
    , m_expansionMinimumColumnWidth{new QSpinBox(this)}
    , m_manageColumns{new QPushButton(tr("Manage columns…"), this)}
{
    auto* tabs = new QTabWidget(this);

    auto* generalTab    = new QWidget(tabs);
    auto* appearanceTab = new QWidget(tabs);

    const auto backgroundPage = [this](BackgroundConfigWidget* background, ColourButton* colour) {
        auto* content = new QWidget(this);
        auto* layout  = new QVBoxLayout(content);
        if(colour) {
            layout->addWidget(Gui::createSectionHeader(tr("Colour"), content));
            layout->addWidget(colour);
        }
        layout->addWidget(background);
        layout->addStretch();
        return content;
    };

    tabs->addTab(generalTab, tr("General"));
    tabs->addTab(appearanceTab, tr("Appearance"));
    tabs->addTab(backgroundPage(m_background, nullptr), tr("Background"));
    tabs->addTab(m_scriptsTab, tr("Expansion"));

    QObject::connect(tabs, &QTabWidget::currentChanged, this, &FilterConfigDialog::updatePreviewVisibility);

    m_expansionBackground->setOptions({});

    auto* expansionTabs      = new TabWidget(m_scriptsTab);
    auto* expansionBehaviour = new QWidget(expansionTabs);
    auto* expandedArtwork    = new QWidget(expansionTabs);

    expansionTabs->addTab(expansionBehaviour, tr("Behaviour"));
    expansionTabs->addTab(expandedArtwork, tr("Artwork"));
    expansionTabs->addTab(backgroundPage(m_expansionBackground, m_expansionBackgroundColour), tr("Background"));

    auto* scriptsPage   = new QWidget(m_scriptsTab);
    auto* scriptEditors = new TabWidget(scriptsPage);

    auto* scriptLayout = new QVBoxLayout(scriptsPage);
    scriptLayout->setContentsMargins({});
    scriptLayout->addWidget(scriptEditors, 1);

    m_groupByScript->setPlaceholderText(tr("Use display script when empty"));
    m_subheaderScript->setPlaceholderText(tr("Leave empty to disable subheaders"));

    auto* subheaderTab    = new QWidget(scriptEditors);
    auto* subheaderLayout = new QGridLayout(subheaderTab);

    subheaderLayout->addWidget(new QLabel(tr("Grouping script") + u":"_s, subheaderTab), 0, 0);
    subheaderLayout->addWidget(m_groupByScript, 1, 0);
    subheaderLayout->addWidget(new QLabel(tr("Display script") + u":"_s, subheaderTab), 0, 1);
    subheaderLayout->addWidget(m_subheaderScript, 1, 1);
    subheaderLayout->setColumnStretch(0, 1);
    subheaderLayout->setColumnStretch(1, 1);
    subheaderLayout->setRowStretch(1, 1);

    scriptEditors->addTab(m_headerScript, tr("Header"));
    scriptEditors->addTab(subheaderTab, tr("Subheader"));
    scriptEditors->addTab(m_trackScript, tr("Track"));
    scriptEditors->addTab(m_footerScript, tr("Footer"));

    m_footerScript->setPlaceholderText(tr("Leave empty to hide the footer"));

    const auto artwork = TrackGenerator::generateArtwork();
    m_preview->setPreviewArtwork(artwork, m_expandedArtwork->isChecked());
    QObject::connect(m_expandedArtwork, &QCheckBox::toggled, m_preview,
                     [this, artwork](bool show) { m_preview->setPreviewArtwork(artwork, show); });

    m_preview->setPreviewMode();
    m_preview->hide();

    auto* syntaxHint = new HintLabel(tr("Use <b>&lt;left&gt;</b> and <b>&lt;right&gt;</b> for alignment, "
                                        "and <b>&lt;hr/&gt;</b> for rules."),
                                     scriptsPage);
    syntaxHint->setWordWrap(true);
    auto* columnsHint = new HintLabel(
        tr("Use <b>%filter_columns%</b> for all column values, <b>%filter_column_count%</b> for their count, "
           "or <b>$filter_column(1)</b> for an individual column in displayed order."),
        scriptsPage);
    columnsHint->setWordWrap(true);

    scriptLayout->addWidget(syntaxHint);
    scriptLayout->addWidget(columnsHint);

    auto* scriptsLayout = new QHBoxLayout(m_scriptsTab);
    auto* previewLayout = new QVBoxLayout();

    scriptsLayout->addWidget(expansionTabs, 1);
    scriptsLayout->addLayout(previewLayout, 1);
    previewLayout->addWidget(scriptsPage, 1);

    auto* expandedArtworkLayout = new QGridLayout(expandedArtwork);

    m_expansionArtworkPosition->addItem(tr("Left"), true);
    m_expansionArtworkPosition->addItem(tr("Right"), false);

    m_expansionArtworkSize->setRange(50, 200);
    m_expansionArtworkSize->setSingleStep(10);
    m_expansionArtworkSize->setValue(100);
    m_expansionArtworkSize->setSuffix(u" %"_s);
    m_expansionArtworkSize->setToolTip(
        tr("Artwork shrinks to leave room for track text and is hidden when space is too limited."));

    m_expansionArtworkAutoMargins->setToolTip(
        tr("Use the configured margins only when the track content is taller than the artwork."));

    for(auto* margin : {m_expansionArtworkLeftMargin, m_expansionArtworkTopMargin, m_expansionArtworkRightMargin,
                        m_expansionArtworkBottomMargin}) {
        margin->setRange(0, 999);
        margin->setValue(10);
        margin->setSuffix(u" px"_s);
    }

    m_expansionArtworkReflection->setToolTip(tr("Hidden when there isn’t enough space below the artwork"));
    m_expansionArtworkReflectionHeight->setRange(1, 100);
    m_expansionArtworkReflectionHeight->setSuffix(u" %"_s);
    m_expansionArtworkReflectionHeight->setToolTip(tr("Reflection height as a percentage of the artwork height"));
    m_expansionArtworkReflectionOpacity->setRange(0, 100);
    m_expansionArtworkReflectionOpacity->setSuffix(u" %"_s);
    m_expansionArtworkReflectionOpacity->setToolTip(tr("Opacity at the top of the reflection, fading to transparent"));
    m_expansionArtworkReflectionGap->setRange(0, 100);
    m_expansionArtworkReflectionGap->setSuffix(u" px"_s);
    m_expansionArtworkReflectionGap->setToolTip(tr("Space between the artwork and its reflection"));

    auto* artworkMarginsLayout = new QGridLayout();
    artworkMarginsLayout->addWidget(new QLabel(tr("Left") + u":"_s, expandedArtwork), 0, 0);
    artworkMarginsLayout->addWidget(m_expansionArtworkLeftMargin, 0, 1);
    artworkMarginsLayout->addWidget(new QLabel(tr("Right") + u":"_s, expandedArtwork), 0, 2);
    artworkMarginsLayout->addWidget(m_expansionArtworkRightMargin, 0, 3);
    artworkMarginsLayout->addWidget(new QLabel(tr("Top") + u":"_s, expandedArtwork), 1, 0);
    artworkMarginsLayout->addWidget(m_expansionArtworkTopMargin, 1, 1);
    artworkMarginsLayout->addWidget(new QLabel(tr("Bottom") + u":"_s, expandedArtwork), 1, 2);
    artworkMarginsLayout->addWidget(m_expansionArtworkBottomMargin, 1, 3);
    artworkMarginsLayout->setColumnStretch(4, 1);

    auto* artworkSizeLabel = new QLabel(tr("Artwork size") + u":"_s, expandedArtwork);
    artworkSizeLabel->setBuddy(m_expansionArtworkSize);

    int row{0};
    expandedArtworkLayout->addWidget(m_expandedArtwork, row++, 0, 1, 5);
    expandedArtworkLayout->addWidget(new QLabel(tr("Artwork position") + u":"_s, expandedArtwork), row, 0);
    expandedArtworkLayout->addWidget(m_expansionArtworkPosition, row++, 1);
    expandedArtworkLayout->addWidget(artworkSizeLabel, row, 0);
    expandedArtworkLayout->addWidget(m_expansionArtworkSize, row++, 1);
    expandedArtworkLayout->addWidget(Gui::createSectionHeader(tr("Margins"), expandedArtwork), row++, 0, 1, 5);
    expandedArtworkLayout->addLayout(artworkMarginsLayout, row++, 0, 1, 5);
    expandedArtworkLayout->addWidget(m_expansionArtworkAutoMargins, row++, 0, 1, 5);
    expandedArtworkLayout->addWidget(Gui::createSectionHeader(tr("Artwork reflection"), expandedArtwork), row++, 0, 1,
                                     5);
    expandedArtworkLayout->addWidget(m_expansionArtworkReflection, row++, 0, 1, 5);
    expandedArtworkLayout->addWidget(new QLabel(tr("Height") + u":"_s, expandedArtwork), row, 0);
    expandedArtworkLayout->addWidget(m_expansionArtworkReflectionHeight, row++, 1);
    expandedArtworkLayout->addWidget(new QLabel(tr("Opacity") + u":"_s, expandedArtwork), row, 0);
    expandedArtworkLayout->addWidget(m_expansionArtworkReflectionOpacity, row++, 1);
    expandedArtworkLayout->addWidget(new QLabel(tr("Gap below artwork") + u":"_s, expandedArtwork), row, 0);
    expandedArtworkLayout->addWidget(m_expansionArtworkReflectionGap, row++, 1);
    expandedArtworkLayout->setColumnStretch(4, 1);
    expandedArtworkLayout->setRowStretch(row, 1);

    m_expansionMaximumColumns->setRange(0, 999);
    m_expansionMaximumColumns->setSpecialValueText(tr("Unlimited"));
    m_expansionMaximumColumns->setToolTip(tr("Set to 0 for unlimited columns."));

    m_expansionMinimumRows->setRange(1, 999);
    m_expansionMinimumRows->setValue(4);
    m_expansionMinimumRows->setToolTip(tr("The final column may contain fewer rows."));

    m_expansionColumnSizing->addItem(tr("Fit content"), false);
    m_expansionColumnSizing->addItem(tr("Fill available space"), true);

    m_expansionMinimumColumnWidth->setRange(25, 300);
    m_expansionMinimumColumnWidth->setSingleStep(10);
    m_expansionMinimumColumnWidth->setSuffix(u" %"_s);
    m_expansionMinimumColumnWidth->setToolTip(
        tr("The minimum column width is relative to the default width at the current font size.\n"
           "Longer content can exceed this minimum."));

    m_expansionPlayingRowColour->setToolTip(
        tr("Use a custom background colour for the currently playing row; transparency is supported"));
    m_expansionPlayingRowFont->setToolTip(tr("Use a custom font for the currently playing row"));

    m_expansionAnimationDuration->setRange(50, 2000);
    m_expansionAnimationDuration->setSingleStep(50);
    m_expansionAnimationDuration->setSuffix(u" ms"_s);

    auto* expansionBehaviourLayout = new QGridLayout(expansionBehaviour);

    row = 0;
    expansionBehaviourLayout->addWidget(m_expandGroups, row++, 0, 1, 3);
    expansionBehaviourLayout->addWidget(m_expansionAnimated, row++, 0, 1, 3);
    expansionBehaviourLayout->addWidget(new QLabel(tr("Animation duration") + u":"_s, expansionBehaviour), row, 0);
    expansionBehaviourLayout->addWidget(m_expansionAnimationDuration, row++, 1);
    expansionBehaviourLayout->addWidget(m_expansionCloseButton, row++, 0, 1, 3);
    expansionBehaviourLayout->addWidget(m_expansionAlternatingRows, row++, 0, 1, 3);
    expansionBehaviourLayout->addWidget(new QLabel(tr("Maximum number of columns") + u":"_s, expansionBehaviour), row,
                                        0);
    expansionBehaviourLayout->addWidget(m_expansionMaximumColumns, row++, 1);
    expansionBehaviourLayout->addWidget(new QLabel(tr("Minimum rows per column") + u":"_s, expansionBehaviour), row, 0);
    expansionBehaviourLayout->addWidget(m_expansionMinimumRows, row++, 1);
    expansionBehaviourLayout->addWidget(new QLabel(tr("Column sizing") + u":"_s, expansionBehaviour), row, 0);
    expansionBehaviourLayout->addWidget(m_expansionColumnSizing, row++, 1);
    expansionBehaviourLayout->addWidget(new QLabel(tr("Minimum column width") + u":"_s, expansionBehaviour), row, 0);
    expansionBehaviourLayout->addWidget(m_expansionMinimumColumnWidth, row++, 1);
    expansionBehaviourLayout->addWidget(Gui::createSectionHeader(tr("Playing track"), expansionBehaviour), row++, 0, 1,
                                        3);
    expansionBehaviourLayout->addWidget(m_expansionPlayingRowColour, row++, 0, 1, 3);
    expansionBehaviourLayout->addWidget(m_expansionPlayingRowFont, row++, 0, 1, 3);
    expansionBehaviourLayout->setColumnStretch(2, 1);
    expansionBehaviourLayout->setRowStretch(row, 1);

    auto* previewGroup = new QGroupBox(tr("Preview"), m_scriptsTab);

    auto* previewGroupLayout = new QVBoxLayout(previewGroup);
    previewGroupLayout->addWidget(m_preview);
    previewLayout->addWidget(previewGroup);

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
    for(auto* button :
        {m_columnsMode, m_artworkBottomMode, m_artworkRightMode, m_artworkOverlayMode, m_artworkNoLabelsMode}) {
        displayModeGroup->addButton(button);
    }

    auto* displayModeLayout = new QVBoxLayout();
    displayModeLayout->addWidget(m_columnsMode);
    displayModeLayout->addWidget(m_artworkBottomMode);
    displayModeLayout->addWidget(m_artworkRightMode);
    displayModeLayout->addWidget(m_artworkOverlayMode);
    displayModeLayout->addWidget(m_artworkNoLabelsMode);

    m_coverType->addItem(tr("Front cover"), static_cast<int>(Track::Cover::Front));
    m_coverType->addItem(tr("Back cover"), static_cast<int>(Track::Cover::Back));
    m_coverType->addItem(tr("Artist"), static_cast<int>(Track::Cover::Artist));

    m_coverSource->addItem(tr("Default"), -1);
    m_coverSource->addItem(tr("Embedded covers"), static_cast<int>(ArtworkSourcePreference::PreferEmbedded));
    m_coverSource->addItem(tr("Directory covers"), static_cast<int>(ArtworkSourcePreference::PreferDirectory));

    auto* appearance       = new QGroupBox(tr("Display"), appearanceTab);
    auto* appearanceLayout = new QGridLayout(appearance);

    row = 0;
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

    auto* iconSizeHint = new HintLabel(tr("Use <b>Ctrl+Scroll</b> in the widget to resize icons."), this);

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
    updatePreview(true);
}

void FilterConfigDialog::changeEvent(QEvent* event)
{
    ConfigDialog::changeEvent(event);

    switch(event->type()) {
        case QEvent::PaletteChange:
        case QEvent::ActivationChange:
            if(!m_expansionBackgroundColour->isChecked()) {
                m_expansionBackgroundColour->setColour(m_preview->defaultBackgroundColour());
            }
            if(!m_expansionPlayingRowColour->isChecked()) {
                m_expansionPlayingRowColour->setColour(Gui::defaultPlayingRowColour());
            }
        default:
            break;
    }
}

void FilterConfigDialog::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();
        if(m_scriptsTab->isVisible()) {
            updatePreview();
        }
        return;
    }
    ConfigDialog::timerEvent(event);
}

FilterWidget::ConfigData FilterConfigDialog::config() const
{
    auto expansionBackground{m_expansionBackground->config()};
    expansionBackground.fadeDuration = 0;

    const auto captions   = m_artworkRightMode->isChecked()    ? ExpandedTreeView::CaptionDisplay::Right
                          : m_artworkOverlayMode->isChecked()  ? ExpandedTreeView::CaptionDisplay::Overlay
                          : m_artworkNoLabelsMode->isChecked() ? ExpandedTreeView::CaptionDisplay::None
                                                               : ExpandedTreeView::CaptionDisplay::Bottom;
    const int coverSource = m_coverSource->currentData().toInt();
    return {
        .doubleClickAction          = m_doubleClick->currentData().toInt(),
        .middleClickAction          = m_middleClick->currentData().toInt(),
        .expansionDoubleClickAction = m_expansionDoubleClick->currentData().toInt(),
        .expansionMiddleClickAction = m_expansionMiddleClick->currentData().toInt(),
        .sendPlayback               = m_playbackOnSend->isChecked(),
        .source                     = static_cast<FilterSource>(m_source->currentData().toInt()),
        .playlistEnabled            = m_playlistEnabled->isChecked(),
        .autoSwitch                 = m_autoSwitch->isChecked(),
        .preservePlaybackPlaylist   = m_preservePlaybackPlaylist->isChecked(),
        .playlistName               = m_playlistName->text(),
        .restoreState               = m_restoreState->isChecked(),
        .viewMode = m_columnsMode->isChecked() ? ExpandedTreeView::ViewMode::Tree : ExpandedTreeView::ViewMode::Icon,
        .captionDisplay = captions,
        .coverType      = static_cast<Track::Cover>(m_coverType->currentData().toInt()),
        .coverSource
        = coverSource < 0 ? std::nullopt : std::optional{static_cast<ArtworkSourcePreference>(coverSource)},
        .showSummary         = m_showSummary->isChecked(),
        .showHeader          = m_showHeader->isChecked(),
        .showScrollbar       = m_showScrollbar->isChecked(),
        .alternatingRows     = m_alternatingRows->isChecked(),
        .rowHeight           = m_overrideRowHeight->isChecked() ? m_rowHeight->value() : 0,
        .iconSize            = {m_iconWidth->value(), m_iconHeight->value()},
        .autoFitArtwork      = m_autoFitArtwork->isChecked(),
        .iconHorizontalGap   = m_iconHorizontalGap->value(),
        .iconVerticalGap     = m_iconVerticalGap->value(),
        .artworkCornerRadius = m_artworkCornerRadius->value(),
        .artworkMargins    = {m_artworkLeftMargin->value(), m_artworkTopMargin->value(), m_artworkRightMargin->value(),
                              m_artworkBottomMargin->value()},
        .background        = m_background->config(),
        .expandGroups      = m_expandGroups->isChecked(),
        .expansionAnimated = m_expansionAnimated->isChecked(),
        .expansionAnimationDuration        = m_expansionAnimationDuration->value(),
        .expansionAlternatingRows          = m_expansionAlternatingRows->isChecked(),
        .expandedArtwork                   = m_expandedArtwork->isChecked(),
        .expansionMinimumRows              = m_expansionMinimumRows->value(),
        .expansionMaximumColumns           = m_expansionMaximumColumns->value(),
        .expansionStretchColumns           = m_expansionColumnSizing->currentData().toBool(),
        .expansionMinimumColumnWidth       = m_expansionMinimumColumnWidth->value(),
        .expansionArtworkOnLeft            = m_expansionArtworkPosition->currentData().toBool(),
        .expansionArtworkSize              = m_expansionArtworkSize->value(),
        .expansionShowCloseButton          = m_expansionCloseButton->isChecked(),
        .expansionArtworkReflection        = m_expansionArtworkReflection->isChecked(),
        .expansionArtworkReflectionHeight  = m_expansionArtworkReflectionHeight->value(),
        .expansionArtworkReflectionOpacity = m_expansionArtworkReflectionOpacity->value(),
        .expansionArtworkReflectionGap     = m_expansionArtworkReflectionGap->value(),
        .expansionArtworkAutoMargins       = m_expansionArtworkAutoMargins->isChecked(),
        .expansionArtworkMargins   = {m_expansionArtworkLeftMargin->value(), m_expansionArtworkTopMargin->value(),
                                      m_expansionArtworkRightMargin->value(), m_expansionArtworkBottomMargin->value()},
        .expansionScripts          = scripts(),
        .expansionBackground       = expansionBackground,
        .expansionBackgroundColour = m_expansionBackgroundColour->isChecked()
                                       ? m_expansionBackgroundColour->colour().name(QColor::HexArgb)
                                       : QString{},
        .expansionPlayingRowFont
        = m_expansionPlayingRowFont->isChecked() ? m_expansionPlayingRowFont->buttonFont().toString() : QString{},
        .expansionPlayingRowColour = m_expansionPlayingRowColour->isChecked()
                                       ? m_expansionPlayingRowColour->colour().name(QColor::HexArgb)
                                       : QString{},
    };
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
    m_artworkLeftMargin->setValue(config.artworkMargins.left());
    m_artworkTopMargin->setValue(config.artworkMargins.top());
    m_artworkRightMargin->setValue(config.artworkMargins.right());
    m_artworkBottomMargin->setValue(config.artworkMargins.bottom());
    m_background->setConfig(config.background);
    m_expansionBackground->setConfig(config.expansionBackground);

    const QColor backgroundColour{config.expansionBackgroundColour};
    m_expansionBackgroundColour->setChecked(backgroundColour.isValid());
    m_expansionBackgroundColour->setColour(backgroundColour.isValid() ? backgroundColour
                                                                      : m_preview->defaultBackgroundColour());

    m_expansionPlayingRowFont->setChecked(!config.expansionPlayingRowFont.isEmpty());
    if(config.expansionPlayingRowFont.isEmpty()) {
        m_expansionPlayingRowFont->setButtonFont(m_preview->font());
    }
    else {
        m_expansionPlayingRowFont->setButtonFont(config.expansionPlayingRowFont);
    }

    const QColor playingRowColour{config.expansionPlayingRowColour};
    m_expansionPlayingRowColour->setChecked(playingRowColour.isValid());
    m_expansionPlayingRowColour->setColour(playingRowColour.isValid() ? playingRowColour
                                                                      : Gui::defaultPlayingRowColour());

    m_expandGroups->setChecked(config.expandGroups);
    m_expansionAnimated->setChecked(config.expansionAnimated);
    m_expansionAnimationDuration->setValue(config.expansionAnimationDuration);
    m_expansionAlternatingRows->setChecked(config.expansionAlternatingRows);
    m_expansionArtworkAutoMargins->setChecked(config.expansionArtworkAutoMargins);
    m_expansionArtworkLeftMargin->setValue(config.expansionArtworkMargins.left());
    m_expansionArtworkTopMargin->setValue(config.expansionArtworkMargins.top());
    m_expansionArtworkRightMargin->setValue(config.expansionArtworkMargins.right());
    m_expansionArtworkBottomMargin->setValue(config.expansionArtworkMargins.bottom());
    m_expansionArtworkReflection->setChecked(config.expansionArtworkReflection);
    m_expansionArtworkReflectionHeight->setValue(config.expansionArtworkReflectionHeight);
    m_expansionArtworkReflectionOpacity->setValue(config.expansionArtworkReflectionOpacity);
    m_expansionArtworkReflectionGap->setValue(config.expansionArtworkReflectionGap);
    m_expansionCloseButton->setChecked(config.expansionShowCloseButton);
    m_preview->setCloseButtonVisible(config.expansionShowCloseButton);
    m_expandedArtwork->setChecked(config.expandedArtwork);
    m_expansionArtworkPosition->setCurrentIndex(config.expansionArtworkOnLeft ? 0 : 1);
    m_expansionArtworkSize->setValue(config.expansionArtworkSize);
    m_expansionMinimumRows->setValue(config.expansionMinimumRows);
    m_expansionMaximumColumns->setValue(config.expansionMaximumColumns);
    m_expansionColumnSizing->setCurrentIndex(m_expansionColumnSizing->findData(config.expansionStretchColumns));
    m_expansionMinimumColumnWidth->setValue(config.expansionMinimumColumnWidth);
    m_headerScript->setText(config.expansionScripts.header);
    m_trackScript->setText(config.expansionScripts.track);
    m_footerScript->setText(config.expansionScripts.footer);
    m_groupByScript->setText(config.expansionScripts.groupBy);
    m_subheaderScript->setText(config.expansionScripts.subheader);

    updatePlaylistControls();
    updateExpansionControls();
    updatePreviewBackground();
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
        &FilterWidget::ConfigData::artworkCornerRadius, &FilterWidget::ConfigData::artworkMargins,
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
        m_autoFitArtwork->setEnabled(m_artworkBottomMode->isChecked() || m_artworkRightMode->isChecked()
                                     || m_artworkOverlayMode->isChecked() || m_artworkNoLabelsMode->isChecked());
    };

    updateAutoFitArtwork();
    QObject::connect(m_artworkBottomMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkRightMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkOverlayMode, &QRadioButton::toggled, this, updateAutoFitArtwork);
    QObject::connect(m_artworkNoLabelsMode, &QRadioButton::toggled, this, updateAutoFitArtwork);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
    QObject::connect(m_playlistEnabled, &QCheckBox::toggled, this, &FilterConfigDialog::updatePlaylistControls);
    QObject::connect(m_manageColumns, &QPushButton::clicked, this, [this]() {
        auto* dialog = new FilterColumnEditorDialog(m_columnRegistry, this);
        dialog->open();
    });

    for(const auto* editor : {m_headerScript, m_trackScript, m_footerScript, m_groupByScript, m_subheaderScript}) {
        QObject::connect(editor, &QPlainTextEdit::textChanged, this, &FilterConfigDialog::schedulePreview);
    }

    QObject::connect(m_expandGroups, &QCheckBox::toggled, this, &FilterConfigDialog::updateExpansionControls);
    QObject::connect(m_expansionAnimated, &QCheckBox::toggled, this, &FilterConfigDialog::updateExpansionControls);
    QObject::connect(m_expandedArtwork, &QCheckBox::toggled, this, &FilterConfigDialog::updateExpansionControls);
    QObject::connect(m_expansionMaximumColumns, &QSpinBox::valueChanged, this,
                     &FilterConfigDialog::updateExpansionControls);

    QObject::connect(m_expansionMinimumRows, &QSpinBox::valueChanged, m_preview,
                     &FilterExpansionWidget::setMinimumColumnRows);
    QObject::connect(m_expansionMaximumColumns, &QSpinBox::valueChanged, m_preview,
                     &FilterExpansionWidget::setMaximumColumns);
    QObject::connect(m_expansionColumnSizing, &QComboBox::currentIndexChanged, this,
                     [this]() { m_preview->setStretchColumns(m_expansionColumnSizing->currentData().toBool()); });
    QObject::connect(m_expansionMinimumColumnWidth, &QSpinBox::valueChanged, m_preview,
                     &FilterExpansionWidget::setMinimumColumnWidth);
    QObject::connect(m_expansionCloseButton, &QCheckBox::toggled, m_preview,
                     &FilterExpansionWidget::setCloseButtonVisible);
    QObject::connect(m_expansionAlternatingRows, &QCheckBox::toggled, m_preview,
                     &FilterExpansionWidget::setAlternatingRowColors);
    QObject::connect(m_expansionArtworkReflection, &QCheckBox::toggled, this,
                     &FilterConfigDialog::updateExpansionControls);
    QObject::connect(m_expansionArtworkReflection, &QCheckBox::toggled, this,
                     &FilterConfigDialog::updatePreviewArtworkReflection);

    for(const auto* spin :
        {m_expansionArtworkReflectionHeight, m_expansionArtworkReflectionOpacity, m_expansionArtworkReflectionGap}) {
        QObject::connect(spin, &QSpinBox::valueChanged, this, &FilterConfigDialog::updatePreviewArtworkReflection);
    }

    QObject::connect(m_expansionArtworkSize, &QSpinBox::valueChanged, m_preview,
                     &FilterExpansionWidget::setArtworkSize);
    QObject::connect(m_expansionArtworkAutoMargins, &QCheckBox::toggled, m_preview,
                     &FilterExpansionWidget::setArtworkAutoMargins);
    QObject::connect(m_expansionArtworkPosition, &QComboBox::currentIndexChanged, this,
                     [this]() { m_preview->setArtworkOnLeft(m_expansionArtworkPosition->currentData().toBool()); });

    for(const auto* margin : {m_expansionArtworkLeftMargin, m_expansionArtworkTopMargin, m_expansionArtworkRightMargin,
                              m_expansionArtworkBottomMargin}) {
        QObject::connect(margin, &QSpinBox::valueChanged, this, &FilterConfigDialog::updatePreviewArtworkMargins);
    }

    QObject::connect(m_expansionBackground, &BackgroundConfigWidget::configChanged, this,
                     &FilterConfigDialog::updatePreviewBackground);
    QObject::connect(m_expansionBackgroundColour, &ColourButton::colourUpdated, this,
                     &FilterConfigDialog::updatePreviewBackground);
    QObject::connect(m_expansionBackgroundColour, &ColourButton::toggled, this,
                     &FilterConfigDialog::updatePreviewBackground);

    QObject::connect(m_expansionPlayingRowFont, &FontButton::fontUpdated, this, &FilterConfigDialog::schedulePreview);
    QObject::connect(m_expansionPlayingRowFont, &FontButton::toggled, this, &FilterConfigDialog::schedulePreview);
    QObject::connect(m_expansionPlayingRowColour, &ColourButton::colourUpdated, this,
                     &FilterConfigDialog::schedulePreview);
    QObject::connect(m_expansionPlayingRowColour, &ColourButton::toggled, this, &FilterConfigDialog::schedulePreview);

    QObject::connect(m_preview, &FilterExpansionWidget::populated, this, &FilterConfigDialog::applyPreview);
    QObject::connect(m_filterWidget, &FilterWidget::filterUpdated, this, &FilterConfigDialog::schedulePreview);
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

void FilterConfigDialog::updateExpansionControls()
{
    const bool enabled = m_expandGroups->isChecked();
    m_expansionAnimated->setEnabled(enabled);
    m_expansionAnimationDuration->setEnabled(enabled && m_expansionAnimated->isChecked());
    m_expansionAlternatingRows->setEnabled(enabled);
    m_expansionDoubleClick->setEnabled(enabled);
    m_expansionMiddleClick->setEnabled(enabled);
    m_expansionBackground->setEnabled(enabled);
    m_expansionBackgroundColour->setEnabled(enabled);
    m_expansionPlayingRowFont->setEnabled(enabled);
    m_expansionPlayingRowColour->setEnabled(enabled);
    m_expansionCloseButton->setEnabled(enabled);
    m_expandedArtwork->setEnabled(enabled);
    m_expansionMaximumColumns->setEnabled(enabled);
    m_expansionColumnSizing->setEnabled(enabled);
    m_expansionMinimumColumnWidth->setEnabled(enabled);
    m_expansionMinimumRows->setEnabled(enabled && m_expansionMaximumColumns->value() != 1);

    const bool artworkEnabled = enabled && m_expandedArtwork->isChecked();
    m_expansionArtworkPosition->setEnabled(artworkEnabled);
    m_expansionArtworkSize->setEnabled(artworkEnabled);
    m_expansionArtworkReflection->setEnabled(artworkEnabled);

    const bool reflectionEnabled = artworkEnabled && m_expansionArtworkReflection->isChecked();
    m_expansionArtworkReflectionHeight->setEnabled(reflectionEnabled);
    m_expansionArtworkReflectionOpacity->setEnabled(reflectionEnabled);
    m_expansionArtworkReflectionGap->setEnabled(reflectionEnabled);

    m_expansionArtworkAutoMargins->setEnabled(artworkEnabled);

    for(auto* margin : {m_expansionArtworkLeftMargin, m_expansionArtworkTopMargin, m_expansionArtworkRightMargin,
                        m_expansionArtworkBottomMargin}) {
        margin->setEnabled(artworkEnabled);
    }
}

void FilterConfigDialog::updatePreviewVisibility()
{
    if(m_scriptsTab->isVisible()) {
        schedulePreview();
    }
    else {
        m_previewTimer.stop();
        ++m_previewGeneration;
        m_preview->cancel();

        if(m_previewSubscription) {
            m_previewSubscription->setDependencies({.unclassified = false});
        }
    }
}

void FilterConfigDialog::updatePreviewArtworkMargins()
{
    m_preview->setArtworkMargins({m_expansionArtworkLeftMargin->value(), m_expansionArtworkTopMargin->value(),
                                  m_expansionArtworkRightMargin->value(), m_expansionArtworkBottomMargin->value()});
}

void FilterConfigDialog::updatePreviewArtworkReflection()
{
    m_preview->setArtworkReflection({.enabled = m_expansionArtworkReflection->isChecked(),
                                     .height  = m_expansionArtworkReflectionHeight->value(),
                                     .opacity = m_expansionArtworkReflectionOpacity->value(),
                                     .gap     = m_expansionArtworkReflectionGap->value()});
}

void FilterConfigDialog::updatePreviewBackground()
{
    m_preview->setBackground(m_expansionBackground->config(),
                             m_expansionBackgroundColour->isChecked()
                                 ? m_expansionBackgroundColour->colour().name(QColor::HexArgb)
                                 : QString{});
}

void FilterConfigDialog::applyPreview(const ExpansionResult& result)
{
    if(result.generation != m_previewGeneration) {
        return;
    }

    m_headerScript->setError(result.errorSections & ExpansionHeader);
    m_trackScript->setError(result.errorSections & ExpansionTracks);
    m_footerScript->setError(result.errorSections & ExpansionFooter);
    m_groupByScript->setError(result.errorSections & ExpansionGrouping);
    m_subheaderScript->setError(result.errorSections & ExpansionSubheaders);

    if(m_previewSubscription) {
        m_previewSubscription->setDependencies(m_scriptsTab->isVisible() ? result.dependencies.combined()
                                                                         : ScriptDependencies{.unclassified = false});
    }

    if(!result.errors.empty()) {
        return;
    }

    m_preview->show();
    m_preview->setContent(result, false);

    if(const auto* context = m_filterWidget->expansionScriptContext();
       context && !context->isCurrent(result.context, result.dependencies.combined(), *result.tracks)
       && !m_previewTimer.isActive()) {
        m_previewTimer.start(150, this);
    }
}

void FilterConfigDialog::schedulePreview()
{
    ++m_previewGeneration;
    m_previewTimer.start(150, this);
}

void FilterConfigDialog::updatePreview(bool synchronous)
{
    m_previewTimer.stop();
    if(!m_previewSubscription) {
        if(auto* context = m_filterWidget->expansionScriptContext()) {
            m_previewSubscription = context->subscribe(this);
            m_previewSubscription->setDependencies({.unclassified = false});

            QObject::connect(m_previewSubscription, &ScriptSubscription::changed, this, [this]() {
                if(m_scriptsTab->isVisible() && !m_previewTimer.isActive()) {
                    m_previewTimer.start(150, this);
                }
            });
        }
    }

    auto request             = m_filterWidget->expansionPreviewRequest(scripts(), m_previewTracks);
    request.generation       = m_previewGeneration;
    request.playingRowFont   = m_expansionPlayingRowFont->isChecked()
                                 ? std::optional{m_expansionPlayingRowFont->buttonFont()}
                                 : std::nullopt;
    request.playingRowColour = m_expansionPlayingRowColour->isChecked() ? m_expansionPlayingRowColour->colour()
                                                                        : Gui::defaultPlayingRowColour();

    if(!request.tracks->empty()) {
        request.context.playback.track     = request.tracks->front();
        request.context.playback.playState = Player::PlayState::Playing;
    }

    if(synchronous) {
        FilterExpansionPopulator populator;
        QObject::connect(&populator, &FilterExpansionPopulator::ready, this, &FilterConfigDialog::applyPreview);
        populator.run(std::move(request), populator.prepareWork());
    }
    else {
        m_preview->submit(std::move(request));
    }
}

ExpansionScripts FilterConfigDialog::scripts() const
{
    return {.header    = m_headerScript->text(),
            .groupBy   = m_groupByScript->text(),
            .subheader = m_subheaderScript->text(),
            .track     = m_trackScript->text(),
            .footer    = m_footerScript->text()};
}
} // namespace Fooyin::Filters
