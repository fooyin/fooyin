/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "plugininfo.h"

#include <utility>

namespace PluginSystem {
PluginInfo::PluginInfo()
    : m_isLoaded{false}
{ }

PluginInfo::PluginInfo(QString name, QString  filename, const QJsonObject& metadata)
    : m_name{std::move(name)}
    , m_filename{std::move(filename)}
    , m_metadata{metadata}
    , m_isLoaded{false}
{ }

void PluginInfo::addDependency(PluginInfo* dependency)
{
    m_dependencies.append(dependency);
}

QString PluginInfo::name() const
{
    return m_name;
}

QString PluginInfo::filename() const
{
    return m_filename;
}

QJsonObject PluginInfo::metadata() const
{
    return m_metadata;
}

bool PluginInfo::isLoaded() const
{
    return m_isLoaded;
}

bool PluginInfo::isRequired() const
{
    return m_isRequired;
}

QString PluginInfo::version() const
{
    return m_version;
}

QString PluginInfo::identifier() const
{
    return (m_vendor + "." + m_name).toLower();
}

QString PluginInfo::category() const
{
    return m_category;
}

QString PluginInfo::copyright() const
{
    return m_copyright;
}

QString PluginInfo::description() const
{
    return m_description;
}

QString PluginInfo::url() const
{
    return m_url;
}

QList<PluginInfo*> PluginInfo::dependencies() const
{
    return m_dependencies;
}
}; // namespace PluginSystem
