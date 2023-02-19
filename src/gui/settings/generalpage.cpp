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

#include "generalpage.h"

#include "gui/guiconstants.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace Gui::Settings {
struct GeneralPageWidget::Private
{
    Core::SettingsManager* settings;

    QVBoxLayout* mainLayout;

    explicit Private(Core::SettingsManager* settings)
        : settings{settings}
    { }
};

GeneralPageWidget::GeneralPageWidget(Core::SettingsManager* settings)
    : p{std::make_unique<Private>(settings)}
{
    p->mainLayout = new QVBoxLayout(this);
}

void GeneralPageWidget::apply() { }

GeneralPage::GeneralPage(Utils::SettingsDialogController* controller, Core::SettingsManager* settings)
    : Utils::SettingsPage{controller}
{
    setId(Constants::Page::GeneralCore);
    setName(tr("General"));
    setCategory("Category.General");
    setCategoryName(tr("General"));
    setWidgetCreator([settings] {
        return new GeneralPageWidget(settings);
    });
    setCategoryIconPath(Constants::Icons::Category::General);
}

} // namespace Gui::Settings
