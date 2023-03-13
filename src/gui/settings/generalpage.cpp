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

#include "generalpage.h"

#include "gui/guiconstants.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class GeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit GeneralPageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QVBoxLayout* m_mainLayout;
};

GeneralPageWidget::GeneralPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_mainLayout{new QVBoxLayout(this)}
{
    Q_UNUSED(m_settings)
}

void GeneralPageWidget::apply() { }

GeneralPage::GeneralPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
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

} // namespace Fy::Gui::Settings
