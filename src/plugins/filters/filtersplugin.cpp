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

#include "filtersplugin.h"

#include "filtermanager.h"
#include "filterwidget.h"
#include "settings/filtersettings.h"

#include <gui/layoutprovider.h>
#include <gui/search/searchcontroller.h>
#include <gui/widgetfactory.h>

#include <utils/actions/actioncontainer.h>

#include <QMenu>

namespace Fy::Filters {
void FiltersPlugin::initialise(const Core::CorePluginContext& context)
{
    m_actionManager   = context.actionManager;
    m_library         = context.library;
    m_playerManager   = context.playerManager;
    m_playlistHandler = context.playlistHandler;
    m_settings        = context.settingsManager;

    m_filterSettings = std::make_unique<Settings::FiltersSettings>(m_settings);
}

void FiltersPlugin::initialise(const Gui::GuiPluginContext& context)
{
    m_layoutProvider   = context.layoutProvider;
    m_searchController = context.searchController;
    m_factory          = context.widgetFactory;
    m_trackSelection   = context.trackSelection;

    m_filterManager = new FilterManager(m_library, m_playlistHandler, m_trackSelection, m_settings, this);

    connect(m_searchController, &Gui::Widgets::SearchController::searchChanged, m_filterManager,
            &FilterManager::searchChanged);

    m_factory->registerClass<FilterWidget>("Filter", [this]() {
        return m_filterManager->createFilter();
    });

    m_generalPage = std::make_unique<Settings::FiltersGeneralPage>(m_settings);
    m_fieldsPage  = std::make_unique<Settings::FiltersFieldsPage>(m_filterManager->fieldRegistry(), m_settings);

    registerLayouts();
}

void FiltersPlugin::shutdown()
{
    m_filterManager->shutdown();
}

void FiltersPlugin::registerLayouts()
{
    m_layoutProvider->registerLayout(
        "Stone",
        R"({"Layout":[{"SplitterVertical":{"Children":["Status","Search",{"SplitterHorizontal":{
                     "Children":[{"Filter":{"Type":"Album Artist","Sort":"AscendingOrder"}},"Playlist"],
                     "State":"AAAA/wAAAAEAAAADAAAA/wAABlEAAAAAAP////8BAAAAAQA="}},"Controls"],
                     "State":"AAAA/wAAAAEAAAAFAAAAGQAAAB4AAAO8AAAAFAAAAAAA/////
                     wEAAAACAA=="}}]})");

    m_layoutProvider->registerLayout(
        "Ember",
        R"({"Layout":[{"SplitterVertical":{"Children":[{"SplitterHorizontal":{"Children":[{"Filter":{"Type":"Genre","Sort":"AscendingOrder"}},
        {"Filter":{"Type":"Album Artist","Sort":"AscendingOrder"}},{"Filter":{"Type":"Artist","Sort":"AscendingOrder"}},
        {"Filter":{"Type":"Album","Sort":"AscendingOrder"}}],"State":"AAAA/wAAAAEAAAAFAAABAAAAAQAAAAEAAAABAAAAAQAA/////wEAAAABAA=="}},
        {"SplitterHorizontal":{"Children":["Controls","Search"],"State":"AAAA/wAAAAEAAAADAAAFfgAAAdIAAAC1AP////8BAAAAAQA="}},
        {"SplitterHorizontal":{"Children":[{"SplitterVertical":{"Children":["Artwork","Info"],
        "State":"AAAA/wAAAAEAAAADAAABzAAAAbcAAAAUAP////8BAAAAAgA="}},"Playlist"],"State":"AAAA/wAAAAEAAAADAAABdQAABdsAAAC1AP////8BAAAAAQA="}},
        "Status"],"State":"AAAA/wAAAAEAAAAFAAAA/wAAAB4AAALRAAAAGQAAAAAA/////wEAAAACAA=="}}]})");
}
} // namespace Fy::Filters
