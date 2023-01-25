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

#include <QJsonObject>
#include <QPluginLoader>
#include <QString>

namespace Plugins {
class Plugin;

class PluginInfo
{
public:
    enum Status
    {
        Invalid,
        Read,
        Resolved,
        Loaded,
        Initialised,
        Running,
        Stopped,
        Deleted
    };

    PluginInfo(const QString& name, const QString& filename, const QJsonObject& metadata);
    ~PluginInfo();

    void load();
    void unload();
    void initialise();

    [[nodiscard]] Plugin* plugin() const;
    [[nodiscard]] QObject* root() const;

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString filename() const;
    [[nodiscard]] QJsonObject metadata() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString identifier() const;
    [[nodiscard]] QString category() const;
    [[nodiscard]] QString copyright() const;
    [[nodiscard]] QString description() const;
    [[nodiscard]] QString url() const;
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isDisabled() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] bool hasError() const;

    void setError(const QString& error);

private:
    QString m_name;
    QString m_filename;
    QJsonObject m_metadata;
    QString m_version;
    QString m_vendor;
    QString m_copyright;
    QString m_license;
    QString m_category;
    QString m_description;
    QString m_url;
    bool m_isRequired{false};
    bool m_isLoaded{false};
    bool m_isDisabled{false};
    Status m_status{Invalid};
    QString m_error;

    QObject* m_root{nullptr};
    Plugin* m_plugin{nullptr};
    QPluginLoader m_loader;
};
} // namespace Plugins
