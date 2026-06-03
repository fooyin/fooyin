/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <core/engine/outputplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/pluginconfigguiplugin.h>
#include <gui/plugins/pluginsettingsprovider.h>

namespace Fooyin::Alsa {
class AlsaPlugin : public QObject,
                   public Plugin,
                   public OutputPlugin,
                   public PluginConfigGuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "alsa.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::OutputPlugin Fooyin::PluginConfigGuiPlugin)

public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] OutputCreator creator() const override;
    [[nodiscard]] std::unique_ptr<PluginSettingsProvider> settingsProvider() const override;
};
} // namespace Fooyin::Alsa
