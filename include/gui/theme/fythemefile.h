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

#include "fygui_export.h"

#include <gui/theme/fytheme.h>

#include <expected>

namespace Fooyin {
class FYGUI_EXPORT FyThemeFile
{
public:
    static constexpr int CurrentVersion{1};

    using ReadResult  = std::expected<FyTheme, QString>;
    using WriteResult = std::expected<void, QString>;

    [[nodiscard]] static QByteArray toJson(const FyTheme& theme);
    [[nodiscard]] static ReadResult fromJson(const QByteArray& data);

    [[nodiscard]] static ReadResult read(const QString& path);
    [[nodiscard]] static WriteResult write(const FyTheme& theme, const QString& path);
};
} // namespace Fooyin
