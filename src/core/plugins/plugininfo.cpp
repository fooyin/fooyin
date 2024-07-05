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

#include <QPluginLoader>
#include <utility>

namespace Fooyin {
PluginInfo::PluginInfo(QString name, const QString& filename, const QJsonObject& allMetadata)
    : m_name{std::move(name)}
    , m_filename{filename}
    , m_metadata{allMetadata.value(QStringLiteral("MetaData")).toObject()}
    , m_version{m_metadata.value(QStringLiteral("Version")).toString()}
    , m_vendor{m_metadata.value(QStringLiteral("Vendor")).toString()}
    , m_copyright{m_metadata.value(QStringLiteral("Copyright")).toString()}
    , m_license{m_metadata.value(QStringLiteral("License")).toString()}
    , m_category{m_metadata.value(QStringLiteral("Category")).toString()}
    , m_description{m_metadata.value(QStringLiteral("Description")).toString()}
    , m_url{m_metadata.value(QStringLiteral("Url")).toString()}
    , m_isRequired{false}
    , m_isLoaded{false}
    , m_isDisabled{false}
    , m_status{Status::Read}
    , m_root{nullptr}
    , m_plugin{nullptr}
{
    m_loader.setFileName(filename);
}

void PluginInfo::load()
{
    if(m_loader.fileName().isEmpty()) {
        return;
    }

    if(!m_loader.load()) {
        m_error  = QStringLiteral("Plugin (%1) couldn't be loaded: %2").arg(m_name, m_error);
        m_status = Status::Invalid;
        return;
    }

    m_root   = m_loader.instance();
    m_plugin = qobject_cast<Plugin*>(m_root);

    if(!m_plugin) {
        m_error  = QStringLiteral("Plugin (%1) does not subclass 'Fooyin::Plugin'").arg(m_name);
        m_status = Status::Invalid;
        return;
    }

    m_status   = Status::Loaded;
    m_isLoaded = true;
}

void PluginInfo::unload()
{
    if(!m_plugin) {
        return;
    }
    m_plugin->shutdown();
    const bool deleted = m_loader.unload();
    if(!deleted) {
        delete m_plugin;
    }
}

void PluginInfo::initialise()
{
    if(isLoaded()) {
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
}

void PluginInfo::setStatus(Status status)
{
    m_status = status;
}

void PluginInfo::setError(const QString& error)
{
    m_error = error;
}

QString PluginInfo::version() const
{
    return m_version;
}

QString PluginInfo::vendor() const
{
    return m_vendor;
}

QString PluginInfo::identifier() const
{
    return QString{m_vendor + u"." + m_name}.simplified().replace(QLatin1String(" "), QString{}).toLower();
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
} // namespace Fooyin
