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

#include "librarytreepage.h"

#include "gui/guiconstants.h"
#include "gui/guisettings.h"

#include <utils/settings/settingsmanager.h>

#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class LibraryTreePageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibraryTreePageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QTextEdit* m_groupScript;
};

LibraryTreePageWidget::LibraryTreePageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_groupScript{new QTextEdit(this)}
{
    auto* groupScriptLabel  = new QLabel("Group tracks by:", this);
    auto* groupScriptLayout = new QHBoxLayout();
    groupScriptLayout->addWidget(groupScriptLabel);
    groupScriptLayout->addWidget(m_groupScript);
    m_groupScript->setPlainText(m_settings->value<Settings::TrackTreeGrouping>());

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(groupScriptLayout);
    mainLayout->addStretch();
}

void LibraryTreePageWidget::apply()
{
    m_settings->set<Settings::TrackTreeGrouping>(m_groupScript->toPlainText());
}

LibraryTreePage::LibraryTreePage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::WidgetsLibraryTree);
    setName(tr("Library Tree"));
    setCategory("Category.Widgets");
    setCategoryName(tr("Widgets"));
    setWidgetCreator([settings] {
        return new LibraryTreePageWidget(settings);
    });
    setCategoryIconPath(Constants::Icons::Category::Widgets);
}
} // namespace Fy::Gui::Settings
