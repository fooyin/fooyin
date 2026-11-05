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

#include "expansionconfigwidget.h"

#include "expansion/filterexpansionwidget.h"
#include "trackgenerator.h"

#include <gui/guiutils.h>
#include <gui/widgets/backgroundconfigwidget.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <gui/widgets/hintlabel.h>
#include <gui/widgets/scriptlineedit.h>

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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

ExpansionConfigWidget::ExpansionConfigWidget(FilterWidget* filterWidget, QWidget* parent)
    : QWidget{parent}
    , m_filterWidget{filterWidget}
    , m_headerScript{new ScriptTextEdit(this)}
    , m_trackScript{new ScriptTextEdit(this)}
    , m_footerScript{new ScriptTextEdit(this)}
    , m_groupByScript{new ScriptTextEdit(this)}
    , m_subheaderScript{new ScriptTextEdit(this)}
    , m_preview{new FilterExpansionWidget(nullptr, nullptr, this)}
    , m_previewTracks{std::make_shared<const TrackList>(TrackGenerator::generateTracks({.albumCount = 1}))}
    , m_previewSubscription{nullptr}
    , m_previewGeneration{0}
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
{
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

    m_expansionBackground->setOptions({});

    auto* expansionTabs      = new TabWidget(this);
    auto* expansionBehaviour = new QWidget(expansionTabs);
    auto* expandedArtwork    = new QWidget(expansionTabs);

    expansionTabs->addTab(expansionBehaviour, tr("Behaviour"));
    expansionTabs->addTab(expandedArtwork, tr("Artwork"));
    expansionTabs->addTab(backgroundPage(m_expansionBackground, m_expansionBackgroundColour), tr("Background"));

    auto* scriptsPage   = new QWidget(this);
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

    auto* scriptsLayout = new QHBoxLayout(this);
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

    auto* previewGroup = new QGroupBox(tr("Preview"), this);

    auto* previewGroupLayout = new QVBoxLayout(previewGroup);
    previewGroupLayout->addWidget(m_preview);
    previewLayout->addWidget(previewGroup);

    setupConnections();
    setConfig(m_filterWidget->currentConfig());
    updatePreview(true);
}

void ExpansionConfigWidget::updateConfig(FilterWidget::ConfigData& config) const
{
    auto expansionBackground{m_expansionBackground->config()};
    expansionBackground.fadeDuration = 0;

    config.expandGroups                      = m_expandGroups->isChecked();
    config.expansionAnimated                 = m_expansionAnimated->isChecked();
    config.expansionAnimationDuration        = m_expansionAnimationDuration->value();
    config.expansionAlternatingRows          = m_expansionAlternatingRows->isChecked();
    config.expandedArtwork                   = m_expandedArtwork->isChecked();
    config.expansionMinimumRows              = m_expansionMinimumRows->value();
    config.expansionMaximumColumns           = m_expansionMaximumColumns->value();
    config.expansionStretchColumns           = m_expansionColumnSizing->currentData().toBool();
    config.expansionMinimumColumnWidth       = m_expansionMinimumColumnWidth->value();
    config.expansionArtworkOnLeft            = m_expansionArtworkPosition->currentData().toBool();
    config.expansionArtworkSize              = m_expansionArtworkSize->value();
    config.expansionShowCloseButton          = m_expansionCloseButton->isChecked();
    config.expansionArtworkReflection        = m_expansionArtworkReflection->isChecked();
    config.expansionArtworkReflectionHeight  = m_expansionArtworkReflectionHeight->value();
    config.expansionArtworkReflectionOpacity = m_expansionArtworkReflectionOpacity->value();
    config.expansionArtworkReflectionGap     = m_expansionArtworkReflectionGap->value();
    config.expansionArtworkAutoMargins       = m_expansionArtworkAutoMargins->isChecked();
    config.expansionArtworkMargins = {m_expansionArtworkLeftMargin->value(), m_expansionArtworkTopMargin->value(),
                                      m_expansionArtworkRightMargin->value(), m_expansionArtworkBottomMargin->value()};
    config.expansionScripts        = scripts();
    config.expansionBackground     = expansionBackground;
    config.expansionBackgroundColour = m_expansionBackgroundColour->isChecked()
                                         ? m_expansionBackgroundColour->colour().name(QColor::HexArgb)
                                         : QString{};
    config.expansionPlayingRowFont
        = m_expansionPlayingRowFont->isChecked() ? m_expansionPlayingRowFont->buttonFont().toString() : QString{};
    config.expansionPlayingRowColour = m_expansionPlayingRowColour->isChecked()
                                         ? m_expansionPlayingRowColour->colour().name(QColor::HexArgb)
                                         : QString{};
}

void ExpansionConfigWidget::setConfig(const FilterWidget::ConfigData& config)
{
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

    updateExpansionControls();
    updatePreviewBackground();
}

void ExpansionConfigWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

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

void ExpansionConfigWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();
        if(isVisible()) {
            updatePreview();
        }
        return;
    }
    QWidget::timerEvent(event);
}

void ExpansionConfigWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updatePreviewVisibility();
}

void ExpansionConfigWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    updatePreviewVisibility();
}

void ExpansionConfigWidget::setupConnections()
{
    for(const auto* editor : {m_headerScript, m_trackScript, m_footerScript, m_groupByScript, m_subheaderScript}) {
        QObject::connect(editor, &QPlainTextEdit::textChanged, this, &ExpansionConfigWidget::schedulePreview);
    }

    QObject::connect(m_expandGroups, &QCheckBox::toggled, this, &ExpansionConfigWidget::updateExpansionControls);
    QObject::connect(m_expansionAnimated, &QCheckBox::toggled, this, &ExpansionConfigWidget::updateExpansionControls);
    QObject::connect(m_expandedArtwork, &QCheckBox::toggled, this, &ExpansionConfigWidget::updateExpansionControls);
    QObject::connect(m_expansionMaximumColumns, &QSpinBox::valueChanged, this,
                     &ExpansionConfigWidget::updateExpansionControls);

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
                     &ExpansionConfigWidget::updateExpansionControls);
    QObject::connect(m_expansionArtworkReflection, &QCheckBox::toggled, this,
                     &ExpansionConfigWidget::updatePreviewArtworkReflection);

    for(const auto* spin :
        {m_expansionArtworkReflectionHeight, m_expansionArtworkReflectionOpacity, m_expansionArtworkReflectionGap}) {
        QObject::connect(spin, &QSpinBox::valueChanged, this, &ExpansionConfigWidget::updatePreviewArtworkReflection);
    }

    QObject::connect(m_expansionArtworkSize, &QSpinBox::valueChanged, m_preview,
                     &FilterExpansionWidget::setArtworkSize);
    QObject::connect(m_expansionArtworkAutoMargins, &QCheckBox::toggled, m_preview,
                     &FilterExpansionWidget::setArtworkAutoMargins);
    QObject::connect(m_expansionArtworkPosition, &QComboBox::currentIndexChanged, this,
                     [this]() { m_preview->setArtworkOnLeft(m_expansionArtworkPosition->currentData().toBool()); });

    for(const auto* margin : {m_expansionArtworkLeftMargin, m_expansionArtworkTopMargin, m_expansionArtworkRightMargin,
                              m_expansionArtworkBottomMargin}) {
        QObject::connect(margin, &QSpinBox::valueChanged, this, &ExpansionConfigWidget::updatePreviewArtworkMargins);
    }

    QObject::connect(m_expansionBackground, &BackgroundConfigWidget::configChanged, this,
                     &ExpansionConfigWidget::updatePreviewBackground);
    QObject::connect(m_expansionBackgroundColour, &ColourButton::colourUpdated, this,
                     &ExpansionConfigWidget::updatePreviewBackground);
    QObject::connect(m_expansionBackgroundColour, &ColourButton::toggled, this,
                     &ExpansionConfigWidget::updatePreviewBackground);

    QObject::connect(m_expansionPlayingRowFont, &FontButton::fontUpdated, this,
                     &ExpansionConfigWidget::schedulePreview);
    QObject::connect(m_expansionPlayingRowFont, &FontButton::toggled, this, &ExpansionConfigWidget::schedulePreview);
    QObject::connect(m_expansionPlayingRowColour, &ColourButton::colourUpdated, this,
                     &ExpansionConfigWidget::schedulePreview);
    QObject::connect(m_expansionPlayingRowColour, &ColourButton::toggled, this,
                     &ExpansionConfigWidget::schedulePreview);

    QObject::connect(m_preview, &FilterExpansionWidget::populated, this, &ExpansionConfigWidget::applyPreview);
    QObject::connect(m_filterWidget, &FilterWidget::filterUpdated, this, &ExpansionConfigWidget::schedulePreview);
    QObject::connect(m_expandGroups, &QCheckBox::toggled, this, &ExpansionConfigWidget::expansionEnabledChanged);
}

void ExpansionConfigWidget::updateExpansionControls()
{
    const bool enabled = m_expandGroups->isChecked();
    m_expansionAnimated->setEnabled(enabled);
    m_expansionAnimationDuration->setEnabled(enabled && m_expansionAnimated->isChecked());
    m_expansionAlternatingRows->setEnabled(enabled);
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

void ExpansionConfigWidget::updatePreviewVisibility()
{
    if(isVisible()) {
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

void ExpansionConfigWidget::updatePreviewArtworkMargins()
{
    m_preview->setArtworkMargins({m_expansionArtworkLeftMargin->value(), m_expansionArtworkTopMargin->value(),
                                  m_expansionArtworkRightMargin->value(), m_expansionArtworkBottomMargin->value()});
}

void ExpansionConfigWidget::updatePreviewArtworkReflection()
{
    m_preview->setArtworkReflection({.enabled = m_expansionArtworkReflection->isChecked(),
                                     .height  = m_expansionArtworkReflectionHeight->value(),
                                     .opacity = m_expansionArtworkReflectionOpacity->value(),
                                     .gap     = m_expansionArtworkReflectionGap->value()});
}

void ExpansionConfigWidget::updatePreviewBackground()
{
    m_preview->setBackground(m_expansionBackground->config(),
                             m_expansionBackgroundColour->isChecked()
                                 ? m_expansionBackgroundColour->colour().name(QColor::HexArgb)
                                 : QString{});
}

void ExpansionConfigWidget::applyPreview(const ExpansionResult& result)
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
        m_previewSubscription->setDependencies(isVisible() ? result.dependencies.combined()
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

void ExpansionConfigWidget::schedulePreview()
{
    ++m_previewGeneration;
    m_previewTimer.start(150, this);
}

void ExpansionConfigWidget::updatePreview(bool synchronous)
{
    m_previewTimer.stop();
    if(!m_previewSubscription) {
        if(auto* context = m_filterWidget->expansionScriptContext()) {
            m_previewSubscription = context->subscribe(this);
            m_previewSubscription->setDependencies({.unclassified = false});

            QObject::connect(m_previewSubscription, &ScriptSubscription::changed, this, [this]() {
                if(isVisible() && !m_previewTimer.isActive()) {
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
        QObject::connect(&populator, &FilterExpansionPopulator::ready, this, &ExpansionConfigWidget::applyPreview);
        populator.run(std::move(request), populator.prepareWork());
    }
    else {
        m_preview->submit(std::move(request));
    }
}

ExpansionScripts ExpansionConfigWidget::scripts() const
{
    return {.header    = m_headerScript->text(),
            .groupBy   = m_groupByScript->text(),
            .subheader = m_subheaderScript->text(),
            .track     = m_trackScript->text(),
            .footer    = m_footerScript->text()};
}
} // namespace Fooyin::Filters
