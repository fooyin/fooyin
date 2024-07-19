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

#include <core/engine/audioinput.h>
#include <core/engine/inputplugin.h>

namespace Fooyin {
class AudioLoaderPrivate;
class Track;

class FYCORE_EXPORT AudioLoader final
{
public:
    AudioLoader();
    ~AudioLoader();

    [[nodiscard]] QStringList supportedFileExtensions() const;
    [[nodiscard]] bool canWriteMetadata(const Track& track) const;

    [[nodiscard]] AudioInput* decoderForTrack(const Track& track) const;
    [[nodiscard]] bool readTrackMetadata(Track& track) const;
    [[nodiscard]] QByteArray readTrackCover(const Track& track, Track::Cover cover) const;
    [[nodiscard]] bool writeTrackMetadata(const Track& track, const AudioInput::WriteOptions& options) const;

    void addDecoder(const QString& name, const InputCreator& creator);

    void destroyThreadInstance();

private:
    std::unique_ptr<AudioLoaderPrivate> p;
};
} // namespace Fooyin
