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

#include <core/engine/enginedefs.h>

#include <QByteArray>
#include <QDataStream>
#include <QString>

namespace Fooyin {
struct DspChainPreset
{
    int id{-1};
    int index{-1};
    QString name;
    bool isDefault{false};
    Engine::DspChains chain;

    [[nodiscard]] bool isValid() const
    {
        return !name.isEmpty();
    }

    bool operator==(const DspChainPreset& other) const
    {
        return std::tie(id, index, name, chain) == std::tie(other.id, other.index, other.name, other.chain);
    }
};

QDataStream& operator<<(QDataStream& stream, const DspChainPreset& preset);
QDataStream& operator>>(QDataStream& stream, DspChainPreset& preset);
} // namespace Fooyin
