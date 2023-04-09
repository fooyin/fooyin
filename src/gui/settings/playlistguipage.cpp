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
#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class PlaylistGuiPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit PlaylistGuiPageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QRadioButton* m_noHeaders;
    QRadioButton* m_discSubheaders;
    QRadioButton* m_splitDiscs;
    QCheckBox* m_scrollBars;
    QCheckBox* m_header;
    QCheckBox* m_altColours;
    QCheckBox* m_simplePlaylist;
};

PlaylistGuiPageWidget::PlaylistGuiPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_noHeaders{new QRadioButton(tr("None"), this)}
    , m_discSubheaders{new QRadioButton(tr("Disc Subheaders"), this)}
    , m_splitDiscs{new QRadioButton("Split Discs", this)}
    , m_scrollBars{new QCheckBox("Show Scrollbar", this)}
    , m_header{new QCheckBox("Show Header", this)}
    , m_altColours{new QCheckBox("Alternate Row Colours", this)}
    , m_simplePlaylist{new QCheckBox("Simple Playlist", this)}
{
    if(m_settings->value<Settings::DiscHeaders>()) {
        m_discSubheaders->setChecked(true);
    }
    else if(m_settings->value<Settings::SplitDiscs>()) {
        m_splitDiscs->setChecked(true);
    }
    else {
        m_noHeaders->setChecked(true);
    }

    m_scrollBars->setChecked(m_settings->value<Settings::PlaylistScrollBar>());
    m_header->setChecked(m_settings->value<Settings::PlaylistHeader>());
    m_altColours->setChecked(m_settings->value<Settings::PlaylistAltColours>());
    m_simplePlaylist->setChecked(m_settings->value<Settings::SimplePlaylist>());

    auto* discBox       = new QGroupBox(tr("Disc Handling"), this);
    auto* discBoxLayout = new QVBoxLayout(discBox);
    discBoxLayout->addWidget(m_noHeaders);
    discBoxLayout->addWidget(m_discSubheaders);
    discBoxLayout->addWidget(m_splitDiscs);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(discBox);
    mainLayout->addWidget(m_scrollBars);
    mainLayout->addWidget(m_header);
    mainLayout->addWidget(m_altColours);
    mainLayout->addWidget(m_simplePlaylist);
    mainLayout->addStretch();
}

void PlaylistGuiPageWidget::apply()
{
    m_settings->set<Settings::DiscHeaders>(m_discSubheaders->isChecked());
    m_settings->set<Settings::SplitDiscs>(m_splitDiscs->isChecked());
    m_settings->set<Settings::PlaylistScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::PlaylistHeader>(m_header->isChecked());
    m_settings->set<Settings::PlaylistAltColours>(m_altColours->isChecked());
    m_settings->set<Settings::SimplePlaylist>(m_simplePlaylist->isChecked());
}

PlaylistGuiPage::PlaylistGuiPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistInterface);
    setName(tr("Interface"));
    setCategory("Category.Playlist");
    setCategoryName(tr("Playlist"));
    setWidgetCreator([settings] {
        return new PlaylistGuiPageWidget(settings);
    });
    setCategoryIconPath(Constants::Icons::Category::Playlist);
}
} // namespace Fy::Gui::Settings
