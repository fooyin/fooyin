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

#include "plugininfo.h"

#include <core/plugins/plugin.h>
#include <utils/stringutils.h>

#include <QPluginLoader>

using namespace Qt::StringLiterals;

namespace Fooyin {
PluginInfo::PluginInfo(QString filepath, const QJsonObject& allMetadata)
    : m_filepath{std::move(filepath)}
    , m_metadata{allMetadata.value("MetaData"_L1).toObject()}
    , m_name{m_metadata.value("Name"_L1).toString()}
    , m_version{m_metadata.value("Version"_L1).toString()}
    , m_author{m_metadata.value("Author"_L1).toString()}
    , m_copyright{m_metadata.value("Copyright"_L1).toString()}
    , m_license{Utils::readMultiLineString(m_metadata.value("License"_L1))}
    , m_category{m_metadata.value("Category"_L1).toString().split(u'.', Qt::SkipEmptyParts)}
    , m_description{m_metadata.value("Description"_L1).toString()}
    , m_url{m_metadata.value("Url"_L1).toString()}
    , m_status{Status::Read}
    , m_isDisabled{false}
    , m_root{nullptr}
    , m_plugin{nullptr}
{
    m_loader.setFileName(m_filepath);
}

void PluginInfo::load()
{
    if(!m_loader.load()) {
        m_error  = u"Plugin (%1) couldn't be loaded: %2"_s.arg(m_name, m_error);
        m_status = Status::Invalid;
        return;
    }

    m_root   = m_loader.instance();
    m_plugin = qobject_cast<Plugin*>(m_root);

    if(!m_plugin) {
        m_error  = u"Plugin (%1) does not subclass 'Fooyin::Plugin'"_s.arg(m_name);
        m_status = Status::Invalid;
        return;
    }

    m_status = Status::Loaded;
}

void PluginInfo::unload()
{
    if(!m_plugin) {
        return;
    }

    m_plugin->shutdown();
    if(m_loader.unload()) {
        m_root   = nullptr;
        m_plugin = nullptr;
    }
}

void PluginInfo::initialise()
{
    if(m_status == Status::Loaded) {
        m_status = Status::Initialised;
    }
}

Plugin* PluginInfo::plugin() const
{
    return m_plugin;
}

QObject* PluginInfo::root() const
{
    return m_root;
}

QString PluginInfo::filepath() const
{
    return m_filepath;
}

QJsonObject PluginInfo::metadata() const
{
    return m_metadata;
}

QString PluginInfo::name() const
{
    return m_name;
}

QString PluginInfo::version() const
{
    return m_version;
}

QString PluginInfo::author() const
{
    return m_author;
}

QString PluginInfo::identifier() const
{
    return QString{m_author + "."_L1 + m_name}.simplified().replace(QLatin1String(" "), QString{}).toLower();
}

QStringList PluginInfo::category() const
{
    return m_category;
}

QString PluginInfo::copyright() const
{
    return m_copyright;
}

QString PluginInfo::license() const
{
    return m_license;
}

QString PluginInfo::description() const
{
    return m_description;
}

QString PluginInfo::url() const
{
    return m_url;
}

bool PluginInfo::isLoaded() const
{
    return m_status == Status::Loaded;
}

bool PluginInfo::isDisabled() const
{
    return m_isDisabled;
}

PluginInfo::Status PluginInfo::status() const
{
    return m_status;
}

QString PluginInfo::error() const
{
    return m_error;
}

bool PluginInfo::hasError() const
{
    return !m_error.isEmpty();
}

void PluginInfo::setDisabled(bool disabled)
{
    m_isDisabled = disabled;
    m_status     = Status::Disabled;
}
} // namespace Fooyin
