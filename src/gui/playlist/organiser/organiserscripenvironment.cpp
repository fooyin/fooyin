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

#include "organiserscripenvironment.h"

#include "playlistorganiseritem.h"

using namespace Qt::StringLiterals;

namespace {
const Fooyin::OrganiserScripEnvironment* organiserEnvironment(const Fooyin::ScriptContext& context)
{
    return context.environment ? static_cast<const Fooyin::OrganiserScripEnvironment*>(context.environment) : nullptr;
}

QString nodeName(const Fooyin::ScriptContext& context)
{
    if(const auto* environment = organiserEnvironment(context)) {
        return environment->nodeName();
    }

    return context.playlist ? context.playlist->name() : QString{};
}

QString isGroup(const Fooyin::ScriptContext& context)
{
    if(const auto* environment = organiserEnvironment(context); environment && environment->isGroup()) {
        return u"1"_s;
    }

    return {};
}

QString organiserCount(const Fooyin::ScriptContext& context)
{
    if(const auto* environment = organiserEnvironment(context)) {
        return QString::number(environment->count());
    }

    return context.playlist ? QString::number(context.playlist->trackCount()) : QString{};
}
} // namespace

namespace Fooyin {
OrganiserScripEnvironment::OrganiserScripEnvironment(const PlaylistOrganiserItem* item)
    : m_item{item}
{ }

void OrganiserScripEnvironment::setItem(const PlaylistOrganiserItem* item)
{
    m_item = item;
}

QString OrganiserScripEnvironment::nodeName() const
{
    return m_item ? m_item->title() : QString{};
}

bool OrganiserScripEnvironment::isGroup() const
{
    return m_item && m_item->type() == PlaylistOrganiserItem::GroupItem;
}

int OrganiserScripEnvironment::count() const
{
    if(!m_item) {
        return 0;
    }

    if(m_item->type() == PlaylistOrganiserItem::GroupItem) {
        return m_item->childCount();
    }

    if(const auto* playlist = m_item->playlist()) {
        return playlist->trackCount();
    }

    return 0;
}

StaticScriptVariableProvider organiserScripVariableProvider()
{
    return {makeScriptVariableDescriptor<nodeName>(VariableKind::Generic, u"NODE_NAME"_s),
            makeScriptVariableDescriptor<nodeName>(VariableKind::Generic, u"NODENAME"_s),
            makeScriptVariableDescriptor<isGroup>(VariableKind::Generic, u"IS_GROUP"_s),
            makeScriptVariableDescriptor<organiserCount>(VariableKind::Generic, u"COUNT"_s)};
}
} // namespace Fooyin
