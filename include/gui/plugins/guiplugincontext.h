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

#pragma once

#include "fygui_export.h"

namespace Fooyin {
class ActionManager;
class EditableLayout;
class LayoutProvider;
class PropertiesDialog;
class SearchController;
class ThemeRegistry;
class TrackSelectionController;
class WidgetProvider;
class WindowController;

/*!
 * Passed to gui plugins in GuiPlugin::initialise.
 */
struct FYGUI_EXPORT GuiPluginContext
{
    GuiPluginContext(ActionManager* actionManager_, LayoutProvider* layoutProvider_,
                     TrackSelectionController* trackSelection_, SearchController* searchController_,
                     PropertiesDialog* propertiesDialog_, WidgetProvider* widgetProvider_,
                     EditableLayout* editableLayout_, WindowController* windowController_,
                     ThemeRegistry* themeRegistry_)
        : actionManager{actionManager_}
        , layoutProvider{layoutProvider_}
        , trackSelection{trackSelection_}
        , searchController{searchController_}
        , propertiesDialog{propertiesDialog_}
        , widgetProvider{widgetProvider_}
        , editableLayout{editableLayout_}
        , windowController{windowController_}
        , themeRegistry{themeRegistry_}
    { }

    ActionManager* actionManager;
    LayoutProvider* layoutProvider;
    TrackSelectionController* trackSelection;
    SearchController* searchController;
    PropertiesDialog* propertiesDialog;
    WidgetProvider* widgetProvider;
    EditableLayout* editableLayout;
    WindowController* windowController;
    ThemeRegistry* themeRegistry;
};
} // namespace Fooyin
