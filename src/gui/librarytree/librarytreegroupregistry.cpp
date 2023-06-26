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

#include "librarytreegroupregistry.h"

#include "gui/guisettings.h"

#include <utils/settings/settingsmanager.h>

#include <QByteArray>
#include <QIODevice>

namespace Fy::Gui::Widgets {
int find(const IndexGroupMap& groupings, const QString& name)
{
    return static_cast<int>(std::ranges::count_if(std::as_const(groupings), [name](const auto& preset) {
        return preset.second.name == name;
    }));
}

void loadDefaults(LibraryTreeGroupRegistry* registry)
{
    registry->addGrouping(
        {-1, "Artist/Album", "$if2(%albumartist%,%artist%)||%album% (%year%)||%disc%.$num(%track%,2). %title%"});
    registry->addGrouping({-1, "Album", "%album% (%year%)||%disc%.$num(%track%,2). %title%"});
}

LibraryTreeGroupRegistry::LibraryTreeGroupRegistry(Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{ }

const IndexGroupMap& LibraryTreeGroupRegistry::groupings() const
{
    return m_groupings;
}

LibraryTreeGrouping LibraryTreeGroupRegistry::addGrouping(const LibraryTreeGrouping& grouping)
{
    if(!grouping.isValid()) {
        return {};
    }

    LibraryTreeGrouping newGroup{grouping};
    if(find(m_groupings, newGroup.name)) {
        auto count = find(m_groupings, newGroup.name + " (");
        newGroup.name += QString{" (%1)"}.arg(count);
    }
    newGroup.index = static_cast<int>(m_groupings.size());

    return m_groupings.emplace(newGroup.index, newGroup).first->second;
}

bool LibraryTreeGroupRegistry::changeGrouping(const LibraryTreeGrouping& grouping)
{
    auto groupIt = std::ranges::find_if(m_groupings, [grouping](const auto& cntrGroup) {
        return cntrGroup.second.index == grouping.index;
    });
    if(groupIt != m_groupings.end()) {
        const LibraryTreeGrouping oldGroup = groupIt->second;
        groupIt->second                    = grouping;
        emit groupingChanged(oldGroup, grouping);
        return true;
    }
    return false;
}

LibraryTreeGrouping LibraryTreeGroupRegistry::groupingByIndex(int index) const
{
    if(!m_groupings.contains(index)) {
        return {};
    }
    return m_groupings.at(index);
}

LibraryTreeGrouping LibraryTreeGroupRegistry::groupingByName(const QString& name) const
{
    if(m_groupings.empty()) {
        return {};
    }
    auto it = std::ranges::find_if(std::as_const(m_groupings), [name](const auto& preset) {
        return preset.second.name == name;
    });
    if(it == m_groupings.end()) {
        return m_groupings.at(0);
    }
    return it->second;
}

bool LibraryTreeGroupRegistry::removeByIndex(int index)
{
    if(!m_groupings.contains(index)) {
        return false;
    }
    m_groupings.erase(index);
    return true;
}

void LibraryTreeGroupRegistry::saveGroupings()
{
    QByteArray byteArray;
    QDataStream out(&byteArray, QIODevice::WriteOnly);

    out << m_groupings;

    byteArray = byteArray.toBase64();
    m_settings->set<Settings::LibraryTreeGrouping>(byteArray);
}

void LibraryTreeGroupRegistry::loadGroupings()
{
    QByteArray groups = m_settings->value<Settings::LibraryTreeGrouping>();
    groups            = QByteArray::fromBase64(groups);

    QDataStream in(&groups, QIODevice::ReadOnly);

    in >> m_groupings;

    if(m_groupings.empty()) {
        loadDefaults(this);
    }
}

QDataStream& operator<<(QDataStream& stream, const LibraryTreeGrouping& group)
{
    stream << group.index;
    stream << group.name;
    stream << group.script;
    return stream;
}

QDataStream& operator>>(QDataStream& stream, LibraryTreeGrouping& group)
{
    stream >> group.index;
    stream >> group.name;
    stream >> group.script;
    return stream;
}

QDataStream& operator<<(QDataStream& stream, const IndexGroupMap& groupMap)
{
    stream << static_cast<int>(groupMap.size());
    for(const auto& [index, group] : groupMap) {
        stream << index;
        stream << group;
    }
    return stream;
}

QDataStream& operator>>(QDataStream& stream, IndexGroupMap& groupMap)
{
    int size;
    stream >> size;

    while(size > 0) {
        --size;

        LibraryTreeGrouping group{};
        int index;
        stream >> index;
        stream >> group;
        groupMap.emplace(index, group);
    }
    return stream;
}
} // namespace Fy::Gui::Widgets
