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

#include "playlistguipage.h"

#include "gui/guiconstants.h"
#include "gui/guisettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class PlaylistGuiPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit PlaylistGuiPageWidget(Utils::SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    Utils::SettingsManager* m_settings;

    QCheckBox* m_scrollBars;
    QCheckBox* m_header;
    QCheckBox* m_altColours;
};

PlaylistGuiPageWidget::PlaylistGuiPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_scrollBars{new QCheckBox("Show Scrollbar", this)}
    , m_header{new QCheckBox("Show Header", this)}
    , m_altColours{new QCheckBox("Alternate Row Colours", this)}
{
    m_scrollBars->setChecked(m_settings->value<Settings::PlaylistScrollBar>());
    m_header->setChecked(m_settings->value<Settings::PlaylistHeader>());
    m_altColours->setChecked(m_settings->value<Settings::PlaylistAltColours>());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_scrollBars);
    mainLayout->addWidget(m_header);
    mainLayout->addWidget(m_altColours);
    mainLayout->addStretch();
}

void PlaylistGuiPageWidget::apply()
{
    m_settings->set<Settings::PlaylistScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::PlaylistHeader>(m_header->isChecked());
    m_settings->set<Settings::PlaylistAltColours>(m_altColours->isChecked());
}

void PlaylistGuiPageWidget::reset()
{
    m_settings->reset<Settings::PlaylistScrollBar>();
    m_settings->reset<Settings::PlaylistHeader>();
    m_settings->reset<Settings::PlaylistAltColours>();
}

PlaylistGuiPage::PlaylistGuiPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistInterface);
    setName(tr("Interface"));
    setCategory({"Playlist"});
    setWidgetCreator([settings] {
        return new PlaylistGuiPageWidget(settings);
    });
}
} // namespace Fy::Gui::Settings
