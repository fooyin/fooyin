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

#include <utils/utils.h>

#include <QCheckBox>
#include <QVBoxLayout>

namespace Gui::Settings {
PlaylistGuiPageWidget::PlaylistGuiPageWidget(Core::SettingsManager* settings)
    : m_settings{settings}
{
    auto* groupHeaders = new QCheckBox("Enable Disc Headers", this);
    groupHeaders->setChecked(m_settings->value<Settings::DiscHeaders>());

    auto* splitDiscs = new QCheckBox("Split Discs", this);
    splitDiscs->setChecked(m_settings->value<Settings::SplitDiscs>());
    splitDiscs->setEnabled(groupHeaders->isChecked());

    auto* simpleList = new QCheckBox("Simple Playlist", this);
    simpleList->setChecked(m_settings->value<Settings::SimplePlaylist>());

    auto* altColours = new QCheckBox("Alternate Row Colours", this);
    altColours->setChecked(m_settings->value<Settings::PlaylistAltColours>());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(groupHeaders);

    auto* indentWidget = Utils::Widgets::indentWidget(splitDiscs, this);
    mainLayout->addWidget(indentWidget);
    mainLayout->addWidget(simpleList);
    mainLayout->addWidget(altColours);
    mainLayout->addStretch();

    connect(groupHeaders, &QCheckBox::clicked, this, [this, splitDiscs](bool checked) {
        m_settings->set<Settings::DiscHeaders>(checked);
        if(checked) {
            splitDiscs->setEnabled(checked);
        }
        else {
            splitDiscs->setChecked(checked);
            splitDiscs->setEnabled(checked);
        }
    });
    connect(splitDiscs, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::SplitDiscs>(checked);
    });
    connect(simpleList, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::SimplePlaylist>(checked);
    });
    connect(altColours, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::PlaylistAltColours>(checked);
    });
}

void PlaylistGuiPageWidget::apply() { }

PlaylistGuiPage::PlaylistGuiPage(Utils::SettingsDialogController* controller, Core::SettingsManager* settings)
    : Utils::SettingsPage{controller}
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
} // namespace Gui::Settings
