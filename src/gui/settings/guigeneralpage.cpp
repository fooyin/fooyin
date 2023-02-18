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

#include "guigeneralpage.h"

#include "guisettings.h"

#include "gui/guiconstants.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace Gui::Settings {
GuiGeneralPageWidget::GuiGeneralPageWidget(Core::SettingsManager* settings)
    : m_settings{settings}
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setAlignment(Qt::AlignTop);

    auto* splitterHandles = new QCheckBox("Show Splitter Handles", this);
    splitterHandles->setChecked(m_settings->value<Settings::SplitterHandles>());

    mainLayout->addWidget(splitterHandles);

    connect(splitterHandles, &QCheckBox::clicked, this, [this](bool checked) {
        m_settings->set<Settings::SplitterHandles>(checked);
    });
}

void GuiGeneralPageWidget::apply() { }

GuiGeneralPage::GuiGeneralPage(Utils::SettingsDialogController* controller, Core::SettingsManager* settings)
    : Utils::SettingsPage{controller}
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
} // namespace Gui::Settings
