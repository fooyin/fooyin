/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <QColor>
#include <QDataStream>
#include <QFont>
#include <QMap>
#include <QPalette>
#include <QString>

namespace Fooyin {
struct FYGUI_EXPORT PaletteKey
{
    QPalette::ColorRole role;
    QPalette::ColorGroup group{QPalette::All};

    bool operator==(const PaletteKey& other) const
    {
        return std::tie(role, group) == std::tie(other.role, other.group);
    };

    bool operator<(const PaletteKey& other) const
    {
        if(role < other.role) {
            return true;
        }
        if(role > other.role) {
            return false;
        }

        return group > other.group;
    }

    friend QDataStream& operator<<(QDataStream& stream, const PaletteKey& key);
    friend QDataStream& operator>>(QDataStream& stream, PaletteKey& key);
};

struct FYGUI_EXPORT FyTheme
{
    int id{0};
    QString name;
    int index{0};
    bool isDefault{false};
    QMap<PaletteKey, QColor> colours;
    QMap<QString, QFont> fonts;

    [[nodiscard]] bool isValid() const
    {
        return !colours.empty() || !fonts.empty();
    }

    bool operator==(const FyTheme& other) const
    {
        return std::tie(id, name, index, isDefault, colours, fonts)
            == std::tie(other.id, other.name, other.index, other.isDefault, other.colours, other.fonts);
    };

    friend QDataStream& operator<<(QDataStream& stream, const FyTheme& scheme);
    friend QDataStream& operator>>(QDataStream& stream, FyTheme& scheme);
};
} // namespace Fooyin
