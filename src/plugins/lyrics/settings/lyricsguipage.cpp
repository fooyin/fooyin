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

#include "lyricsguipage.h"

#include "lyricsarea.h"
#include "lyricscolours.h"
#include "lyricsconstants.h"
#include "lyricssettings.h"

#include <gui/guisettings.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>

namespace Fooyin::Lyrics {
class LyricsGuiPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LyricsGuiPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_showScrollbar;
    QComboBox* m_alignment;
    QSpinBox* m_lineSpacing;
    QSpinBox* m_leftMargin;
    QSpinBox* m_topMargin;
    QSpinBox* m_rightMargin;
    QSpinBox* m_bottomMargin;

    QCheckBox* m_bgColour;
    ColourButton* m_bgColourButton;
    QCheckBox* m_unplayedColour;
    ColourButton* m_unplayedColourButton;
    QCheckBox* m_playedColour;
    ColourButton* m_playedColourButton;
    QCheckBox* m_lineColour;
    ColourButton* m_lineColourButton;
    QCheckBox* m_wordLineColour;
    ColourButton* m_wordLineColourButton;
    QCheckBox* m_wordColour;
    ColourButton* m_wordColourButton;

    QCheckBox* m_lineFont;
    FontButton* m_lineFontButton;
    QCheckBox* m_wordLineFont;
    FontButton* m_wordLineFontButton;
    QCheckBox* m_wordFont;
    FontButton* m_wordFontButton;
};

LyricsGuiPageWidget::LyricsGuiPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_showScrollbar{new QCheckBox(tr("Show scrollbar"), this)}
    , m_alignment{new QComboBox(this)}
    , m_lineSpacing{new QSpinBox(this)}
    , m_leftMargin{new QSpinBox(this)}
    , m_topMargin{new QSpinBox(this)}
    , m_rightMargin{new QSpinBox(this)}
    , m_bottomMargin{new QSpinBox(this)}
    , m_bgColour{new QCheckBox(tr("Background colour") + u":", this)}
    , m_bgColourButton{new ColourButton(this)}
    , m_unplayedColour{new QCheckBox(tr("Line colour") + u":", this)}
    , m_unplayedColourButton{new ColourButton(this)}
    , m_playedColour{new QCheckBox(tr("Played line colour") + u":", this)}
    , m_playedColourButton{new ColourButton(this)}
    , m_lineColour{new QCheckBox(tr("Current line colour") + u":", this)}
    , m_lineColourButton{new ColourButton(this)}
    , m_wordLineColour{new QCheckBox(tr("Current line colour") + u":", this)}
    , m_wordLineColourButton{new ColourButton(this)}
    , m_wordColour{new QCheckBox(tr("Current word colour") + u":", this)}
    , m_wordColourButton{new ColourButton(this)}
    , m_lineFont{new QCheckBox(tr("Current line font") + u":", this)}
    , m_lineFontButton{new FontButton(this)}
    , m_wordLineFont{new QCheckBox(tr("Current line font") + u":", this)}
    , m_wordLineFontButton{new FontButton(this)}
    , m_wordFont{new QCheckBox(tr("Current word font") + u":", this)}
    , m_wordFontButton{new FontButton(this)}
{
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    m_alignment->addItem(tr("Align to centre"), Qt::AlignCenter);
    m_alignment->addItem(tr("Align to left"), Qt::AlignLeft);
    m_alignment->addItem(tr("Align to right"), Qt::AlignRight);

    m_lineSpacing->setRange(0, 100);
    m_lineSpacing->setSuffix(QStringLiteral(" px"));

    int row{0};
    generalLayout->addWidget(m_showScrollbar, row++, 0, 1, 2);
    generalLayout->addWidget(new QLabel(tr("Line spacing") + u":", this), row, 0);
    generalLayout->addWidget(m_lineSpacing, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Alignment") + u":", this), row, 0);
    generalLayout->addWidget(m_alignment, row++, 1);
    generalLayout->setColumnStretch(generalLayout->columnCount(), 1);

    auto* marginsGroup  = new QGroupBox(tr("Margins"), this);
    auto* marginsLayout = new QGridLayout(marginsGroup);

    m_leftMargin->setRange(0, 100);
    m_topMargin->setRange(0, 100);
    m_rightMargin->setRange(0, 100);
    m_bottomMargin->setRange(0, 100);

    m_leftMargin->setSuffix(QStringLiteral(" px"));
    m_topMargin->setSuffix(QStringLiteral(" px"));
    m_rightMargin->setSuffix(QStringLiteral(" px"));
    m_bottomMargin->setSuffix(QStringLiteral(" px"));

    row = 0;
    marginsLayout->addWidget(new QLabel(tr("Left") + u":", this), row, 0);
    marginsLayout->addWidget(m_leftMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Right") + u":", this), row, 2);
    marginsLayout->addWidget(m_rightMargin, row++, 3);
    marginsLayout->addWidget(new QLabel(tr("Top") + u":", this), row, 0);
    marginsLayout->addWidget(m_topMargin, row, 1);
    marginsLayout->addWidget(new QLabel(tr("Bottom") + u":", this), row, 2);
    marginsLayout->addWidget(m_bottomMargin, row, 3);
    marginsLayout->setColumnStretch(marginsLayout->columnCount(), 1);

    auto* fontsGroup       = new QGroupBox(tr("Fonts"), this);
    auto* fontsGroupLayout = new QGridLayout(fontsGroup);

    auto* syncedFontsGroup  = new QGroupBox(tr("Synced"), this);
    auto* syncedFontsLayout = new QGridLayout(syncedFontsGroup);

    row = 0;
    syncedFontsLayout->addWidget(m_lineFont, row, 0);
    syncedFontsLayout->addWidget(m_lineFontButton, row++, 1);
    syncedFontsLayout->setColumnStretch(1, 1);

    auto* syncedWordsFontsGroup  = new QGroupBox(tr("Synced Words"), this);
    auto* syncedWordsFontsLayout = new QGridLayout(syncedWordsFontsGroup);

    row = 0;
    syncedWordsFontsLayout->addWidget(m_wordLineFont, row, 0);
    syncedWordsFontsLayout->addWidget(m_wordLineFontButton, row++, 1);
    syncedWordsFontsLayout->addWidget(m_wordFont, row, 0);
    syncedWordsFontsLayout->addWidget(m_wordFontButton, row++, 1);
    syncedWordsFontsLayout->setColumnStretch(1, 1);

    row = 0;
    fontsGroupLayout->addWidget(syncedFontsGroup, row++, 0);
    fontsGroupLayout->addWidget(syncedWordsFontsGroup, row++, 0);

    auto* coloursGroup       = new QGroupBox(tr("Colours"), this);
    auto* coloursGroupLayout = new QGridLayout(coloursGroup);

    auto* syncedLineGroup  = new QGroupBox(tr("Synced"), this);
    auto* syncedLineLayout = new QGridLayout(syncedLineGroup);

    row = 0;
    syncedLineLayout->addWidget(m_playedColour, row, 0);
    syncedLineLayout->addWidget(m_playedColourButton, row++, 1);
    syncedLineLayout->addWidget(m_lineColour, row, 0);
    syncedLineLayout->addWidget(m_lineColourButton, row++, 1);
    syncedLineLayout->setColumnStretch(1, 1);

    auto* syncedWordGroup  = new QGroupBox(tr("Synced Words"), this);
    auto* syncedWordLayout = new QGridLayout(syncedWordGroup);

    row = 0;
    syncedWordLayout->addWidget(m_wordLineColour, row, 0);
    syncedWordLayout->addWidget(m_wordLineColourButton, row++, 1);
    syncedWordLayout->addWidget(m_wordColour, row, 0);
    syncedWordLayout->addWidget(m_wordColourButton, row++, 1);
    syncedWordLayout->setColumnStretch(1, 1);

    row = 0;
    coloursGroupLayout->addWidget(m_bgColour, row, 0);
    coloursGroupLayout->addWidget(m_bgColourButton, row++, 1);
    coloursGroupLayout->addWidget(m_unplayedColour, row, 0);
    coloursGroupLayout->addWidget(m_unplayedColourButton, row++, 1);
    coloursGroupLayout->addWidget(syncedLineGroup, row++, 0, 1, 2);
    coloursGroupLayout->addWidget(syncedWordGroup, row++, 0, 1, 2);
    coloursGroupLayout->setColumnStretch(1, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(generalGroup, row++, 0);
    layout->addWidget(marginsGroup, row++, 0);
    layout->addWidget(fontsGroup, row++, 0);
    layout->addWidget(coloursGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);

    QObject::connect(m_lineFont, &QCheckBox::clicked, m_lineFontButton, &QWidget::setEnabled);
    QObject::connect(m_wordLineFont, &QCheckBox::clicked, m_wordLineFontButton, &QWidget::setEnabled);
    QObject::connect(m_wordFont, &QCheckBox::clicked, m_wordFontButton, &QWidget::setEnabled);

    QObject::connect(m_bgColour, &QCheckBox::clicked, m_bgColourButton, &QWidget::setEnabled);
    QObject::connect(m_unplayedColour, &QCheckBox::clicked, m_unplayedColourButton, &QWidget::setEnabled);
    QObject::connect(m_playedColour, &QCheckBox::clicked, m_playedColourButton, &QWidget::setEnabled);
    QObject::connect(m_lineColour, &QCheckBox::clicked, m_lineColourButton, &QWidget::setEnabled);
    QObject::connect(m_wordLineColour, &QCheckBox::clicked, m_wordLineColourButton, &QWidget::setEnabled);
    QObject::connect(m_wordColour, &QCheckBox::clicked, m_wordColourButton, &QWidget::setEnabled);

    m_settings->subscribe<Settings::Gui::Theme>(this, &SettingsPageWidget::load);
    m_settings->subscribe<Settings::Gui::Style>(this, &SettingsPageWidget::load);
}

void LyricsGuiPageWidget::load()
{
    m_showScrollbar->setChecked(m_settings->value<Settings::Lyrics::ShowScrollbar>());
    m_lineSpacing->setValue(m_settings->value<Settings::Lyrics::LineSpacing>());
    m_alignment->setCurrentIndex(m_alignment->findData(m_settings->value<Settings::Lyrics::Alignment>()));

    const auto margins = m_settings->value<Settings::Lyrics::Margins>().value<QMargins>();
    m_leftMargin->setValue(margins.left());
    m_topMargin->setValue(margins.top());
    m_rightMargin->setValue(margins.right());
    m_bottomMargin->setValue(margins.bottom());

    const Colours defaultColours;
    const auto currentColours = m_settings->value<Settings::Lyrics::Colours>().value<Colours>();

    const auto loadColour
        = [&defaultColours, &currentColours](QCheckBox* check, ColourButton* button, Colours::Type type) {
              check->setChecked(currentColours.colour(type) != defaultColours.colour(type));
              button->setColour(currentColours.colour(type));
              button->setEnabled(check->isChecked());
          };

    loadColour(m_bgColour, m_bgColourButton, Colours::Type::Background);
    loadColour(m_unplayedColour, m_unplayedColourButton, Colours::Type::LineUnplayed);
    loadColour(m_playedColour, m_playedColourButton, Colours::Type::LinePlayed);
    loadColour(m_lineColour, m_lineColourButton, Colours::Type::LineSynced);
    loadColour(m_wordLineColour, m_wordLineColourButton, Colours::Type::WordLineSynced);
    loadColour(m_wordColour, m_wordColourButton, Colours::Type::WordSynced);

    const auto loadFont = [](QCheckBox* check, FontButton* button, const QString& fontStr, const QFont& defaultFont) {
        QFont currentFont{defaultFont};
        check->setChecked(false);
        if(!fontStr.isEmpty() && currentFont.fromString(fontStr)) {
            check->setChecked(true);
        }
        button->setButtonFont(currentFont);
        button->setEnabled(check->isChecked());
    };

    loadFont(m_lineFont, m_lineFontButton, m_settings->value<Settings::Lyrics::LineFont>(),
             LyricsArea::defaultLineFont());
    loadFont(m_wordLineFont, m_wordLineFontButton, m_settings->value<Settings::Lyrics::WordLineFont>(),
             LyricsArea::defaultWordLineFont());
    loadFont(m_wordFont, m_wordFontButton, m_settings->value<Settings::Lyrics::WordFont>(),
             LyricsArea::defaultWordFont());
}

void LyricsGuiPageWidget::apply()
{
    m_settings->set<Settings::Lyrics::ShowScrollbar>(m_showScrollbar->isChecked());
    m_settings->set<Settings::Lyrics::LineSpacing>(m_lineSpacing->value());

    const auto alignment = m_alignment->currentData();
    if(alignment.isValid()) {
        m_settings->set<Settings::Lyrics::Alignment>(alignment.toInt());
    }

    m_settings->set<Settings::Lyrics::Margins>(QVariant::fromValue(
        QMargins{m_leftMargin->value(), m_topMargin->value(), m_rightMargin->value(), m_bottomMargin->value()}));

    if(m_lineFont->isChecked()) {
        m_settings->set<Settings::Lyrics::LineFont>(m_lineFontButton->buttonFont().toString());
    }
    else {
        m_settings->reset<Settings::Lyrics::LineFont>();
    }

    if(m_wordLineFont->isChecked()) {
        m_settings->set<Settings::Lyrics::WordLineFont>(m_wordLineFontButton->buttonFont().toString());
    }
    else {
        m_settings->reset<Settings::Lyrics::WordLineFont>();
    }

    if(m_wordFont->isChecked()) {
        m_settings->set<Settings::Lyrics::WordFont>(m_wordFontButton->buttonFont().toString());
    }
    else {
        m_settings->reset<Settings::Lyrics::WordFont>();
    }

    Colours colours;

    const auto applyColour = [&colours](QCheckBox* check, ColourButton* button, Colours::Type type) {
        if(check->isChecked()) {
            colours.setColour(type, button->colour());
        }
    };

    applyColour(m_bgColour, m_bgColourButton, Colours::Type::Background);
    applyColour(m_unplayedColour, m_unplayedColourButton, Colours::Type::LineUnplayed);
    applyColour(m_playedColour, m_playedColourButton, Colours::Type::LinePlayed);
    applyColour(m_lineColour, m_lineColourButton, Colours::Type::LineSynced);
    applyColour(m_wordLineColour, m_wordLineColourButton, Colours::Type::WordLineSynced);
    applyColour(m_wordColour, m_wordColourButton, Colours::Type::WordSynced);

    if(colours != Colours{}) {
        m_settings->set<Settings::Lyrics::Colours>(QVariant::fromValue(colours));
    }
    else {
        m_settings->set<Settings::Lyrics::Colours>(QVariant{});
    }

    load();
}

void LyricsGuiPageWidget::reset()
{
    m_settings->reset<Settings::Lyrics::ShowScrollbar>();
    m_settings->reset<Settings::Lyrics::LineSpacing>();
    m_settings->reset<Settings::Lyrics::Alignment>();
    m_settings->reset<Settings::Lyrics::Margins>();
    m_settings->reset<Settings::Lyrics::Colours>();
    m_settings->reset<Settings::Lyrics::LineFont>();
    m_settings->reset<Settings::Lyrics::WordLineFont>();
    m_settings->reset<Settings::Lyrics::WordFont>();
}

LyricsGuiPage::LyricsGuiPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LyricsInterface);
    setName(tr("Appearance"));
    setCategory({tr("Lyrics")});
    setWidgetCreator([settings] { return new LyricsGuiPageWidget(settings); });
}
} // namespace Fooyin::Lyrics

#include "lyricsguipage.moc"
#include "moc_lyricsguipage.cpp"
