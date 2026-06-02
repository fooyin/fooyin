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

#include <gui/settings/context/staticcontextmenupage.h>

#include "configurablecontextmenupage.h"

namespace {
Fooyin::ContextMenuNodeList contextMenuNodes(std::span<const Fooyin::StaticContextMenu::Item> items)
{
    Fooyin::ContextMenuNodeList nodes;
    nodes.reserve(items.size());

    for(const auto& item : items) {
        nodes.emplace_back(QString::fromUtf8(item.id), item.isSeparator ? QString{} : Fooyin::translate(item.title),
                           QString{}, item.isSeparator);
    }

    return nodes;
}
} // namespace

namespace Fooyin {
StaticContextMenuPage::StaticContextMenuPage(SettingsManager* settings, StaticContextMenuDescriptor descriptor,
                                             QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(descriptor.pageId);
    setName(translate(descriptor.pageName));
    setCategory({tr("Interface"), tr("Context Menu")});

    const QString description = translate(descriptor.description);
    setWidgetCreator([description, descriptor = std::move(descriptor)] {
        return new ConfigurableContextMenuWidget(
            description, {.nodeFactory          = [items = descriptor.items] { return ::contextMenuNodes(items); },
                          .readDisabledIds      = descriptor.readDisabledIds,
                          .writeDisabledIds     = descriptor.writeDisabledIds,
                          .defaultDisabledIds   = descriptor.defaultDisabledIds,
                          .readTopLevelOrder    = descriptor.readTopLevelOrder,
                          .writeTopLevelOrder   = descriptor.writeTopLevelOrder,
                          .defaultTopLevelOrder = descriptor.defaultTopLevelOrder,
                          .allowReordering      = descriptor.allowReordering});
    });
}
} // namespace Fooyin
