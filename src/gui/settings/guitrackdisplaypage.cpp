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

#include <core/ratingsymbols.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QSpinBox>

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

    QRadioButton* m_preferPlaying;
    QRadioButton* m_preferSelection;

    QSpinBox* m_starRatingSize;
    QSpinBox* m_loveHeartSize;
    QLineEdit* m_fullStarSymbol;
    QLineEdit* m_halfStarSymbol;
    QLineEdit* m_emptyStarSymbol;
    QLabel* m_ratingPreview;
};

GuiTrackDisplayPageWidget::GuiTrackDisplayPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_titleScript{new ScriptLineEdit(this)}
    , m_propertiesSidebarTrackScript{new ScriptLineEdit(this)}
    , m_preferPlaying{new QRadioButton(tr("Prefer currently playing track"), this)}
    , m_preferSelection{new QRadioButton(tr("Prefer current selection"), this)}
    , m_starRatingSize{new QSpinBox(this)}
    , m_loveHeartSize{new QSpinBox(this)}
    , m_fullStarSymbol{new QLineEdit(this)}
    , m_halfStarSymbol{new QLineEdit(this)}
    , m_emptyStarSymbol{new QLineEdit(this)}
    , m_ratingPreview{new QLabel(this)}
{
    auto* nowPlayingGroup       = new QGroupBox(tr("Now Playing"), this);
    auto* nowPlayingGroupLayout = new QGridLayout(nowPlayingGroup);

    int row = 0;
    nowPlayingGroupLayout->addWidget(new QLabel(tr("Window title") + u":"_s, this), row, 0);
    nowPlayingGroupLayout->addWidget(m_titleScript, row++, 1, 1, 2);
    nowPlayingGroupLayout->setColumnStretch(2, 1);

    auto* propertiesDialogGroup       = new QGroupBox(tr("Properties Dialog"), this);
    auto* propertiesDialogGroupLayout = new QGridLayout(propertiesDialogGroup);

    m_propertiesSidebarTrackScript->setToolTip(
        tr("Controls how individual tracks are labelled in the properties sidebar."));

    row = 0;
    propertiesDialogGroupLayout->addWidget(new QLabel(tr("Sidebar track display") + u":"_s, this), row, 0);
    propertiesDialogGroupLayout->addWidget(m_propertiesSidebarTrackScript, row++, 1);
    propertiesDialogGroupLayout->setColumnStretch(1, 1);

    m_starRatingSize->setRange(5, 30);
    m_starRatingSize->setSuffix(u" px"_s);

    m_starRatingSize->setToolTip(tr("Controls the star size used by the rating editor widget"));
    m_fullStarSymbol->setToolTip(tr("Used for the filled portion of %rating_stars% and %rating_stars_padded%"));
    m_halfStarSymbol->setToolTip(tr(
        "Used for the half-step portion of %rating_stars% and %rating_stars_padded%.\n"
        "If the default half-star does not render correctly with your system font, choose a different symbol here."));
    m_emptyStarSymbol->setToolTip(tr("Used for the trailing empty portion of %rating_stars_padded%"));
    m_ratingPreview->setToolTip(tr("Preview of %rating_stars_padded% using the current font."));

    auto* ratingsGroupBox = new QGroupBox(tr("Ratings"), this);
    auto* ratingsLayout   = new QGridLayout(ratingsGroupBox);

    row = 0;
    ratingsLayout->addWidget(new QLabel(tr("Rating editor star size") + u":"_s, this), row, 0);
    ratingsLayout->addWidget(m_starRatingSize, row++, 1);
    ratingsLayout->addWidget(new QLabel(tr("Full star symbol") + u":"_s, this), row, 0);
    ratingsLayout->addWidget(m_fullStarSymbol, row++, 1);
    ratingsLayout->addWidget(new QLabel(tr("Half star symbol") + u":"_s, this), row, 0);
    ratingsLayout->addWidget(m_halfStarSymbol, row++, 1);
    ratingsLayout->addWidget(new QLabel(tr("Empty star symbol") + u":"_s, this), row, 0);
    ratingsLayout->addWidget(m_emptyStarSymbol, row++, 1);
    ratingsLayout->addWidget(new QLabel(tr("Preview") + u":"_s, this), row, 0);
    ratingsLayout->addWidget(m_ratingPreview, row++, 1);
    ratingsLayout->setColumnStretch(3, 1);

    m_loveHeartSize->setRange(5, 30);
    m_loveHeartSize->setSuffix(u" px"_s);
    m_loveHeartSize->setToolTip(tr("Controls the heart size used by the love editor widget"));

    auto* loveGroupBox = new QGroupBox(tr("Love"), this);
    auto* loveLayout   = new QGridLayout(loveGroupBox);

    loveLayout->addWidget(new QLabel(tr("Love editor heart size") + u":"_s, this), 0, 0);
    loveLayout->addWidget(m_loveHeartSize, 0, 1);
    loveLayout->setColumnStretch(2, 1);

    auto* selectionGroupBox    = new QGroupBox(tr("Selection Display"), this);
    auto* selectionGroup       = new QButtonGroup(this);
    auto* selectionGroupLayout = new QGridLayout(selectionGroupBox);

    selectionGroup->addButton(m_preferPlaying);
    selectionGroup->addButton(m_preferSelection);

    row = 0;
    selectionGroupLayout->addWidget(new QLabel(tr("Selection info") + u":"_s, this), row++, 0, 1, 3);
    selectionGroupLayout->addWidget(m_preferPlaying, row++, 0, 1, 3);
    selectionGroupLayout->addWidget(m_preferSelection, row++, 0, 1, 3);
    selectionGroupLayout->setColumnStretch(2, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(nowPlayingGroup, row++, 0, 1, 2);
    mainLayout->addWidget(propertiesDialogGroup, row++, 0, 1, 2);
    mainLayout->addWidget(selectionGroupBox, row++, 0, 1, 2);
    mainLayout->addWidget(ratingsGroupBox, row++, 0, 1, 2);
    mainLayout->addWidget(loveGroupBox, row++, 0, 1, 2);
    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

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
    m_titleScript->setText(m_settings->value<WindowTitleTrackScript>());
    m_propertiesSidebarTrackScript->setText(m_settings->value<PropertiesSidebarTrackScript>());

    const auto option = static_cast<SelectionDisplay>(m_settings->value<InfoDisplayPrefer>());
    if(option == SelectionDisplay::PreferPlaying) {
        m_preferPlaying->setChecked(true);
    }
    else {
        m_preferSelection->setChecked(true);
    }

    m_starRatingSize->setValue(m_settings->value<StarRatingSize>());
    m_loveHeartSize->setValue(m_settings->value<LoveHeartSize>());
    m_fullStarSymbol->setText(m_settings->value<RatingFullStarSymbol>());
    m_halfStarSymbol->setText(m_settings->value<RatingHalfStarSymbol>());
    m_emptyStarSymbol->setText(m_settings->value<RatingEmptyStarSymbol>());
    updateRatingPreview();
}

void GuiTrackDisplayPageWidget::apply()
{
    m_settings->set<WindowTitleTrackScript>(m_titleScript->text());
    m_settings->set<PropertiesSidebarTrackScript>(m_propertiesSidebarTrackScript->text());

    const SelectionDisplay option
        = m_preferPlaying->isChecked() ? SelectionDisplay::PreferPlaying : SelectionDisplay::PreferSelection;
    m_settings->set<InfoDisplayPrefer>(static_cast<int>(option));

    const auto& defaultSymbols = defaultRatingStarSymbols();
    m_settings->set<StarRatingSize>(m_starRatingSize->value());
    m_settings->set<LoveHeartSize>(m_loveHeartSize->value());
    m_settings->set<RatingFullStarSymbol>(m_fullStarSymbol->text().isEmpty() ? defaultSymbols.fullStarSymbol
                                                                             : m_fullStarSymbol->text());
    m_settings->set<RatingHalfStarSymbol>(m_halfStarSymbol->text().isEmpty() ? defaultSymbols.halfStarSymbol
                                                                             : m_halfStarSymbol->text());
    m_settings->set<RatingEmptyStarSymbol>(m_emptyStarSymbol->text().isEmpty() ? defaultSymbols.emptyStarSymbol
                                                                               : m_emptyStarSymbol->text());
}

void GuiTrackDisplayPageWidget::reset()
{
    m_settings->reset<WindowTitleTrackScript>();
    m_settings->reset<PropertiesSidebarTrackScript>();
    m_settings->reset<InfoDisplayPrefer>();
    m_settings->reset<StarRatingSize>();
    m_settings->reset<LoveHeartSize>();
    m_settings->reset<RatingFullStarSymbol>();
    m_settings->reset<RatingHalfStarSymbol>();
    m_settings->reset<RatingEmptyStarSymbol>();
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
