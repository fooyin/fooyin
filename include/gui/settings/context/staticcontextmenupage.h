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

#pragma once

#include "fygui_export.h"

#include <gui/settings/context/staticcontextmenu.h>
#include <utils/settings/settingsmanager.h>
#include <utils/settings/settingspage.h>

#include <QAnyStringView>

#include <functional>
#include <span>
#include <utility>

namespace Fooyin {
namespace ContextMenuSettings {
template <auto Key>
auto makeStringListReader(SettingsManager* settings)
{
    return [settings] {
        return settings->value<Key>();
    };
}

template <auto Key>
auto makeStringListWriter(SettingsManager* settings)
{
    return [settings](const QStringList& values) {
        settings->set<Key>(values);
    };
}

inline auto makeFileStringListReader(SettingsManager* settings, QAnyStringView key, QStringList defaultValue = {})
{
    const QString ownedKey = key.toString();
    return [settings, ownedKey, defaultValue = std::move(defaultValue)] {
        return settings->fileValue(ownedKey, defaultValue).toStringList();
    };
}

inline auto makeFileStringListWriter(SettingsManager* settings, QAnyStringView key)
{
    const QString ownedKey = key.toString();
    return [settings, ownedKey](const QStringList& values) {
        settings->fileSet(ownedKey, values);
    };
}
} // namespace ContextMenuSettings

struct FYGUI_EXPORT StaticContextMenuDescriptor
{
    using StringListReader = std::function<QStringList()>;
    using StringListWriter = std::function<void(const QStringList&)>;

    const char* pageId;
    TranslatableText pageName;
    TranslatableText description;
    std::span<const StaticContextMenu::Item> items;
    StringListReader readDisabledIds;
    StringListWriter writeDisabledIds;
    QStringList defaultDisabledIds;
    StringListReader readTopLevelOrder;
    StringListWriter writeTopLevelOrder;
    QStringList defaultTopLevelOrder;
    bool allowReordering{true};
};

template <size_t N, typename ReadDisabledIds, typename WriteDisabledIds, typename ReadTopLevelOrder,
          typename WriteTopLevelOrder>
StaticContextMenuDescriptor
makeStaticContextMenuDescriptor(const char* pageId, TranslatableText pageName, TranslatableText description,
                                const std::array<StaticContextMenu::Item, N>& items, ReadDisabledIds&& readDisabledIds,
                                WriteDisabledIds&& writeDisabledIds, ReadTopLevelOrder&& readTopLevelOrder,
                                WriteTopLevelOrder&& writeTopLevelOrder, const QStringList& defaultDisabledIds = {},
                                const QStringList& defaultTopLevelOrder = {})
{
    return {
        .pageId               = pageId,
        .pageName             = pageName,
        .description          = description,
        .items                = std::span<const StaticContextMenu::Item>{items},
        .readDisabledIds      = std::forward<ReadDisabledIds>(readDisabledIds),
        .writeDisabledIds     = std::forward<WriteDisabledIds>(writeDisabledIds),
        .defaultDisabledIds   = defaultDisabledIds,
        .readTopLevelOrder    = std::forward<ReadTopLevelOrder>(readTopLevelOrder),
        .writeTopLevelOrder   = std::forward<WriteTopLevelOrder>(writeTopLevelOrder),
        .defaultTopLevelOrder = defaultTopLevelOrder,
        .allowReordering      = true,
    };
}

template <auto DisabledKey, auto LayoutKey, size_t N>
StaticContextMenuDescriptor
makeStaticContextMenuDescriptor(const char* pageId, TranslatableText pageName, TranslatableText description,
                                const std::array<StaticContextMenu::Item, N>& items, SettingsManager* settings)
{
    return makeStaticContextMenuDescriptor(
        pageId, pageName, description, items, ContextMenuSettings::makeStringListReader<DisabledKey>(settings),
        ContextMenuSettings::makeStringListWriter<DisabledKey>(settings),
        ContextMenuSettings::makeStringListReader<LayoutKey>(settings),
        ContextMenuSettings::makeStringListWriter<LayoutKey>(settings),
        settings->defaultValue<DisabledKey>().toStringList(), settings->defaultValue<LayoutKey>().toStringList());
}

class FYGUI_EXPORT StaticContextMenuPage : public SettingsPage
{
    Q_OBJECT

public:
    StaticContextMenuPage(SettingsManager* settings, StaticContextMenuDescriptor descriptor, QObject* parent = nullptr);
};
} // namespace Fooyin
