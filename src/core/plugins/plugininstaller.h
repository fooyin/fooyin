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

#include "fycore_export.h"

#include "plugininfo.h"

#include <QString>

#include <expected>

namespace Fooyin::PluginInstaller {
enum class InstallError : uint8_t
{
    AlreadyInstalled = 0,
    InvalidSource,
    UnsupportedPlatform,
    BundleReadFailed,
    InvalidBundle,
    MissingPlatform,
    InvalidPlugin,
    IncompatibleApi,
    InvalidMetadata,
    MultiplePlugins,
    WriteFailed,
};

FYCORE_EXPORT std::expected<std::unique_ptr<PluginInfo>, InstallError>
install(const QString& filepath, const QString& pluginsPath, bool overwrite);
FYCORE_EXPORT bool applyPendingUpdates(const QString& pluginsPath);
} // namespace Fooyin::PluginInstaller
