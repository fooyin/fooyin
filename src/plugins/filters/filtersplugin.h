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

#include "filterwidget.h"

#include <core/coreplugin.h>
#include <core/plugins/plugin.h>

#include <gui/guiplugin.h>
#include <gui/widgetfactory.h>

namespace Fy {
namespace Filters {
class FilterManager;

namespace Settings {
class FiltersSettings;
} // namespace Settings

class FiltersPlugin : public QObject,
                      public Plugins::Plugin,
                      public Core::CorePlugin,
                      public Gui::GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.fooyin.plugin" FILE "metadata.json")
    Q_INTERFACES(Fy::Plugins::Plugin)
    Q_INTERFACES(Fy::Core::CorePlugin)
    Q_INTERFACES(Fy::Gui::GuiPlugin)

public:
    void initialise(const Core::CorePluginContext& context) override;
    void initialise(const Gui::GuiPluginContext& context) override;
    void shutdown() override;

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

    Utils::ActionManager* m_actionManager;
    Utils::SettingsManager* m_settings;
    Utils::ThreadManager* m_threadManager;
    Core::Library::MusicLibrary* m_library;
    Core::Player::PlayerManager* m_playerManager;
    Gui::Widgets::WidgetFactory* m_factory;

    FilterManager* m_filterManager;
    std::unique_ptr<Settings::FiltersSettings> m_filterSettings;
};
} // namespace Filters
} // namespace Fy
