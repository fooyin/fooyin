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

#pragma once

#include "core/plugins/databaseplugin.h"
#include "filterwidget.h"

#include <core/plugins/plugin.h>
#include <core/plugins/settingsplugin.h>
#include <core/plugins/threadplugin.h>
#include <core/plugins/widgetplugin.h>
#include <gui/widgetfactory.h>

namespace Filters {
class FilterManager;

namespace Settings {
class FiltersSettings;
}

class FiltersPlugin : public QObject,
                      public Plugins::Plugin,
                      public Plugins::WidgetPlugin,
                      public Plugins::ThreadPlugin,
                      public Plugins::DatabasePlugin,
                      public Plugins::SettingsPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.fooyin.plugin" FILE "metadata.json")
    Q_INTERFACES(Plugins::Plugin)
    Q_INTERFACES(Plugins::WidgetPlugin)
    Q_INTERFACES(Plugins::ThreadPlugin)
    Q_INTERFACES(Plugins::DatabasePlugin)
    Q_INTERFACES(Plugins::SettingsPlugin)

public:
    FiltersPlugin()           = default;
    ~FiltersPlugin() override = default;

    void initialise() override;
    void initialise(WidgetPluginContext context) override;
    void initialise(SettingsPluginContext context) override;
    void initialise(ThreadPluginContext context) override;
    void initialise(DatabasePluginContext context) override;

private:
    template <typename T>
    void registerFilter(const QString& key, const QString& name)
    {
        m_factory->registerClass<T>(key,
                                    [this]() {
                                        return new T(m_filterManager, m_settings);
                                    },
                                    name, {"Filter"});
    }

    Core::ActionManager* m_actionManager;
    Core::SettingsManager* m_settings;
    Core::ThreadManager* m_threadManager;
    Core::DB::Database* m_database;
    Core::Library::MusicLibrary* m_library;
    Core::Player::PlayerManager* m_playerManager;
    Gui::Widgets::WidgetFactory* m_factory;
    FilterManager* m_filterManager;
    std::unique_ptr<Settings::FiltersSettings> m_filterSettings;
};
} // namespace Filters
