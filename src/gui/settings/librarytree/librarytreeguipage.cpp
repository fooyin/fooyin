/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "librarytreeguipage.h"

#include "internalguisettings.h"
#include "librarytree/librarytreeappearance.h"
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

namespace Fooyin {
class LibraryTreeGuiPageWidget : public SettingsPageWidget
{
public:
    explicit LibraryTreeGuiPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void setup();

    SettingsManager* m_settings;

    QCheckBox* m_showHeader;
    QCheckBox* m_showScrollbar;
    QCheckBox* m_altColours;

    QFont m_font;
    QColor m_colour;

    bool m_fontChanged{false};
    bool m_colourChanged{false};

    QPushButton* m_fontButton;
    QPushButton* m_colourButton;
    QSpinBox* m_rowHeight;
};

LibraryTreeGuiPageWidget::LibraryTreeGuiPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_showHeader{new QCheckBox(tr("Show Header"), this)}
    , m_showScrollbar{new QCheckBox(tr("Show Scrollbar"), this)}
    , m_altColours{new QCheckBox(tr("Alternating Row Colours"), this)}
    , m_fontButton{new QPushButton(QIcon::fromTheme(Constants::Icons::Font), tr("Font"), this)}
    , m_colourButton{new QPushButton(this)}
    , m_rowHeight{new QSpinBox(this)}
{
    auto* layout = new QGridLayout(this);

    auto* rowHeightLabel = new QLabel(tr("Row Height:"), this);

    layout->addWidget(m_showHeader, 0, 0, 1, 2);
    layout->addWidget(m_showScrollbar, 1, 0, 1, 2);
    layout->addWidget(m_altColours, 2, 0, 1, 2);
    layout->addWidget(rowHeightLabel, 3, 0);
    layout->addWidget(m_rowHeight, 3, 1);
    layout->addWidget(m_fontButton, 4, 0);
    layout->addWidget(m_colourButton, 4, 1);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(5, 1);

    QObject::connect(m_fontButton, &QPushButton::pressed, this, [this]() {
        bool ok;
        const QFont chosenFont = QFontDialog::getFont(&ok, m_font, this, tr("Select Font"));
        if(ok && chosenFont != m_font) {
            m_fontChanged = true;
            m_font        = chosenFont;
        }
    });

    QObject::connect(m_colourButton, &QPushButton::pressed, this, [this]() {
        const QColor chosenColour
            = QColorDialog::getColor(m_colour, this, tr("Select Colour"), QColorDialog::ShowAlphaChannel);
        if(chosenColour.isValid() && chosenColour != m_colour) {
            m_colourChanged = true;
            m_colour        = chosenColour;
        }
    });
}

void LibraryTreeGuiPageWidget::load()
{
    m_showHeader->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeHeader>());
    m_showScrollbar->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeScrollBar>());
    m_altColours->setChecked(m_settings->value<Settings::Gui::Internal::LibTreeAltColours>());

    const auto options = m_settings->value<Settings::Gui::Internal::LibTreeAppearance>().value<LibraryTreeAppearance>();
    m_fontChanged      = options.fontChanged;
    m_font             = options.font;
    m_colourChanged    = options.colourChanged;
    m_colour           = options.colour;
    m_rowHeight->setValue(options.rowHeight);

    m_fontButton->setText(QString{"%1 (%2)"}.arg(m_font.family()).arg(m_font.pointSize()));

    QPixmap px(20, 20);
    px.fill(m_colour);
    m_colourButton->setIcon(px);
    m_colourButton->setText(m_colour.name());
}

void LibraryTreeGuiPageWidget::apply()
{
    m_settings->set<Settings::Gui::Internal::LibTreeHeader>(m_showHeader->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeScrollBar>(m_showScrollbar->isChecked());
    m_settings->set<Settings::Gui::Internal::LibTreeAltColours>(m_altColours->isChecked());

    LibraryTreeAppearance options;
    options.fontChanged   = m_fontChanged;
    options.font          = m_font;
    options.colourChanged = m_colourChanged;
    options.colour        = m_colour;
    options.rowHeight     = m_rowHeight->value();
    m_settings->set<Settings::Gui::Internal::LibTreeAppearance>(QVariant::fromValue(options));
}

void LibraryTreeGuiPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::LibTreeHeader>();
    m_settings->reset<Settings::Gui::Internal::LibTreeScrollBar>();
    m_settings->reset<Settings::Gui::Internal::LibTreeAltColours>();
    m_settings->reset<Settings::Gui::Internal::LibTreeAppearance>();
}

LibraryTreeGuiPage::LibraryTreeGuiPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibraryTreeAppearance);
    setName(tr("Appearance"));
    setCategory({tr("Widgets"), tr("Library Tree")});
    setWidgetCreator([settings] { return new LibraryTreeGuiPageWidget(settings); });
}
} // namespace Fooyin
