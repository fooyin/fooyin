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

#include "filtercontroller.h"
#include "filterwidget.h"
#include "settings/filterscolumnpage.h"
#include "settings/filtersettings.h"
#include "settings/filtersgeneralpage.h"

#include <gui/editablelayout.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/searchcontroller.h>
#include <gui/widgetfactory.h>
#include <utils/actions/actioncontainer.h>

namespace Fooyin::Filters {
struct FiltersPlugin::Private
{
    ActionManager* actionManager;
    SettingsManager* settings;
    MusicLibrary* library;
    PlayerManager* playerManager;
    LayoutProvider* layoutProvider;
    WidgetFactory* factory;
    TrackSelectionController* trackSelection;

    FilterController* filterController;
    std::unique_ptr<FiltersSettings> filterSettings;

    std::unique_ptr<FiltersGeneralPage> generalPage;
    std::unique_ptr<FiltersColumnPage> columnsPage;

    void registerLayouts() const
    {
        layoutProvider->registerLayout(
            R"({"Obsidian":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAGQAAA8EAAAAUAP////8BAAAAAgA=",
            "Widgets":[{"StatusBar":{}},{"SplitterHorizontal":{
            "State":"AAAA/wAAAAEAAAADAAAB+AAAA5wAAAGyAP////8BAAAAAQA=","Widgets":[{"SplitterVertical":{
            "State":"AAAA/wAAAAEAAAACAAAAHQAAA6AA/////wEAAAACAA==","Widgets":[{"SearchBar":{
            "Widgets": "1c827a58f07a4a939b185d9c0285f936|09356ff889694ff7941174448bd67b7a"}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAAA5wAAAQ0A/////wEAAAABAA==","Widgets":[
            {"LibraryFilter":{"Columns":"1","ID":"1c827a58f07a4a939b185d9c0285f936",
            "State":"AAAAJXjaY2BgYGRgYHjKAKFBgNH+A5QBFWAAAC6BAhk="}},{"LibraryFilter":{"Columns":"3",
            "ID":"09356ff889694ff7941174448bd67b7a","State":"AAAAJXjaY2BgYAQibgYwDQaM9h+gDKgAAwAVFAFA"}}]}}]}},
            {"PlaylistTabs":{"ID":"63546e1b2731459eb8dc448086c3ac6a","Widgets":[
            {"Playlist":{"ID":"943e9ac526414587baa7de36937a8b89"}}]}},
            {"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAAHQAAAbEAAAHTAP////8BAAAAAgA=",
            "Widgets":[{"Spacer":{}},{"ArtworkPanel":{}},{"SelectionInfo":{}}]}}]}},{"ControlBar":{}}]}}]})");

        layoutProvider->registerLayout(
            R"({"Ember":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAAEAAAA3AAAABoAAALMAAAAGQD/////AQAAAAIA",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAABAAAAAQAAAAEAAAABAAD/////AQAAAAEA",
            "Widgets":[{"LibraryFilter":{"Columns":"0","ID":"955f29805de446d7a9b6195a94bfd817",
            "State":"AAAAJXjaY2BgYASi8wxgGgwY7T9AGVABBgAsDAIE="}},{"LibraryFilter":{"Columns":"1",
            "ID":"4fee1a754b3e47ff86f4c711fbf0f0eb","State":"AAAAJXjaY2BgYASicwxgGgwY7T9AGVABBgAr7gID="}},
            {"LibraryFilter":{"Columns":"2","ID":"3b20c1db282d4bfe9a95c776e6723608",
            "State":"AAAAJXjaY2BgYASi8wxgGgwY7T9AGVABBgAsDAIE="}},{"LibraryFilter":{"Columns":"3",
            "ID":"34777508a4ae4ec5939620f235e8ec1a","State":"AAAAJXjaY2BgYASicwxgGgwY7T9AGVABBgAr7gID="}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAAFewAAAc8A/////wEAAAABAA==",
            "Widgets":[{"ControlBar":{}},{"SearchBar":{"Widgets": "955f29805de446d7a9b6195a94bfd817|
            34777508a4ae4ec5939620f235e8ec1a|4fee1a754b3e47ff86f4c711fbf0f0eb|3b20c1db282d4bfe9a95c776e6723608"}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAADAAABdAAABGgAAAFgAP////8BAAAAAQA=",
            "Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAACAAABdAAAAU0A/////wEAAAACAA==",
            "Widgets":[{"ArtworkPanel":{}},{"SelectionInfo":{}}]}},{"Playlist":{
            "ID":"3eb9c89184844b5a97c2dabb05aac1c0"}},{"PlaylistOrganiser":{
            "ID":"1a202fbca60f4a838b456663cebd1c67"}}]}},{"StatusBar":{}}]}}]})");
    }
};

FiltersPlugin::FiltersPlugin()
    : p{std::make_unique<Private>()}
{ }

FiltersPlugin::~FiltersPlugin() = default;

void FiltersPlugin::initialise(const CorePluginContext& context)
{
    p->library       = context.library;
    p->playerManager = context.playerManager;
    p->settings      = context.settingsManager;

    p->filterSettings = std::make_unique<FiltersSettings>(p->settings);
}

void FiltersPlugin::initialise(const GuiPluginContext& context)
{
    p->actionManager  = context.actionManager;
    p->layoutProvider = context.layoutProvider;
    p->factory        = context.widgetFactory;
    p->trackSelection = context.trackSelection;

    p->filterController
        = new FilterController(p->library, p->trackSelection, context.editableLayout, p->settings, this);

    p->factory->registerClass<FilterWidget>(
        QStringLiteral("LibraryFilter"), [this]() { return p->filterController->createFilter(); }, "Library Filter");

    p->generalPage = std::make_unique<FiltersGeneralPage>(p->settings);
    p->columnsPage
        = std::make_unique<FiltersColumnPage>(p->actionManager, p->filterController->columnRegistry(), p->settings);

    p->registerLayouts();
}

void FiltersPlugin::shutdown()
{
    p->filterController->shutdown();
}
} // namespace Fooyin::Filters

#include "moc_filtersplugin.cpp"
