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

#pragma once

#include <core/engine/encoderplugin.h>
#include <core/engine/inputplugin.h>
#include <core/plugins/plugin.h>

namespace Fooyin::Flac {
class FlacPlugin : public QObject,
                   public Plugin,
                   public InputPlugin,
                   public EncoderPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FOOYIN_PLUGIN_IID FILE "flac.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::InputPlugin Fooyin::EncoderPlugin)

public:
    [[nodiscard]] QString inputName() const override;
    [[nodiscard]] InputCreator inputCreator() const override;

    [[nodiscard]] QString encoderName() const override;
    [[nodiscard]] EncoderCreator encoderCreator() const override;
};
} // namespace Fooyin::Flac
