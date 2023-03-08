/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

    QVBoxLayout* m_mainLayout;

    QGroupBox* m_discBox;
    QVBoxLayout* m_discBoxLayout;

    QRadioButton* m_noHeaders;
    QRadioButton* m_discSubheaders;
    QRadioButton* m_splitDiscs;
    QCheckBox* m_simpleList;
    QCheckBox* m_altColours;
};

PlaylistGuiPageWidget::PlaylistGuiPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_mainLayout{new QVBoxLayout(this)}
    , m_discBox{new QGroupBox(tr("Disc Handling"), this)}
    , m_discBoxLayout{new QVBoxLayout(m_discBox)}
    , m_noHeaders{new QRadioButton(tr("None"), this)}
    , m_discSubheaders{new QRadioButton(tr("Disc Subheaders"), this)}
    , m_splitDiscs{new QRadioButton("Split Discs", this)}
    , m_simpleList{new QCheckBox("Simple Playlist", this)}
    , m_altColours{new QCheckBox("Alternate Row Colours", this)}
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

    m_simpleList->setChecked(m_settings->value<Settings::SimplePlaylist>());
    m_altColours->setChecked(m_settings->value<Settings::PlaylistAltColours>());

    m_discBoxLayout->addWidget(m_noHeaders);
    m_discBoxLayout->addWidget(m_discSubheaders);
    m_discBoxLayout->addWidget(m_splitDiscs);

    m_mainLayout->addWidget(m_discBox);
    m_mainLayout->addWidget(m_simpleList);
    m_mainLayout->addWidget(m_altColours);
    m_mainLayout->addStretch();
}

void PlaylistGuiPageWidget::apply()
{
    m_settings->set<Settings::DiscHeaders>(m_discSubheaders->isChecked());
    m_settings->set<Settings::SplitDiscs>(m_splitDiscs->isChecked());
    m_settings->set<Settings::SimplePlaylist>(m_simpleList->isChecked());
    m_settings->set<Settings::PlaylistAltColours>(m_altColours->isChecked());
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
