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

struct PlaylistGuiPageWidget::Private
{
    Core::SettingsManager* settings;

    QVBoxLayout* mainLayout;

    QCheckBox* groupHeaders;
    QCheckBox* splitDiscs;
    QCheckBox* simpleList;
    QCheckBox* altColours;

    explicit Private(Core::SettingsManager* settings)
        : settings{settings}
    { }
};

PlaylistGuiPageWidget::PlaylistGuiPageWidget(Core::SettingsManager* settings)
    : p{std::make_unique<Private>(settings)}
{
    p->groupHeaders = new QCheckBox("Enable Disc Headers", this);
    p->groupHeaders->setChecked(p->settings->value<Settings::DiscHeaders>());

    p->splitDiscs = new QCheckBox("Split Discs", this);
    p->splitDiscs->setChecked(p->settings->value<Settings::SplitDiscs>());
    p->splitDiscs->setEnabled(p->groupHeaders->isChecked());

    p->simpleList = new QCheckBox("Simple Playlist", this);
    p->simpleList->setChecked(p->settings->value<Settings::SimplePlaylist>());

    p->altColours = new QCheckBox("Alternate Row Colours", this);
    p->altColours->setChecked(p->settings->value<Settings::PlaylistAltColours>());

    p->mainLayout = new QVBoxLayout(this);
    p->mainLayout->addWidget(p->groupHeaders);

    auto* indentWidget = Utils::Widgets::indentWidget(p->splitDiscs, this);
    p->mainLayout->addWidget(indentWidget);
    p->mainLayout->addWidget(p->simpleList);
    p->mainLayout->addWidget(p->altColours);
    p->mainLayout->addStretch();

    connect(p->groupHeaders, &QCheckBox::clicked, this, [this](bool checked) {
        if(checked) {
            p->splitDiscs->setEnabled(checked);
        }
        else {
            p->splitDiscs->setChecked(checked);
            p->splitDiscs->setEnabled(checked);
        }
    });
}

void PlaylistGuiPageWidget::apply()
{
    p->settings->set<Settings::DiscHeaders>(p->groupHeaders->isChecked());
    p->settings->set<Settings::SplitDiscs>(p->splitDiscs->isChecked());
    p->settings->set<Settings::SimplePlaylist>(p->simpleList->isChecked());
    p->settings->set<Settings::PlaylistAltColours>(p->altColours->isChecked());
}

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
