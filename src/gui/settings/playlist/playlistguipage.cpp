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
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

namespace Fy::Gui::Settings {
class PlaylistGuiPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit PlaylistGuiPageWidget(Utils::SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void setValues();

    Utils::SettingsManager* m_settings;

    QCheckBox* m_scrollBars;
    QCheckBox* m_header;
    QCheckBox* m_altColours;
    QSpinBox* m_thumbnailSize;
};

PlaylistGuiPageWidget::PlaylistGuiPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_scrollBars{new QCheckBox("Show Scrollbar", this)}
    , m_header{new QCheckBox("Show Header", this)}
    , m_altColours{new QCheckBox("Alternate Row Colours", this)}
    , m_thumbnailSize{new QSpinBox(this)}
{
    auto* layout = new QGridLayout(this);

    auto* rowHeightLabel = new QLabel("Thumbnail Size:", this);

    m_thumbnailSize->setMinimum(1);
    m_thumbnailSize->setMaximum(300);
    m_thumbnailSize->setSuffix("px");

    layout->addWidget(m_scrollBars, 0, 0, 1, 2);
    layout->addWidget(m_header, 1, 0, 1, 2);
    layout->addWidget(m_altColours, 2, 0, 1, 2);
    layout->addWidget(rowHeightLabel, 3, 0);
    layout->addWidget(m_thumbnailSize, 3, 1);
    layout->setColumnStretch(2, 1);
    layout->setRowStretch(5, 1);

    setValues();
}

void PlaylistGuiPageWidget::apply()
{
    m_settings->set<Settings::PlaylistScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::PlaylistHeader>(m_header->isChecked());
    m_settings->set<Settings::PlaylistAltColours>(m_altColours->isChecked());
    m_settings->set<Settings::PlaylistThumbnailSize>(m_thumbnailSize->value());
}

void PlaylistGuiPageWidget::reset()
{
    m_settings->reset<Settings::PlaylistScrollBar>();
    m_settings->reset<Settings::PlaylistHeader>();
    m_settings->reset<Settings::PlaylistAltColours>();
    m_settings->reset<Settings::PlaylistThumbnailSize>();

    setValues();
}

void PlaylistGuiPageWidget::setValues()
{
    m_scrollBars->setChecked(m_settings->value<Settings::PlaylistScrollBar>());
    m_header->setChecked(m_settings->value<Settings::PlaylistHeader>());
    m_altColours->setChecked(m_settings->value<Settings::PlaylistAltColours>());
    m_thumbnailSize->setValue(m_settings->value<Settings::PlaylistThumbnailSize>());
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
