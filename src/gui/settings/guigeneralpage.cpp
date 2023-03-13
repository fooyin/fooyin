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

#include "guigeneralpage.h"

#include "gui/guiconstants.h"
#include "gui/guisettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class GuiGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit GuiGeneralPageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QVBoxLayout* m_mainLayout;

    QGroupBox* m_splitterBox;
    QVBoxLayout* m_splitterBoxLayout;
    QCheckBox* m_splitterHandles;
};

GuiGeneralPageWidget::GuiGeneralPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_mainLayout{new QVBoxLayout(this)}
    , m_splitterBox{new QGroupBox(tr("Splitters"))}
    , m_splitterBoxLayout{new QVBoxLayout(m_splitterBox)}
    , m_splitterHandles{new QCheckBox("Show Splitter Handles", this)}
{
    m_splitterHandles->setChecked(m_settings->value<Settings::SplitterHandles>());

    m_splitterBoxLayout->addWidget(m_splitterHandles);
    m_mainLayout->addWidget(m_splitterBox);

    m_mainLayout->addStretch();
}

void GuiGeneralPageWidget::apply()
{
    m_settings->set<Settings::SplitterHandles>(m_splitterHandles->isChecked());
}

GuiGeneralPage::GuiGeneralPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::InterfaceGeneral);
    setName(tr("General"));
    setCategory("Category.Interface");
    setCategoryName(tr("Interface"));
    setWidgetCreator([settings] {
        return new GuiGeneralPageWidget(settings);
    });
    setCategoryIconPath(Constants::Icons::Category::Interface);
}
} // namespace Fy::Gui::Settings
