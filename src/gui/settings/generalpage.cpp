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

namespace Fy::Gui::Settings {
class GeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit GeneralPageWidget(Core::SettingsManager* settings);

    void apply() override;

private:
    Core::SettingsManager* m_settings;

    QVBoxLayout* m_mainLayout;
};

GeneralPageWidget::GeneralPageWidget(Core::SettingsManager* settings)
    : m_settings{settings}
    , m_mainLayout{new QVBoxLayout(this)}
{
    Q_UNUSED(m_settings)
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

} // namespace Fy::Gui::Settings
