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

struct LoadedSource
{
    //! Non-owning view passed into decoder/reader backends.
    AudioSource source;
    //! Owns the opened input device for `source.device`.
    std::unique_ptr<QIODevice> device;
    //! Owns the archive reader when `device` comes from an archive entry.
    std::unique_ptr<ArchiveReader> archiveReader;

    void rebind()
    {
        source.device        = device.get();
        source.archiveReader = archiveReader.get();
    }
};

struct LoadedDecoder
{
    //! Opened input source backing `decoder`.
    LoadedSource input;
    //! Format resolved during decoder initialisation, if any.
    std::optional<AudioFormat> format;
    //! Initialised decoder selected for the input source.
    std::unique_ptr<AudioDecoder> decoder;
};

struct LoadedReader
{
    //! Opened input source backing `reader`.
    LoadedSource input;
    //! Initialised metadata reader selected for the input source.
    std::unique_ptr<AudioReader> reader;
};

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
    //! True when `track` has a usable reader backend that supports metadata writes.
    [[nodiscard]] bool canWriteMetadata(const Track& track) const;

    //! True when `file` extension resolves to a registered archive reader.
    [[nodiscard]] bool isArchive(const QString& file) const;
    /*!
     * Open and initialise the highest-priority usable decoder for `track`.
     *
     * The returned bundle owns the selected decoder and any opened file or
     * archive state required to keep decoding alive. Returns an empty result
     * when no decoder can be initialised.
     */
    [[nodiscard]] LoadedDecoder loadDecoderForTrack(const Track& track,
                                                    AudioDecoder::DecoderOptions options = AudioDecoder::None,
                                                    AudioDecoder::PlaybackHints hints    = AudioDecoder::NoHints) const;
    /*!
     * Open and initialise the highest-priority usable metadata reader for `track`.
     *
     * The returned bundle owns the selected reader and any opened file or
     * archive state required to keep reading alive. Returns an empty result
     * when no reader can be initialised.
     */
    [[nodiscard]] LoadedReader loadReaderForTrack(const Track& track) const;
    /*!
     * Open and initialise the highest-priority usable decoder for an archive member.
     *
     * Candidate decoders are resolved from `track.pathInArchive()`, and the
     * returned bundle keeps the owning archive reader and entry device alive.
     * Returns an empty result when no decoder can be initialised.
     */
    [[nodiscard]] LoadedDecoder loadDecoderForArchiveTrack(const Track& track, AudioDecoder::DecoderOptions options,
                                                           AudioDecoder::PlaybackHints hints
                                                           = AudioDecoder::NoHints) const;
    /*!
     * Open and initialise the highest-priority usable metadata reader for an archive member.
     *
     * Candidate readers are resolved from `track.pathInArchive()`, and the
     * returned bundle keeps the owning archive reader and entry device alive.
     * Returns an empty result when no reader can be initialised.
     */
    [[nodiscard]] LoadedReader loadReaderForArchiveTrack(const Track& track) const;
    //! Create decoder candidates for `file`, ordered by configured priority.
    [[nodiscard]] std::vector<std::unique_ptr<AudioDecoder>> decodersForFile(const QString& file) const;
    //! Create decoder candidates for `track`, ordered by configured priority.
    [[nodiscard]] std::vector<std::unique_ptr<AudioDecoder>> decodersForTrack(const Track& track) const;
    //! Create metadata reader candidates for `file`, ordered by configured priority.
    [[nodiscard]] std::vector<std::unique_ptr<AudioReader>> readersForFile(const QString& file) const;
    //! Create metadata reader candidates for `track`, ordered by configured priority.
    [[nodiscard]] std::vector<std::unique_ptr<AudioReader>> readersForTrack(const Track& track) const;
    //! Create the highest-priority archive reader backend for `file`, if any.
    [[nodiscard]] std::unique_ptr<ArchiveReader> archiveReaderForFile(const QString& file) const;

    //! Read and apply metadata tags to `track`.
    [[nodiscard]] bool readTrackMetadata(Track& track) const;
    //! Read cover image from `track` source.
    [[nodiscard]] QByteArray readTrackCover(const Track& track, Track::Cover cover) const;
    //! Persist updated metadata tags for `track`.
    [[nodiscard]] bool writeTrackMetadata(const Track& track, AudioReader::WriteOptions options) const;
    //! Persist cover image updates for `track`.
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

    //! Enable/disable decoder backend by registered name.
    void setDecoderEnabled(const QString& name, bool enabled);
    //! Reorder decoder backend priority index.
    void changeDecoderIndex(const QString& name, int index);
    //! Enable/disable reader backend by registered name.
    void setReaderEnabled(const QString& name, bool enabled);
    //! Reorder reader backend priority index.
    void changeReaderIndex(const QString& name, int index);

    //! Re-query extension list from the decoder backend implementation.
    void reloadDecoderExtensions(const QString& name);
    //! Re-query extension list from the reader backend implementation.
    void reloadReaderExtensions(const QString& name);

    //! Reset backend registry and runtime state.
    void reset();

private:
    std::unique_ptr<AudioLoaderPrivate> p;
};
} // namespace Fooyin
