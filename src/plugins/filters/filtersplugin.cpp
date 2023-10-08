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
#include "settings/filtersfieldspage.h"
#include "settings/filtersgeneralpage.h"

#include <gui/plugins/guiplugincontext.h>
#include <gui/layoutprovider.h>
#include <gui/searchcontroller.h>
#include <gui/widgetfactory.h>
#include <utils/actions/actioncontainer.h>

namespace Fy::Filters {
struct FiltersPlugin::Private
{
    Utils::ActionManager* actionManager;
    Utils::SettingsManager* settings;
    Core::Library::MusicLibrary* library;
    Core::Player::PlayerManager* playerManager;
    Gui::LayoutProvider* layoutProvider;
    Gui::Widgets::SearchController* searchController;
    Gui::Widgets::WidgetFactory* factory;
    Gui::TrackSelectionController* trackSelection;

    FilterManager* filterManager;
    std::unique_ptr<Settings::FiltersSettings> filterSettings;

    std::unique_ptr<Settings::FiltersGeneralPage> generalPage;
    std::unique_ptr<Settings::FiltersFieldsPage> fieldsPage;

    void registerLayouts()
    {
        layoutProvider->registerLayout(
            "Stone",
            R"({"Layout":[{"SplitterVertical":{"Children":["Status","Search",{"SplitterHorizontal":{
                         "Children":[{"Filter":{"Type":"Album Artist","Sort":"AscendingOrder"}},"Playlist"],
                         "State":"AAAA/wAAAAEAAAADAAAA/wAABlEAAAAAAP////8BAAAAAQA="}},"Controls"],
                         "State":"AAAA/wAAAAEAAAAFAAAAGQAAAB4AAAO8AAAAFAAAAAAA/////
                         wEAAAACAA=="}}]})");

        layoutProvider->registerLayout(
            "Ember",
            R"({"Layout":[{"SplitterVertical":{"Children":[{"SplitterHorizontal":{"Children":[{"Filter":{"Type":"Genre","Sort":"AscendingOrder"}},
            {"Filter":{"Type":"Album
            Artist","Sort":"AscendingOrder"}},{"Filter":{"Type":"Artist","Sort":"AscendingOrder"}},
            {"Filter":{"Type":"Album","Sort":"AscendingOrder"}}],"State":"AAAA/wAAAAEAAAAFAAABAAAAAQAAAAEAAAABAAAAAQAA/////wEAAAABAA=="}},
            {"SplitterHorizontal":{"Children":["Controls","Search"],"State":"AAAA/wAAAAEAAAADAAAFfgAAAdIAAAC1AP////8BAAAAAQA="}},
            {"SplitterHorizontal":{"Children":[{"SplitterVertical":{"Children":["Artwork","Info"],
            "State":"AAAA/wAAAAEAAAADAAABzAAAAbcAAAAUAP////8BAAAAAgA="}},"Playlist"],"State":"AAAA/wAAAAEAAAADAAABdQAABdsAAAC1AP////8BAAAAAQA="}},
            "Status"],"State":"AAAA/wAAAAEAAAAFAAAA/wAAAB4AAALRAAAAGQAAAAAA/////wEAAAACAA=="}}]})");
    }
};

FiltersPlugin::FiltersPlugin()
    : p{std::make_unique<Private>()}
{ }

FiltersPlugin::~FiltersPlugin() = default;

void FiltersPlugin::initialise(const Core::CorePluginContext& context)
{
    p->library       = context.library;
    p->playerManager = context.playerManager;
    p->settings      = context.settingsManager;

    p->filterSettings = std::make_unique<Settings::FiltersSettings>(p->settings);
}

void FiltersPlugin::initialise(const Gui::GuiPluginContext& context)
{
    p->actionManager    = context.actionManager;
    p->layoutProvider   = context.layoutProvider;
    p->searchController = context.searchController;
    p->factory          = context.widgetFactory;
    p->trackSelection   = context.trackSelection;

    p->filterManager = new FilterManager(p->library, p->trackSelection, p->settings, this);

    connect(p->searchController, &Gui::Widgets::SearchController::searchChanged, p->filterManager,
            &FilterManager::searchChanged);

    p->factory->registerClass<FilterWidget>("Filter", [this]() {
        return p->filterManager->createFilter();
    });

    p->generalPage = std::make_unique<Settings::FiltersGeneralPage>(p->settings);
    p->fieldsPage  = std::make_unique<Settings::FiltersFieldsPage>(p->filterManager->fieldRegistry(), p->settings);

    p->registerLayouts();
}

void FiltersPlugin::shutdown()
{
    p->filterManager->shutdown();
}
} // namespace Fy::Filters
