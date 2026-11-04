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

#include "guitrackdisplaypage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/fontbutton.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/ratingsymbols.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

#include <array>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Settings::Gui;
using namespace Settings::Gui::Internal;

class GuiTrackDisplayPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiTrackDisplayPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateRatingPreview();

    SettingsManager* m_settings;

    ScriptLineEdit* m_titleScript;

    ScriptLineEdit* m_propertiesSidebarTrackScript;

    QSpinBox* m_starRatingSize;
    QCheckBox* m_ratingEditorOnlyOnHover;
    QCheckBox* m_useCustomRatingSymbols;
    FontButton* m_ratingSymbolFont;
    QSpinBox* m_loveHeartSize;
    QCheckBox* m_loveEditorOnlyOnHover;
    QCheckBox* m_useCustomLoveSymbols;
    QLineEdit* m_lovedSymbol;
    QLineEdit* m_unlovedSymbol;
    FontButton* m_loveSymbolFont;
    QLineEdit* m_fullStarSymbol;
    QLineEdit* m_halfStarSymbol;
    QLineEdit* m_emptyStarSymbol;
    QLabel* m_ratingPreview;
    std::array<ColourButton*, 5> m_ratingColours;
    ColourButton* m_unratedColour;
    ColourButton* m_loveColour;
    ColourButton* m_unlovedColour;
};

GuiTrackDisplayPageWidget::GuiTrackDisplayPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_titleScript{new ScriptLineEdit(this)}
    , m_propertiesSidebarTrackScript{new ScriptLineEdit(this)}
    , m_starRatingSize{new QSpinBox(this)}
    , m_ratingEditorOnlyOnHover{new QCheckBox(tr("Only show unrated stars on hovered\nor selected rows"), this)}
    , m_useCustomRatingSymbols{new QCheckBox(tr("Use custom rating symbols"), this)}
    , m_ratingSymbolFont{new FontButton(tr("Symbol font") + u":"_s, true, this)}
    , m_loveHeartSize{new QSpinBox(this)}
    , m_loveEditorOnlyOnHover{new QCheckBox(tr("Only show unloved hearts on hovered\nor selected rows"), this)}
    , m_useCustomLoveSymbols{new QCheckBox(tr("Use custom love symbols"), this)}
    , m_lovedSymbol{new QLineEdit(this)}
    , m_unlovedSymbol{new QLineEdit(this)}
    , m_loveSymbolFont{new FontButton(tr("Symbol font") + u":"_s, true, this)}
    , m_fullStarSymbol{new QLineEdit(this)}
    , m_halfStarSymbol{new QLineEdit(this)}
    , m_emptyStarSymbol{new QLineEdit(this)}
    , m_ratingPreview{new QLabel(this)}
    , m_ratingColours{
          new ColourButton(tr("1 star") + u":"_s, true, this),
          new ColourButton(tr("2 stars") + u":"_s, true, this),
          new ColourButton(tr("3 stars") + u":"_s, true, this),
          new ColourButton(tr("4 stars") + u":"_s, true, this),
          new ColourButton(tr("5 stars") + u":"_s, true, this),
      }
    , m_unratedColour{new ColourButton(tr("Unrated") + u":"_s, true, this)}
    , m_loveColour{new ColourButton(tr("Heart colour") + u":"_s, true, this)}
    , m_unlovedColour{new ColourButton(tr("Unloved heart colour") + u":"_s, true, this)}
{
    auto* trackLabelsGroup  = new QGroupBox(tr("Track Labels"), this);
    auto* trackLabelsLayout = new QGridLayout(trackLabelsGroup);

    int row = 0;
    trackLabelsLayout->addWidget(new QLabel(tr("Window title") + u":"_s, this), row, 0);
    trackLabelsLayout->addWidget(m_titleScript, row++, 1);

    m_propertiesSidebarTrackScript->setToolTip(
        tr("Controls how individual tracks are labelled in the properties sidebar."));

    trackLabelsLayout->addWidget(new QLabel(tr("Properties sidebar") + u":"_s, this), row, 0);
    trackLabelsLayout->addWidget(m_propertiesSidebarTrackScript, row++, 1);
    trackLabelsLayout->setColumnStretch(1, 1);

    m_starRatingSize->setRange(5, 30);
    m_starRatingSize->setSuffix(u" px"_s);

    m_useCustomRatingSymbols->setToolTip(tr("Use the configured full, half and empty symbols in rating editors.\n"
                                            "When disabled, rating editors use the default star shapes."));
    m_ratingSymbolFont->setToolTip(tr("Font used for custom symbols in rating editors. "
                                      "Star size controls the symbol size."));
    m_starRatingSize->setToolTip(tr("Size of each star or custom symbol in rating editors"));
    m_fullStarSymbol->setToolTip(tr("Symbol used for full stars in %rating_stars% and %rating_stars_padded%.\n"
                                    "Also used in rating editors when ‘Use custom rating symbols’ is enabled."));
    m_halfStarSymbol->setToolTip(tr("Symbol used for half stars in %rating_stars% and %rating_stars_padded%.\n"
                                    "Also used in rating editors when ‘Use custom rating symbols’ is enabled."));
    m_emptyStarSymbol->setToolTip(tr("Symbol used for empty stars in %rating_stars_padded%.\n"
                                     "Also used in rating editors when ‘Use custom rating symbols’ is enabled."));
    m_ratingPreview->setToolTip(tr("Preview of %rating_stars_padded% using the current font"));

    auto* ratingScriptsGroupBox = new QGroupBox(tr("Rating Symbols"), this);
    auto* ratingScriptsLayout   = new QGridLayout(ratingScriptsGroupBox);

    row = 0;
    ratingScriptsLayout->addWidget(new QLabel(tr("Full star symbol") + u":"_s, this), row, 0);
    ratingScriptsLayout->addWidget(m_fullStarSymbol, row++, 1);
    ratingScriptsLayout->addWidget(new QLabel(tr("Half star symbol") + u":"_s, this), row, 0);
    ratingScriptsLayout->addWidget(m_halfStarSymbol, row++, 1);
    ratingScriptsLayout->addWidget(new QLabel(tr("Empty star symbol") + u":"_s, this), row, 0);
    ratingScriptsLayout->addWidget(m_emptyStarSymbol, row++, 1);
    ratingScriptsLayout->addWidget(new QLabel(tr("Preview") + u":"_s, this), row, 0);
    ratingScriptsLayout->addWidget(m_ratingPreview, row++, 1);
    ratingScriptsLayout->setColumnStretch(2, 1);

    auto* ratingsGroupBox = new QGroupBox(tr("Rating Editor"), this);
    auto* ratingsLayout   = new QGridLayout(ratingsGroupBox);

    row = 0;
    ratingsLayout->addWidget(m_useCustomRatingSymbols, row++, 0, 1, 2);
    ratingsLayout->addWidget(m_ratingSymbolFont, row++, 0, 1, 2);
    ratingsLayout->addWidget(m_ratingEditorOnlyOnHover, row++, 0, 1, 2);
    ratingsLayout->addWidget(new QLabel(tr("Star size") + u":"_s, this), row, 0);
    ratingsLayout->addWidget(m_starRatingSize, row++, 1);
    ratingsLayout->addWidget(Gui::createSectionHeader(tr("Colours"), this), row++, 0, 1, 3);

    auto* ratingColoursLayout = new QGridLayout();

    const std::array ratingColours{m_unratedColour,       m_ratingColours.at(0), m_ratingColours.at(1),
                                   m_ratingColours.at(2), m_ratingColours.at(3), m_ratingColours.at(4)};
    for(size_t i{0}; i < ratingColours.size(); ++i) {
        const int index = static_cast<int>(i);
        ratingColoursLayout->addWidget(ratingColours[i], index % 3, index / 3);
    }
    ratingColoursLayout->setColumnStretch(0, 1);
    ratingColoursLayout->setColumnStretch(1, 1);
    ratingsLayout->addLayout(ratingColoursLayout, row++, 0, 1, 3);
    ColourButton::alignLabels({m_ratingColours.at(0), m_ratingColours.at(1), m_ratingColours.at(2),
                               m_ratingColours.at(3), m_ratingColours.at(4), m_unratedColour});
    ratingsLayout->setColumnStretch(2, 1);
    ratingsLayout->setRowStretch(row, 1);

    m_loveHeartSize->setRange(5, 30);
    m_loveHeartSize->setSuffix(u" px"_s);
    m_loveHeartSize->setToolTip(tr("Size of the heart or custom symbol in love editors"));
    m_useCustomLoveSymbols->setToolTip(tr("Use the configured loved and unloved symbols in love editors.\n"
                                          "When disabled, love editors use the default heart shape."));
    m_lovedSymbol->setToolTip(tr("Symbol shown for loved tracks when custom love symbols are enabled"));
    m_unlovedSymbol->setToolTip(tr("Symbol shown for unloved tracks when custom love symbols are enabled"));
    m_loveSymbolFont->setToolTip(tr("Font used for custom symbols in love editors. "
                                    "Heart size controls the symbol size."));

    auto* loveSymbolsGroupBox = new QGroupBox(tr("Love Symbols"), this);
    auto* loveSymbolsLayout   = new QGridLayout(loveSymbolsGroupBox);

    row = 0;
    loveSymbolsLayout->addWidget(new QLabel(tr("Loved symbol") + u":"_s, this), row, 0);
    loveSymbolsLayout->addWidget(m_lovedSymbol, row++, 1);
    loveSymbolsLayout->addWidget(new QLabel(tr("Unloved symbol") + u":"_s, this), row, 0);
    loveSymbolsLayout->addWidget(m_unlovedSymbol, row++, 1);
    loveSymbolsLayout->setColumnStretch(2, 1);
    loveSymbolsLayout->setRowStretch(2, 1);

    auto* loveGroupBox = new QGroupBox(tr("Love Editor"), this);
    auto* loveLayout   = new QGridLayout(loveGroupBox);

    row = 0;
    loveLayout->addWidget(m_useCustomLoveSymbols, row++, 0, 1, 2);
    loveLayout->addWidget(m_loveSymbolFont, row++, 0, 1, 2);
    loveLayout->addWidget(m_loveEditorOnlyOnHover, row++, 0, 1, 2);
    loveLayout->addWidget(new QLabel(tr("Heart size") + u":"_s, this), row, 0);
    loveLayout->addWidget(m_loveHeartSize, row++, 1);
    loveLayout->addWidget(Gui::createSectionHeader(tr("Colours"), this), row++, 0, 1, 2);
    loveLayout->addWidget(m_unlovedColour, row++, 0, 1, 2);
    loveLayout->addWidget(m_loveColour, row++, 0, 1, 2);
    ColourButton::alignLabels({m_loveColour, m_unlovedColour});
    loveLayout->setColumnStretch(2, 1);
    loveLayout->setRowStretch(row, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(trackLabelsGroup, row++, 0, 1, 2);
    mainLayout->addWidget(ratingScriptsGroupBox, row, 0);
    mainLayout->addWidget(loveSymbolsGroupBox, row++, 1);
    mainLayout->addWidget(ratingsGroupBox, row, 0, 1, 1);
    mainLayout->addWidget(loveGroupBox, row++, 1, 1, 1);
    mainLayout->setColumnStretch(0, 1);
    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    QObject::connect(m_useCustomRatingSymbols, &QCheckBox::toggled, m_ratingSymbolFont, &QWidget::setEnabled);
    QObject::connect(m_ratingSymbolFont, &FontButton::toggled, this, [this](bool checked) {
        if(!checked) {
            m_ratingSymbolFont->setButtonFont(font());
        }
    });

    loveSymbolsGroupBox->setEnabled(m_useCustomLoveSymbols->isChecked());
    QObject::connect(m_useCustomLoveSymbols, &QCheckBox::toggled, loveSymbolsGroupBox, &QWidget::setEnabled);
    QObject::connect(m_useCustomLoveSymbols, &QCheckBox::toggled, m_loveSymbolFont, &QWidget::setEnabled);
    QObject::connect(m_loveSymbolFont, &FontButton::toggled, this, [this](bool checked) {
        if(!checked) {
            m_loveSymbolFont->setButtonFont(font());
        }
    });

    QObject::connect(m_fullStarSymbol, &QLineEdit::textChanged, this, [this]() { updateRatingPreview(); });
    QObject::connect(m_halfStarSymbol, &QLineEdit::textChanged, this, [this]() { updateRatingPreview(); });
    QObject::connect(m_emptyStarSymbol, &QLineEdit::textChanged, this, [this]() { updateRatingPreview(); });
}

void GuiTrackDisplayPageWidget::updateRatingPreview()
{
    const auto& defaultSymbols = defaultRatingStarSymbols();
    const QString fullStarSymbol
        = m_fullStarSymbol->text().isEmpty() ? defaultSymbols.fullStarSymbol : m_fullStarSymbol->text();
    const QString halfStarSymbol
        = m_halfStarSymbol->text().isEmpty() ? defaultSymbols.halfStarSymbol : m_halfStarSymbol->text();
    const QString emptyStarSymbol
        = m_emptyStarSymbol->text().isEmpty() ? defaultSymbols.emptyStarSymbol : m_emptyStarSymbol->text();

    m_ratingPreview->setText(fullStarSymbol + fullStarSymbol + fullStarSymbol + halfStarSymbol + emptyStarSymbol);
}

void GuiTrackDisplayPageWidget::load()
{
    m_titleScript->setScript(m_settings->value<WindowTitleTrackScript>());
    m_propertiesSidebarTrackScript->setScript(m_settings->value<PropertiesSidebarTrackScript>());

    m_starRatingSize->setValue(m_settings->value<StarRatingSize>());
    m_useCustomRatingSymbols->setChecked(m_settings->value<UseCustomRatingSymbols>());
    const QVariant symbolFont = m_settings->value<RatingSymbolFont>();
    m_ratingSymbolFont->setEnabled(m_useCustomRatingSymbols->isChecked());
    m_ratingSymbolFont->setChecked(!symbolFont.isNull());
    m_ratingSymbolFont->setButtonFont(symbolFont.isNull() ? font() : symbolFont.value<QFont>());
    m_ratingEditorOnlyOnHover->setChecked(m_settings->value<RatingEditorOnlyOnHover>());
    m_useCustomLoveSymbols->setChecked(m_settings->value<UseCustomLoveSymbols>());
    m_lovedSymbol->setText(m_settings->value<LovedSymbol>());
    m_unlovedSymbol->setText(m_settings->value<UnlovedSymbol>());
    const QVariant loveFont = m_settings->value<LoveSymbolFont>();
    m_loveSymbolFont->setEnabled(m_useCustomLoveSymbols->isChecked());
    m_loveSymbolFont->setChecked(!loveFont.isNull());
    m_loveSymbolFont->setButtonFont(loveFont.isNull() ? font() : loveFont.value<QFont>());
    m_loveHeartSize->setValue(m_settings->value<LoveHeartSize>());
    m_loveEditorOnlyOnHover->setChecked(m_settings->value<LoveEditorOnlyOnHover>());
    m_fullStarSymbol->setText(m_settings->value<RatingFullStarSymbol>());
    m_halfStarSymbol->setText(m_settings->value<RatingHalfStarSymbol>());
    m_emptyStarSymbol->setText(m_settings->value<RatingEmptyStarSymbol>());

    const std::array ratingColours{
        m_settings->value<RatingOneStarColour>(),   m_settings->value<RatingTwoStarColour>(),
        m_settings->value<RatingThreeStarColour>(), m_settings->value<RatingFourStarColour>(),
        m_settings->value<RatingFiveStarColour>(),
    };
    const std::array defaultRatingColours{
        QColor{u"#d9534f"_s}, QColor{u"#f0ad4e"_s}, QColor{u"#f1c40f"_s}, QColor{u"#8bc34a"_s}, QColor{u"#43a047"_s},
    };
    for(size_t i{0}; i < m_ratingColours.size(); ++i) {
        m_ratingColours.at(i)->setChecked(!ratingColours.at(i).isNull());
        m_ratingColours.at(i)->setColour(ratingColours.at(i).isNull() ? defaultRatingColours.at(i)
                                                                      : ratingColours.at(i).value<QColor>());
    }
    const QVariant unratedColour = m_settings->value<UnratedStarColour>();
    m_unratedColour->setChecked(!unratedColour.isNull());
    m_unratedColour->setColour(unratedColour.isNull() ? QColor{u"#339e9e9e"_s} : unratedColour.value<QColor>());

    const QVariant loveColour = m_settings->value<LoveHeartColour>();
    m_loveColour->setChecked(!loveColour.isNull());
    m_loveColour->setColour(loveColour.isNull() ? QColor{u"#e53935"_s} : loveColour.value<QColor>());
    const QVariant unlovedColour = m_settings->value<UnlovedHeartColour>();
    m_unlovedColour->setChecked(!unlovedColour.isNull());
    m_unlovedColour->setColour(unlovedColour.isNull() ? QColor{u"#339e9e9e"_s} : unlovedColour.value<QColor>());

    updateRatingPreview();
}

void GuiTrackDisplayPageWidget::apply()
{
    m_settings->set<WindowTitleTrackScript>(m_titleScript->text());
    m_settings->set<PropertiesSidebarTrackScript>(m_propertiesSidebarTrackScript->text());

    m_settings->set<UseCustomRatingSymbols>(m_useCustomRatingSymbols->isChecked());
    if(m_ratingSymbolFont->isChecked()) {
        m_settings->set<RatingSymbolFont>(m_ratingSymbolFont->buttonFont());
    }
    else {
        m_settings->reset<RatingSymbolFont>();
    }
    m_settings->set<RatingEditorOnlyOnHover>(m_ratingEditorOnlyOnHover->isChecked());
    m_settings->set<LoveEditorOnlyOnHover>(m_loveEditorOnlyOnHover->isChecked());
    m_settings->set<UseCustomLoveSymbols>(m_useCustomLoveSymbols->isChecked());
    if(m_loveSymbolFont->isChecked()) {
        m_settings->set<LoveSymbolFont>(m_loveSymbolFont->buttonFont());
    }
    else {
        m_settings->reset<LoveSymbolFont>();
    }
    const auto& loveSymbols = defaultLoveSymbols();
    m_settings->set<LovedSymbol>(m_lovedSymbol->text().isEmpty() ? loveSymbols.lovedSymbol : m_lovedSymbol->text());
    m_settings->set<UnlovedSymbol>(m_unlovedSymbol->text().isEmpty() ? loveSymbols.unlovedSymbol
                                                                     : m_unlovedSymbol->text());

    const auto& defaultSymbols = defaultRatingStarSymbols();
    m_settings->set<StarRatingSize>(m_starRatingSize->value());
    m_settings->set<LoveHeartSize>(m_loveHeartSize->value());
    m_settings->set<RatingFullStarSymbol>(m_fullStarSymbol->text().isEmpty() ? defaultSymbols.fullStarSymbol
                                                                             : m_fullStarSymbol->text());
    m_settings->set<RatingHalfStarSymbol>(m_halfStarSymbol->text().isEmpty() ? defaultSymbols.halfStarSymbol
                                                                             : m_halfStarSymbol->text());
    m_settings->set<RatingEmptyStarSymbol>(m_emptyStarSymbol->text().isEmpty() ? defaultSymbols.emptyStarSymbol
                                                                               : m_emptyStarSymbol->text());

    const auto setColour = [this]<Settings::Gui::GuiSettings Setting>(const ColourButton* button) {
        if(button->isChecked()) {
            m_settings->set<Setting>(button->colour());
        }
        else {
            m_settings->reset<Setting>();
        }
    };
    setColour.template operator()<RatingOneStarColour>(m_ratingColours.at(0));
    setColour.template operator()<RatingTwoStarColour>(m_ratingColours.at(1));
    setColour.template operator()<RatingThreeStarColour>(m_ratingColours.at(2));
    setColour.template operator()<RatingFourStarColour>(m_ratingColours.at(3));
    setColour.template operator()<RatingFiveStarColour>(m_ratingColours.at(4));
    setColour.template operator()<UnratedStarColour>(m_unratedColour);
    setColour.template operator()<LoveHeartColour>(m_loveColour);
    setColour.template operator()<UnlovedHeartColour>(m_unlovedColour);
}

void GuiTrackDisplayPageWidget::reset()
{
    m_settings->reset<WindowTitleTrackScript>();
    m_settings->reset<PropertiesSidebarTrackScript>();
    m_settings->reset<UseCustomRatingSymbols>();
    m_settings->reset<RatingSymbolFont>();
    m_settings->reset<RatingEditorOnlyOnHover>();
    m_settings->reset<LoveEditorOnlyOnHover>();
    m_settings->reset<UseCustomLoveSymbols>();
    m_settings->reset<LovedSymbol>();
    m_settings->reset<UnlovedSymbol>();
    m_settings->reset<LoveSymbolFont>();
    m_settings->reset<StarRatingSize>();
    m_settings->reset<LoveHeartSize>();
    m_settings->reset<RatingFullStarSymbol>();
    m_settings->reset<RatingHalfStarSymbol>();
    m_settings->reset<RatingEmptyStarSymbol>();
    m_settings->reset<RatingOneStarColour>();
    m_settings->reset<RatingTwoStarColour>();
    m_settings->reset<RatingThreeStarColour>();
    m_settings->reset<RatingFourStarColour>();
    m_settings->reset<RatingFiveStarColour>();
    m_settings->reset<UnratedStarColour>();
    m_settings->reset<LoveHeartColour>();
    m_settings->reset<UnlovedHeartColour>();
}

GuiTrackDisplayPage::GuiTrackDisplayPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceTrackDisplay);
    setName(tr("Track Display"));
    setCategory({tr("Interface")});
    setWidgetCreator([settings] { return new GuiTrackDisplayPageWidget(settings); });
}
} // namespace Fooyin

#include "guitrackdisplaypage.moc"
#include "moc_guitrackdisplaypage.cpp"
