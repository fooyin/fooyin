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
 */

#include "splitterstate.h"

#include <QDataStream>
#include <QIODevice>

constexpr qint32 SplitterMagic        = 0xFF;
constexpr qint32 SplitterStateVersion = 1;

namespace Fooyin {
std::optional<SplitterState> decodeSplitterState(const QByteArray& data)
{
    QByteArray stateData{data};
    QDataStream stream{&stateData, QIODevice::ReadOnly};
    stream.setVersion(QDataStream::Qt_5_0);

    qint32 marker;
    qint32 version;
    stream >> marker;
    stream >> version;
    if(marker != SplitterMagic || version > SplitterStateVersion) {
        return {};
    }

    SplitterState state;
    qint32 handleWidth;
    qint32 orientation;

    stream >> state.sizes;
    stream >> state.childrenCollapsible;
    stream >> handleWidth;
    stream >> state.opaqueResize;
    stream >> orientation;

    if(version >= 1) {
        stream >> state.opaqueResizeSet;
    }
    if(stream.status() != QDataStream::Ok) {
        return {};
    }

    state.handleWidth = handleWidth;
    state.orientation = static_cast<Qt::Orientation>(orientation);
    return state;
}

QByteArray encodeSplitterState(const SplitterState& state)
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_5_0);

    stream << SplitterMagic;
    stream << SplitterStateVersion;
    stream << state.sizes;
    stream << state.childrenCollapsible;
    stream << state.handleWidth;
    stream << state.opaqueResize;
    stream << static_cast<qint32>(state.orientation);
    stream << state.opaqueResizeSet;

    return data;
}
} // namespace Fooyin
