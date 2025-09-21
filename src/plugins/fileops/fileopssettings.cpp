/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "fileopssettings.h"

#include <core/coresettings.h>

constexpr auto SavedPresets = "FileOps/Presets";

namespace Fooyin::FileOps {
std::vector<FileOpPreset> getPresets()
{
    std::vector<FileOpPreset> presets;

    const FySettings settings;
    auto byteArray = settings.value(SavedPresets).toByteArray();

    if(!byteArray.isEmpty()) {
        byteArray = qUncompress(byteArray);

        QDataStream stream(&byteArray, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        qint32 size;
        stream >> size;

        while(size > 0) {
            --size;

            FileOpPreset preset;
            stream >> preset;

            presets.push_back(preset);
        }
    }

    return presets;
}

void savePresets(const std::vector<FileOpPreset>& presets)
{
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<qint32>(presets.size());

    for(const auto& preset : presets) {
        stream << preset;
    }

    byteArray = qCompress(byteArray, 9);

    FySettings settings;
    settings.setValue(SavedPresets, byteArray);
}
} // namespace Fooyin::FileOps
