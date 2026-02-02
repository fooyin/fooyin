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

/*!
 * Registry and selection layer for decoder/reader/archive backends.
 *
 * `AudioLoader` stores backend priority/enable state, resolves appropriate
 * backend for a file/track, and provides convenience helpers for metadata and
 * cover read/write operations.
 */
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
        bool isArchiveWrapper{false};
        T creator;
    };

    AudioLoader();
    ~AudioLoader();

    //! Persist current backend order/enable flags to settings storage.
    void saveState();
    //! Restore backend order/enable flags from settings storage.
    void restoreState();

    //! All supported file extensions across readers/decoders/archive readers.
    [[nodiscard]] QStringList supportedFileExtensions() const;
    //! Extensions that can be decoded as playable tracks.
    [[nodiscard]] QStringList supportedTrackExtensions() const;
    //! Extensions that are archive containers.
    [[nodiscard]] QStringList supportedArchiveExtensions() const;
    //! True when track has a writable reader backend.
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

    //! Register decoder backend with optional priority and archive-wrapper flag.
    void addDecoder(const QString& name, const DecoderCreator& creator, int priority = -1,
                    bool isArchiveWrapper = false);
    //! Register metadata reader backend with optional priority.
    void addReader(const QString& name, const ReaderCreator& creator, int priority = -1, bool isArchiveWrapper = false);
    //! Register archive reader backend with optional priority.
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

    //! Reset backend registry and runtime state.
    void reset();

private:
    std::unique_ptr<AudioLoaderPrivate> p;
};
} // namespace Fooyin
