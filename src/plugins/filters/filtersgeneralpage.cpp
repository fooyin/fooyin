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

#include "filtersgeneralpage.h"

#include "constants.h"
#include "filtersettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QVBoxLayout>

namespace Fy::Filters::Settings {
class FiltersGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit FiltersGeneralPageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QVBoxLayout* m_mainLayout;

    QGroupBox* m_splitterBox;
    QVBoxLayout* m_splitterBoxLayout;
    QCheckBox* m_filterScrollBars;
};

FiltersGeneralPageWidget::FiltersGeneralPageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_mainLayout{new QVBoxLayout(this)}
    , m_splitterBox{new QGroupBox(tr("Filters"))}
    , m_splitterBoxLayout{new QVBoxLayout(m_splitterBox)}
    , m_filterScrollBars{new QCheckBox("Show Filter Scrollbars", this)}
{
    m_filterScrollBars->setChecked(m_settings->value<Settings::FilterScrollBar>());

    m_splitterBoxLayout->addWidget(m_filterScrollBars);
    m_mainLayout->addWidget(m_splitterBox);

    m_mainLayout->addStretch();
}

void FiltersGeneralPageWidget::apply()
{
    m_settings->set<Settings::FilterScrollBar>(m_filterScrollBars->isChecked());
}

FiltersGeneralPage::FiltersGeneralPage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersGeneral);
    setName(tr("General"));
    setCategory("Category.Filters");
    setCategoryName(tr("Filters"));
    setWidgetCreator([settings] {
        return new FiltersGeneralPageWidget(settings);
    });
    setCategoryIconPath(Constants::Icons::Category::Filters);
}
} // namespace Fy::Filters::Settings
