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

#pragma once

#include "pluginsystem_global.h"

#include <QJsonObject>
#include <QString>

namespace PluginSystem {
class PLUGINSYSTEM_EXPORT PluginInfo
{
public:
    PluginInfo();

    PluginInfo(QString  name, QString  filename, const QJsonObject& metadata);

    void addDependency(PluginInfo* dependency);

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString filename() const;
    [[nodiscard]] QJsonObject metadata() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString identifier() const;
    [[nodiscard]] QString category() const;
    [[nodiscard]] QString copyright() const;
    [[nodiscard]] QString description() const;
    [[nodiscard]] QString url() const;
    [[nodiscard]] QList<PluginInfo*> dependencies() const;
    [[nodiscard]] bool isRequired() const;
    [[nodiscard]] bool isLoaded() const;

    friend class PluginManager;

private:
    QString m_name;
    QString m_filename;
    QString m_version;
    QString m_vendor;
    QString m_copyright;
    QString m_category;
    QString m_description;
    QList<PluginInfo*> m_dependencies;
    QString m_url;
    QJsonObject m_metadata;
    bool m_isRequired;
    bool m_isLoaded;
};
}; // namespace PluginSystem

Q_DECLARE_METATYPE(PluginSystem::PluginInfo*)
