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

#include "lyricsconfigwidget.h"

#include "lyricscolours.h"

#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <gui/widgets/scriptlineedit.h>
#include <gui/widgets/slidereditor.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
LyricsConfigDialog::LyricsConfigDialog(LyricsWidget* lyricsWidget, QWidget* parent)
    : WidgetConfigDialog{lyricsWidget, LyricsWidget::tr("Configure %1").arg(lyricsWidget->name()), parent}
    , m_tabs{new QTabWidget(this)}
    , m_seekOnClick{new QCheckBox(tr("Seek on click"), this)}
    , m_noLyricsScript{new ScriptLineEdit(this)}
    , m_scrollDuration{new SliderEditor(tr("Synced scroll duration"), this)}
    , m_scrollManual{new QRadioButton(tr("Manual"), this)}
    , m_scrollSynced{new QRadioButton(tr("Synced"), this)}
    , m_scrollAutomatic{new QRadioButton(tr("Automatic"), this)}
    , m_showScrollbar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_alignment{new QComboBox(this)}
    , m_lineSpacing{new QSpinBox(this)}
    , m_leftMargin{new QSpinBox(this)}
    , m_topMargin{new QSpinBox(this)}
    , m_rightMargin{new QSpinBox(this)}
    , m_bottomMargin{new QSpinBox(this)}
    , m_coloursGroup{new QGroupBox(tr("Colours"), this)}
    , m_bgColour{new QCheckBox(tr("Background colour") + ":"_L1, this)}
    , m_bgColourBtn{new ColourButton(this)}
    , m_lineColour{new QCheckBox(tr("Line colour") + ":"_L1, this)}
    , m_lineColourBtn{new ColourButton(this)}
    , m_unplayedColour{new QCheckBox(tr("Unplayed line colour") + ":"_L1, this)}
    , m_unplayedColourBtn{new ColourButton(this)}
    , m_playedColour{new QCheckBox(tr("Played line colour") + ":"_L1, this)}
    , m_playedColourBtn{new ColourButton(this)}
    , m_syncedLineColour{new QCheckBox(tr("Current line colour") + ":"_L1, this)}
    , m_syncedLineColourBtn{new ColourButton(this)}
    , m_wordLineColour{new QCheckBox(tr("Current line colour") + ":"_L1, this)}
    , m_wordLineColourBtn{new ColourButton(this)}
    , m_wordColour{new QCheckBox(tr("Current word colour") + ":"_L1, this)}
    , m_wordColourBtn{new ColourButton(this)}
    , m_lineFont{new QCheckBox(tr("Current line font") + u":"_s, this)}
    , m_lineFontBtn{new FontButton(this)}
    , m_wordLineFont{new QCheckBox(tr("Current line font") + u":"_s, this)}
    , m_wordLineFontBtn{new FontButton(this)}
    , m_wordFont{new QCheckBox(tr("Current word font") + u":"_s, this)}
    , m_wordFontBtn{new FontButton(this)}
{
    auto* generalPage   = new QWidget(this);
    auto* generalGroup  = new QGroupBox(tr("General"), generalPage);
    auto* generalLayout = new QGridLayout(generalGroup);

    auto* seekHint     = new QLabel(u"🛈 "_s + tr("This will only function with synced lyrics."), generalPage);
    auto* noLyricsHint = new QLabel(
        u"🛈 "_s + tr("This will be displayed if lyrics for the current track can't be found."), generalPage);

    int row = 0;
    generalLayout->addWidget(m_seekOnClick, row++, 0, 1, 2);
    generalLayout->addWidget(seekHint, row++, 0, 1, 2);
    generalLayout->addWidget(new QLabel(tr("No lyrics script") + u":"_s, generalPage), row, 0);
    generalLayout->addWidget(m_noLyricsScript, row++, 1);
    generalLayout->addWidget(noLyricsHint, row++, 0, 1, 2);
    generalLayout->setColumnStretch(1, 1);

    auto* scrollingGroup  = new QGroupBox(tr("Scrolling"), generalPage);
    auto* scrollingLayout = new QGridLayout(scrollingGroup);

    m_scrollDuration->setRange(0, 2000);
    m_scrollDuration->setSuffix(u" ms"_s);

    auto* scrollModeGroup  = new QGroupBox(tr("Scroll Mode"), generalPage);
    auto* scrollModeLayout = new QGridLayout(scrollModeGroup);
    m_scrollManual->setToolTip(tr("No automatic scrolling will take place"));
    m_scrollSynced->setToolTip(tr("Synced lyrics will be scrolled"));
    m_scrollAutomatic->setToolTip(tr("All lyrics will be scrolled"));
    scrollModeLayout->addWidget(m_scrollManual, 0, 0);
    scrollModeLayout->addWidget(m_scrollSynced, 1, 0);
    scrollModeLayout->addWidget(m_scrollAutomatic, 2, 0);

    row = 0;
    scrollingLayout->addWidget(m_scrollDuration, row++, 0);
    scrollingLayout->addWidget(scrollModeGroup, row++, 0, 1, 2);
    scrollingLayout->setColumnStretch(2, 1);

    auto* generalPageLayout = new QVBoxLayout(generalPage);
    generalPageLayout->addWidget(generalGroup);
    generalPageLayout->addWidget(scrollingGroup);
    generalPageLayout->addStretch();

    auto* appearancePage          = new QWidget(this);
    auto* appearanceGeneralGroup  = new QGroupBox(tr("General"), appearancePage);
    auto* appearanceGeneralLayout = new QGridLayout(appearanceGeneralGroup);

    m_alignment->addItem(tr("Align to centre"), Qt::AlignCenter);
    m_alignment->addItem(tr("Align to left"), Qt::AlignLeft);
    m_alignment->addItem(tr("Align to right"), Qt::AlignRight);

    m_lineSpacing->setRange(0, 100);
    m_lineSpacing->setSuffix(u" px"_s);

    row = 0;
    appearanceGeneralLayout->addWidget(m_showScrollbar, row++, 0, 1, 2);
    appearanceGeneralLayout->addWidget(new QLabel(tr("Line spacing") + u":"_s, appearancePage), row, 0);
    appearanceGeneralLayout->addWidget(m_lineSpacing, row++, 1);
    appearanceGeneralLayout->addWidget(new QLabel(tr("Alignment") + u":"_s, appearancePage), row, 0);
    appearanceGeneralLayout->addWidget(m_alignment, row++, 1);
    appearanceGeneralLayout->setColumnStretch(2, 1);

    auto* marginsGroup  = new QGroupBox(tr("Margins"), appearancePage);
    auto* marginsLayout = new QGridLayout(marginsGroup);

    for(auto* spin : {m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin}) {
        spin->setRange(0, 100);
        spin->setSuffix(u" px"_s);
    }

    row = 0;
    marginsLayout->addWidget(new QLabel(tr("Left") + u":"_s, appearancePage), row, 0);
    marginsLayout->addWidget(m_leftMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Right") + u":"_s, appearancePage), row, 2);
    marginsLayout->addWidget(m_rightMargin, row++, 3);
    marginsLayout->addWidget(new QLabel(tr("Top") + u":"_s, appearancePage), row, 0);
    marginsLayout->addWidget(m_topMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Bottom") + u":"_s, appearancePage), row, 2);
    marginsLayout->addWidget(m_bottomMargin, row++, 3);
    marginsLayout->setColumnStretch(4, 1);

    auto* fontsGroup             = new QGroupBox(tr("Fonts"), appearancePage);
    auto* fontsGroupLayout       = new QGridLayout(fontsGroup);
    auto* syncedFontsGroup       = new QGroupBox(tr("Synced"), appearancePage);
    auto* syncedFontsLayout      = new QGridLayout(syncedFontsGroup);
    auto* syncedWordsFontsGroup  = new QGroupBox(tr("Synced Words"), appearancePage);
    auto* syncedWordsFontsLayout = new QGridLayout(syncedWordsFontsGroup);

    row = 0;
    syncedFontsLayout->addWidget(m_lineFont, row, 0);
    syncedFontsLayout->addWidget(m_lineFontBtn, row++, 1);
    syncedFontsLayout->setColumnStretch(1, 1);

    row = 0;
    syncedWordsFontsLayout->addWidget(m_wordLineFont, row, 0);
    syncedWordsFontsLayout->addWidget(m_wordLineFontBtn, row++, 1);
    syncedWordsFontsLayout->addWidget(m_wordFont, row, 0);
    syncedWordsFontsLayout->addWidget(m_wordFontBtn, row++, 1);
    syncedWordsFontsLayout->setColumnStretch(1, 1);

    row = 0;
    fontsGroupLayout->addWidget(syncedFontsGroup, row++, 0);
    fontsGroupLayout->addWidget(syncedWordsFontsGroup, row++, 0);

    auto* coloursLayout    = new QGridLayout(m_coloursGroup);
    auto* syncedLineGroup  = new QGroupBox(tr("Synced"), appearancePage);
    auto* syncedLineLayout = new QGridLayout(syncedLineGroup);
    auto* syncedWordGroup  = new QGroupBox(tr("Synced Words"), appearancePage);
    auto* syncedWordLayout = new QGridLayout(syncedWordGroup);

    row = 0;
    syncedLineLayout->addWidget(m_unplayedColour, row, 0);
    syncedLineLayout->addWidget(m_unplayedColourBtn, row++, 1);
    syncedLineLayout->addWidget(m_playedColour, row, 0);
    syncedLineLayout->addWidget(m_playedColourBtn, row++, 1);
    syncedLineLayout->addWidget(m_syncedLineColour, row, 0);
    syncedLineLayout->addWidget(m_syncedLineColourBtn, row++, 1);
    syncedLineLayout->setColumnStretch(1, 1);

    row = 0;
    syncedWordLayout->addWidget(m_wordLineColour, row, 0);
    syncedWordLayout->addWidget(m_wordLineColourBtn, row++, 1);
    syncedWordLayout->addWidget(m_wordColour, row, 0);
    syncedWordLayout->addWidget(m_wordColourBtn, row++, 1);
    syncedWordLayout->setColumnStretch(1, 1);

    row = 0;
    coloursLayout->addWidget(m_bgColour, row, 0);
    coloursLayout->addWidget(m_bgColourBtn, row++, 1);
    coloursLayout->addWidget(m_lineColour, row, 0);
    coloursLayout->addWidget(m_lineColourBtn, row++, 1);
    coloursLayout->addWidget(syncedLineGroup, row++, 0, 1, 2);
    coloursLayout->addWidget(syncedWordGroup, row++, 0, 1, 2);
    coloursLayout->setColumnStretch(1, 1);

    auto* appearancePageLayout = new QVBoxLayout(appearancePage);
    appearancePageLayout->addWidget(appearanceGeneralGroup);
    appearancePageLayout->addWidget(marginsGroup);
    appearancePageLayout->addWidget(fontsGroup);
    appearancePageLayout->addWidget(m_coloursGroup);
    appearancePageLayout->addStretch();

    m_tabs->addTab(generalPage, tr("General"));
    m_tabs->addTab(appearancePage, tr("Appearance"));

    auto* layout = contentLayout();
    layout->addWidget(m_tabs);

    QObject::connect(m_lineFont, &QCheckBox::clicked, m_lineFontBtn, &QWidget::setEnabled);
    QObject::connect(m_wordLineFont, &QCheckBox::clicked, m_wordLineFontBtn, &QWidget::setEnabled);
    QObject::connect(m_wordFont, &QCheckBox::clicked, m_wordFontBtn, &QWidget::setEnabled);

    QObject::connect(m_bgColour, &QCheckBox::clicked, m_bgColourBtn, &QWidget::setEnabled);
    QObject::connect(m_lineColour, &QCheckBox::clicked, m_lineColourBtn, &QWidget::setEnabled);
    QObject::connect(m_unplayedColour, &QCheckBox::clicked, m_unplayedColourBtn, &QWidget::setEnabled);
    QObject::connect(m_playedColour, &QCheckBox::clicked, m_playedColourBtn, &QWidget::setEnabled);
    QObject::connect(m_syncedLineColour, &QCheckBox::clicked, m_syncedLineColourBtn, &QWidget::setEnabled);
    QObject::connect(m_wordLineColour, &QCheckBox::clicked, m_wordLineColourBtn, &QWidget::setEnabled);
    QObject::connect(m_wordColour, &QCheckBox::clicked, m_wordColourBtn, &QWidget::setEnabled);

    loadCurrentConfig();
}

LyricsWidget::ConfigData LyricsConfigDialog::config() const
{
    LyricsWidget::ConfigData config{
        .seekOnClick    = m_seekOnClick->isChecked(),
        .noLyricsScript = m_noLyricsScript->text(),
        .scrollDuration = m_scrollDuration->value(),
        .scrollMode     = static_cast<int>(scrollMode()),
        .showScrollbar  = m_showScrollbar->isChecked(),
        .alignment      = m_alignment->currentData().toInt(),
        .lineSpacing    = m_lineSpacing->value(),
        .margins      = {m_leftMargin->value(), m_topMargin->value(), m_rightMargin->value(), m_bottomMargin->value()},
        .colours      = QVariant{},
        .lineFont     = m_lineFont->isChecked() ? m_lineFontBtn->buttonFont().toString() : QString{},
        .wordLineFont = m_wordLineFont->isChecked() ? m_wordLineFontBtn->buttonFont().toString() : QString{},
        .wordFont     = m_wordFont->isChecked() ? m_wordFontBtn->buttonFont().toString() : QString{},
    };

    Colours colours;

    const auto applyColour = [&colours](QCheckBox* check, ColourButton* button, Colours::Type type) {
        if(check->isChecked()) {
            colours.setColour(type, button->colour());
        }
    };

    applyColour(m_bgColour, m_bgColourBtn, Colours::Type::Background);
    applyColour(m_lineColour, m_lineColourBtn, Colours::Type::LineUnsynced);
    applyColour(m_unplayedColour, m_unplayedColourBtn, Colours::Type::LineUnplayed);
    applyColour(m_playedColour, m_playedColourBtn, Colours::Type::LinePlayed);
    applyColour(m_syncedLineColour, m_syncedLineColourBtn, Colours::Type::LineSynced);
    applyColour(m_wordLineColour, m_wordLineColourBtn, Colours::Type::WordLineSynced);
    applyColour(m_wordColour, m_wordColourBtn, Colours::Type::WordSynced);

    if(colours != Colours{}) {
        config.colours = QVariant::fromValue(colours);
    }

    return config;
}

void LyricsConfigDialog::setConfig(const LyricsWidget::ConfigData& config)
{
    m_seekOnClick->setChecked(config.seekOnClick);
    m_noLyricsScript->setText(config.noLyricsScript);
    m_scrollDuration->setValue(config.scrollDuration);

    setScrollMode(static_cast<ScrollMode>(config.scrollMode));

    m_showScrollbar->setChecked(config.showScrollbar);
    m_lineSpacing->setValue(config.lineSpacing);

    int alignIdx = m_alignment->findData(config.alignment);
    if(alignIdx < 0) {
        alignIdx = m_alignment->findData(Qt::AlignCenter);
    }
    m_alignment->setCurrentIndex(alignIdx);

    m_leftMargin->setValue(config.margins.left());
    m_topMargin->setValue(config.margins.top());
    m_rightMargin->setValue(config.margins.right());
    m_bottomMargin->setValue(config.margins.bottom());

    const Colours defaultColours;
    const Colours currentColours = config.colours.isValid() && config.colours.canConvert<Colours>()
                                     ? config.colours.value<Colours>()
                                     : Colours{};

    const auto loadColour
        = [&defaultColours, &currentColours](QCheckBox* check, ColourButton* button, Colours::Type type) {
              check->setChecked(currentColours.colour(type) != defaultColours.colour(type));
              button->setColour(currentColours.colour(type));
              button->setEnabled(check->isChecked());
          };

    loadColour(m_bgColour, m_bgColourBtn, Colours::Type::Background);
    loadColour(m_lineColour, m_lineColourBtn, Colours::Type::LineUnsynced);
    loadColour(m_unplayedColour, m_unplayedColourBtn, Colours::Type::LineUnplayed);
    loadColour(m_playedColour, m_playedColourBtn, Colours::Type::LinePlayed);
    loadColour(m_syncedLineColour, m_syncedLineColourBtn, Colours::Type::LineSynced);
    loadColour(m_wordLineColour, m_wordLineColourBtn, Colours::Type::WordLineSynced);
    loadColour(m_wordColour, m_wordColourBtn, Colours::Type::WordSynced);

    const auto loadFont = [](QCheckBox* check, FontButton* button, const QString& fontStr, const QFont& fallback) {
        QFont font{fallback};
        bool custom = false;
        if(!fontStr.isEmpty() && font.fromString(fontStr)) {
            custom = true;
        }
        check->setChecked(custom);
        button->setButtonFont(font);
        button->setEnabled(custom);
    };

    loadFont(m_lineFont, m_lineFontBtn, config.lineFont, Lyrics::defaultLineFont());
    loadFont(m_wordLineFont, m_wordLineFontBtn, config.wordLineFont, Lyrics::defaultWordLineFont());
    loadFont(m_wordFont, m_wordFontBtn, config.wordFont, Lyrics::defaultWordFont());
}

ScrollMode LyricsConfigDialog::scrollMode() const
{
    if(m_scrollManual->isChecked()) {
        return ScrollMode::Manual;
    }
    if(m_scrollSynced->isChecked()) {
        return ScrollMode::Synced;
    }
    return ScrollMode::Automatic;
}

void LyricsConfigDialog::setScrollMode(ScrollMode mode)
{
    m_scrollManual->setChecked(mode == ScrollMode::Manual);
    m_scrollSynced->setChecked(mode == ScrollMode::Synced);
    m_scrollAutomatic->setChecked(mode == ScrollMode::Automatic);
}
} // namespace Fooyin::Lyrics
