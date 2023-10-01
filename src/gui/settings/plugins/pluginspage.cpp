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

#include "pluginspage.h"

#include "gui/guiconstants.h"
#include "pluginsmodel.h"

#include <core/plugins/pluginmanager.h>

#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class PluginPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit PluginPageWidget(Plugins::PluginManager* pluginManager);

    void apply() override;
    void reset() override;

private:
    Plugins::PluginManager* m_pluginManager;

    QTableView* m_pluginList;
    PluginsModel* m_model;
};

PluginPageWidget::PluginPageWidget(Plugins::PluginManager* pluginManager)
    : m_pluginManager{pluginManager}
    , m_pluginList{new QTableView(this)}
    , m_model{new PluginsModel(m_pluginManager)}
{
    m_pluginList->setModel(m_model);

    m_pluginList->verticalHeader()->hide();
    m_pluginList->horizontalHeader()->setStretchLastSection(true);
    m_pluginList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_pluginList);
}

void PluginPageWidget::apply() { }

void PluginPageWidget::reset() { }

PluginPage::PluginPage(Utils::SettingsManager* settings, Plugins::PluginManager* pluginManager)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::Plugins);
    setName(tr("General"));
    setCategory({"Plugins"});
    setWidgetCreator([pluginManager] {
        return new PluginPageWidget(pluginManager);
    });
}
} // namespace Fy::Gui::Settings
