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

#include "playlistguipage.h"

#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

namespace Fooyin {
class PlaylistGuiPageWidget : public SettingsPageWidget
{
public:
    explicit PlaylistGuiPageWidget(SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void setValues();

    SettingsManager* m_settings;

    QCheckBox* m_scrollBars;
    QCheckBox* m_header;
    QCheckBox* m_altColours;
    QSpinBox* m_thumbnailSize;
};

PlaylistGuiPageWidget::PlaylistGuiPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_scrollBars{new QCheckBox(tr("Show Scrollbar"), this)}
    , m_header{new QCheckBox(tr("Show Header"), this)}
    , m_altColours{new QCheckBox(tr("Alternate Row Colours"), this)}
    , m_thumbnailSize{new QSpinBox(this)}
{
    auto* layout = new QGridLayout(this);

    auto* rowHeightLabel = new QLabel(tr("Thumbnail Size:"), this);

    m_thumbnailSize->setMinimum(1);
    m_thumbnailSize->setMaximum(300);
    m_thumbnailSize->setSuffix(QStringLiteral("px"));

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
    m_settings->set<Settings::Gui::Internal::PlaylistScrollBar>(m_scrollBars->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistHeader>(m_header->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistAltColours>(m_altColours->isChecked());
    m_settings->set<Settings::Gui::Internal::PlaylistThumbnailSize>(m_thumbnailSize->value());
}

void PlaylistGuiPageWidget::reset()
{
    m_settings->reset<Settings::Gui::Internal::PlaylistScrollBar>();
    m_settings->reset<Settings::Gui::Internal::PlaylistHeader>();
    m_settings->reset<Settings::Gui::Internal::PlaylistAltColours>();
    m_settings->reset<Settings::Gui::Internal::PlaylistThumbnailSize>();

    setValues();
}

void PlaylistGuiPageWidget::setValues()
{
    m_scrollBars->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistScrollBar>());
    m_header->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistHeader>());
    m_altColours->setChecked(m_settings->value<Settings::Gui::Internal::PlaylistAltColours>());
    m_thumbnailSize->setValue(m_settings->value<Settings::Gui::Internal::PlaylistThumbnailSize>());
}

PlaylistGuiPage::PlaylistGuiPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::PlaylistInterface);
    setName(tr("Interface"));
    setCategory({tr("Playlist")});
    setWidgetCreator([settings] { return new PlaylistGuiPageWidget(settings); });
}
} // namespace Fooyin
