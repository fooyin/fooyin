/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>

namespace Fy::Gui::Settings {
class LibraryTreeGuiPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibraryTreeGuiPageWidget(Utils::SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void setup();

    Utils::SettingsManager* m_settings;

    QCheckBox* m_showHeader;
    QCheckBox* m_showScrollbar;
    QCheckBox* m_altColours;
};

LibraryTreeGuiPageWidget::LibraryTreeGuiPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_showHeader{new QCheckBox(tr("Show Header"), this)}
    , m_showScrollbar{new QCheckBox(tr("Show Scrollbar"), this)}
    , m_altColours{new QCheckBox(tr("Alternating Row Colours"), this)}
{
    auto* layout = new QGridLayout(this);
    layout->addWidget(m_showHeader, 0, 0);
    layout->addWidget(m_showScrollbar, 1, 0);
    layout->addWidget(m_altColours, 2, 0);
    layout->setRowStretch(3, 1);

    setup();
}

void LibraryTreeGuiPageWidget::apply()
{
    m_settings->set<Settings::LibraryTreeHeader>(m_showHeader->isChecked());
    m_settings->set<Settings::LibraryTreeScrollBar>(m_showScrollbar->isChecked());
    m_settings->set<Settings::LibraryTreeAltColours>(m_altColours->isChecked());
}

void LibraryTreeGuiPageWidget::reset()
{
    m_settings->reset<Settings::LibraryTreeHeader>();
    m_settings->reset<Settings::LibraryTreeScrollBar>();
    m_settings->reset<Settings::LibraryTreeAltColours>();

    setup();
}

void LibraryTreeGuiPageWidget::setup()
{
    m_showHeader->setChecked(m_settings->value<Settings::LibraryTreeHeader>());
    m_showScrollbar->setChecked(m_settings->value<Settings::LibraryTreeScrollBar>());
    m_altColours->setChecked(m_settings->value<Settings::LibraryTreeAltColours>());
}

LibraryTreeGuiPage::LibraryTreeGuiPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibraryTreeAppearance);
    setName(tr("Appearance"));
    setCategory({"Widgets", "Library Tree"});
    setWidgetCreator([settings] {
        return new LibraryTreeGuiPageWidget(settings);
    });
}
} // namespace Fy::Gui::Settings
