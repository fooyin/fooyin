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

#include "fycore_export.h"

#include <core/engine/audiodecoder.h>
#include <core/engine/decoderplugin.h>

namespace Fooyin {
class Track;

class FYCORE_EXPORT DecoderProvider final
{
public:
    DecoderProvider();
    ~DecoderProvider();

    [[nodiscard]] QStringList supportedFileExtensions() const;
    [[nodiscard]] std::unique_ptr<AudioDecoder> createDecoderForTrack(const Track& track) const;

    void addDecoder(const QString& name, const QStringList& supportedExtensions, const DecoderCreator& creator);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
