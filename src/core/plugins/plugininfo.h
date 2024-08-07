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

#pragma once

#include "fycore_export.h"

#include <QJsonObject>
#include <QPluginLoader>
#include <QString>

namespace Fooyin {
class PluginManager;
class Plugin;

class FYCORE_EXPORT PluginInfo
{
    Q_GADGET

public:
    enum class Status
    {
        Invalid,
        Read,
        Loaded,
        Initialised,
        Disabled,
    };
    Q_ENUM(Status)

    PluginInfo(QString filepath, const QJsonObject& metadata);

    void load();
    void unload();
    void initialise();

    [[nodiscard]] Plugin* plugin() const;
    [[nodiscard]] QObject* root() const;

    [[nodiscard]] QString filepath() const;
    [[nodiscard]] QJsonObject metadata() const;

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString author() const;
    [[nodiscard]] QString identifier() const;
    [[nodiscard]] QStringList category() const;
    [[nodiscard]] QString copyright() const;
    [[nodiscard]] QString license() const;
    [[nodiscard]] QString description() const;
    [[nodiscard]] QString url() const;

    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isDisabled() const;
    [[nodiscard]] Status status() const;
    [[nodiscard]] QString error() const;
    [[nodiscard]] bool hasError() const;

    void setDisabled(bool disabled);

private:
    QString m_filepath;
    QJsonObject m_metadata;

    QString m_name;
    QString m_version;
    QString m_author;
    QString m_copyright;
    QString m_license;
    QStringList m_category;
    QString m_description;
    QString m_url;

    Status m_status;
    QString m_error;
    bool m_isDisabled;

    QObject* m_root;
    Plugin* m_plugin;
    QPluginLoader m_loader;
};
} // namespace Fooyin
