/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "trackcontextmenupage.h"

#include "configurablecontextmenupage.h"
#include "internalguisettings.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
TrackContextMenuPage::TrackContextMenuPage(TrackSelectionController* trackSelection, SettingsManager* settings,
                                           QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceContextMenuTrack);
    setName(tr("Track"));
    setCategory({tr("Interface"), tr("Context Menu")});
    setWidgetCreator([trackSelection, settings] {
        return new ConfigurableContextMenuWidget(
            tr("Unchecked items will be hidden from track selection context menus."),
            {.nodeFactory = [trackSelection] { return trackContextMenuNodes(trackSelection); },
             .readDisabledIds
             = ContextMenuSettings::makeStringListReader<Settings::Gui::Internal::ContextMenuTrackDisabledSections>(
                 settings),
             .writeDisabledIds
             = ContextMenuSettings::makeStringListWriter<Settings::Gui::Internal::ContextMenuTrackDisabledSections>(
                 settings),
             .defaultDisabledIds
             = settings->defaultValue<Settings::Gui::Internal::ContextMenuTrackDisabledSections>().toStringList(),
             .readTopLevelOrder
             = ContextMenuSettings::makeStringListReader<Settings::Gui::Internal::ContextMenuTrackLayout>(settings),
             .writeTopLevelOrder
             = ContextMenuSettings::makeStringListWriter<Settings::Gui::Internal::ContextMenuTrackLayout>(settings),
             .defaultTopLevelOrder
             = settings->defaultValue<Settings::Gui::Internal::ContextMenuTrackLayout>().toStringList(),
             .allowReordering = true});
    });
}

ContextMenuNodeList TrackContextMenuPage::trackContextMenuNodes(TrackSelectionController* trackSelection)
{
    ContextMenuNodeList nodes;

    const auto menuNodes = trackSelection->trackContextMenuNodes();
    const QString rootId = QString::fromLatin1(Constants::Menus::Context::TrackSelection);

    for(const auto& node : menuNodes) {
        if(node.area != TrackContextMenuArea::Track || !node.parentId) {
            continue;
        }

        QString parentId = node.parentId->name();
        if(node.isSeparator && parentId != rootId) {
            continue;
        }

        if(parentId == rootId) {
            parentId.clear();
        }

        nodes.emplace_back(node.id.name(), node.title, parentId, node.isSeparator);
    }

    return nodes;
}
} // namespace Fooyin
