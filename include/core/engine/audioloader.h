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
    template <typename T>
    struct LoaderEntry
    {
        int index{0};
        QString name;
        QStringList extensions;
        bool enabled{true};
        T creator;
    };

    AudioLoader();
    ~AudioLoader();

    void saveState();
    void restoreState();

    [[nodiscard]] QStringList supportedFileExtensions() const;
    [[nodiscard]] QStringList supportedTrackExtensions() const;
    [[nodiscard]] QStringList supportedArchiveExtensions() const;
    [[nodiscard]] bool canWriteMetadata(const Track& track) const;

    [[nodiscard]] bool isArchive(const QString& file) const;
    [[nodiscard]] std::unique_ptr<AudioDecoder> decoderForFile(const QString& file) const;
    [[nodiscard]] std::unique_ptr<AudioDecoder> decoderForTrack(const Track& track) const;
    [[nodiscard]] std::unique_ptr<AudioReader> readerForFile(const QString& file) const;
    [[nodiscard]] std::unique_ptr<AudioReader> readerForTrack(const Track& track) const;
    [[nodiscard]] std::unique_ptr<ArchiveReader> archiveReaderForFile(const QString& file) const;

    [[nodiscard]] bool readTrackMetadata(Track& track) const;
    [[nodiscard]] QByteArray readTrackCover(const Track& track, Track::Cover cover) const;
    [[nodiscard]] bool writeTrackMetadata(const Track& track, AudioReader::WriteOptions options) const;
    [[nodiscard]] bool writeTrackCover(const Track& track, const TrackCovers& coverData,
                                       AudioReader::WriteOptions options) const;

    void addDecoder(const QString& name, const DecoderCreator& creator, int priority = -1);
    void addReader(const QString& name, const ReaderCreator& creator, int priority = -1);
    void addArchiveReader(const QString& name, const ArchiveReaderCreator& creator, int priority = -1);

    [[nodiscard]] std::vector<LoaderEntry<DecoderCreator>> decoders() const;
    [[nodiscard]] std::vector<LoaderEntry<ReaderCreator>> readers() const;
    [[nodiscard]] std::vector<LoaderEntry<ArchiveReaderCreator>> archiveReaders() const;

    void setDecoderEnabled(const QString& name, bool enabled);
    void changeDecoderIndex(const QString& name, int index);
    void setReaderEnabled(const QString& name, bool enabled);
    void changeReaderIndex(const QString& name, int index);

    void reloadDecoderExtensions(const QString& name);
    void reloadReaderExtensions(const QString& name);

    void reset();

private:
    std::unique_ptr<AudioLoaderPrivate> p;
};
} // namespace Fooyin
