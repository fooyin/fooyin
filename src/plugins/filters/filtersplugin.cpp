/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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
#include "settings/filtersguipage.h"

#include <gui/editablelayout.h>
#include <gui/layoutprovider.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>

namespace Fooyin::Filters {
struct FiltersPlugin::Private
{
    ActionManager* actionManager;
    SettingsManager* settings;
    MusicLibrary* library;
    PlayerController* playerController;
    LayoutProvider* layoutProvider;
    WidgetProvider* widgetProvider;
    TrackSelectionController* trackSelection;

    FilterController* filterController;
    std::unique_ptr<FiltersSettings> filterSettings;

    std::unique_ptr<FiltersGeneralPage> generalPage;
    std::unique_ptr<FiltersGuiPage> guiPage;
    std::unique_ptr<FiltersColumnPage> columnsPage;

    void registerLayouts() const
    {
        layoutProvider->registerLayout(
            R"({"Name":"Obsidian","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAADAAAALQAAA3cAAAAcAP////8BAAAAAgA=",
            "Widgets":[{"StatusBar":{}},{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAADAAACBwAAA9YAAAGhAP////8BAAAAAQA=",
            "Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAACAAAAIAAAA6AA/////wEAAAACAA==","Widgets":[{"SearchBar":{
            "ID":"866eee837dee4919bd1d1e7c916e4013","Widgets":"1c827a58f07a4a939b185d9c0285f936|09356ff889694ff7941174448bd67b7a"}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAACAAAA5wAAAQ0A/////wEAAAABAA==","Widgets":[{"LibraryFilter":{"Columns":"1",
            "Group":"Default","ID":"1c827a58f07a4a939b185d9c0285f936","Index":0,"State":"AAAAJXjaY2BgYGRgYOhigNAgwGj/AcqACjAAACPXAb4="}},
            {"LibraryFilter":{"Columns":"3","Group":"Default","ID":"09356ff889694ff7941174448bd67b7a","Index":1,
            "State":"AAAAJXjaY2BgYGRgYFjKAKFBgNH+A5QBFWAAACcBAdk="}}]}}]}},{"PlaylistTabs":{"Widgets":[{"Playlist":{}}]}},
            {"SplitterVertical":{"State":"AAAA/wAAAAEAAAACAAABsQAAAdMA/////wEAAAACAA==","Widgets":[{"ArtworkPanel":{}},{"SelectionInfo":{}}]}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAAAggAABoIAAAA+AAAAHAD/////AQAAAAEA","Widgets":[{"PlayerControls":{}},
            {"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}}]}}]}}]})");

        layoutProvider->registerLayout(
            R"({"Name":"Ember","Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAAEAAAA2QAAABoAAALGAAAAFgD/////AQAAAAIA",
            "Widgets":[{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAEAAABAAAAAQAAAAEAAAABAAD/////AQAAAAEA",
            "Widgets":[{"LibraryFilter":{"Columns":"0","Group":"Default","ID":"955f29805de446d7a9b6195a94bfd817","Index":0}},
            {"LibraryFilter":{"Columns":"1","Group":"Default","ID":"4fee1a754b3e47ff86f4c711fbf0f0eb","Index":1}},
            {"LibraryFilter":{"Columns":"2","Group":"Default","ID":"3b20c1db282d4bfe9a95c776e6723608","Index":2}},
            {"LibraryFilter":{"Columns":"3","Group":"Default","ID":"34777508a4ae4ec5939620f235e8ec1a","Index":3}}]}},
            {"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAAFAAAAcgAABRoAAAA2AAAAGAAAAWQA/////wEAAAABAA==",
            "Widgets":[{"PlayerControls":{}},{"SeekBar":{}},{"PlaylistControls":{}},{"VolumeControls":{}},{"SearchBar":{
            "Widgets":"955f29805de446d7a9b6195a94bfd817|34777508a4ae4ec5939620f235e8ec1a|4fee1a754b3e47ff86f4c711fbf0f0eb|
            3b20c1db282d4bfe9a95c776e6723608"}}]}},{"SplitterHorizontal":{"State":"AAAA/wAAAAEAAAADAAABdAAABGgAAAFgAP////8BAAAAAQA=",
            "Widgets":[{"SplitterVertical":{"State":"AAAA/wAAAAEAAAACAAABdAAAAU0A/////wEAAAACAA==","Widgets":[{"ArtworkPanel":{}},
            {"SelectionInfo":{}}]}},{"Playlist":{}},{"PlaylistOrganiser":{}}]}},{"StatusBar":{}}]}}]})");
    }
};

FiltersPlugin::FiltersPlugin()
    : p{std::make_unique<Private>()}
{ }

FiltersPlugin::~FiltersPlugin() = default;

void FiltersPlugin::initialise(const CorePluginContext& context)
{
    p->library          = context.library;
    p->playerController = context.playerController;
    p->settings         = context.settingsManager;
}

void FiltersPlugin::initialise(const GuiPluginContext& context)
{
    p->actionManager  = context.actionManager;
    p->layoutProvider = context.layoutProvider;
    p->widgetProvider = context.widgetProvider;
    p->trackSelection = context.trackSelection;

    p->filterSettings = std::make_unique<FiltersSettings>(p->settings);
    p->filterController
        = new FilterController(p->library, p->trackSelection, context.editableLayout, p->settings, this);

    p->widgetProvider->registerWidget(
        QStringLiteral("LibraryFilter"), [this]() { return p->filterController->createFilter(); },
        tr("Library Filter"));

    p->generalPage = std::make_unique<FiltersGeneralPage>(p->settings);
    p->guiPage     = std::make_unique<FiltersGuiPage>(p->settings);
    p->columnsPage = std::make_unique<FiltersColumnPage>(p->actionManager, p->settings);

    p->registerLayouts();
}
} // namespace Fooyin::Filters

#include "moc_filtersplugin.cpp"
