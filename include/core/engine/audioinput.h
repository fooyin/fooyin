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

#include <core/engine/audiobuffer.h>
#include <core/track.h>

#include <QIODevice>
#include <QObject>

#include <memory>

namespace Fooyin {
class AudioDecoderPrivate;
class ArchiveReader;

/*!
 * Input source descriptor passed to readers/decoders.
 *
 * `filepath` is always set. `device` is set when caller already opened the
 * source and wants backend code to read from that handle. `archiveReader` is
 * only present for tracks coming from archives.
 */
struct AudioSource
{
    //! Source path (always set).
    QString filepath;
    //! Already-open source device.
    QIODevice* device{nullptr};
    //! Optional archive helper when source originates from an archive container.
    ArchiveReader* archiveReader{nullptr};
};

/*!
 * Decoder interface used by playback pipeline.
 *
 * Lifecycle:
 * 1. `init()` once per track/subsong
 * 2. optional `start()`
 * 3. repeated `readBuffer()`
 * 4. optional `seek()`
 * 5. `stop()` for teardown/reset
 *
 * Runtime playback policy is provided via `PlaybackHints`, which can be
 * injected before and during playback by the engine.
 */
class FYCORE_EXPORT AudioDecoder
{
    Q_GADGET

public:
    enum DecoderFlag : uint8_t
    {
        None = 0,
        //! Disable decoder seeking.
        NoSeeking = 1 << 0,
        //! Disable all loop/repeat behaviour.
        NoLooping = 1 << 1,
        //! Replace infinite looping with backend/default bounded looping.
        NoInfiniteLooping = 1 << 2,
        //! Allow decoder to update `Track` metadata after `init()`.
        //! Useful when duration/fields depend on decoder options.
        UpdateTracks = 1 << 3,
    };
    Q_DECLARE_FLAGS(DecoderOptions, DecoderFlag)
    Q_FLAG(DecoderOptions)

    enum PlaybackHint : uint8_t
    {
        NoHints = 0,
        //! Current playback session is in repeat-track mode.
        RepeatTrackEnabled = 1 << 0,
    };
    Q_DECLARE_FLAGS(PlaybackHints, PlaybackHint)
    Q_FLAG(PlaybackHints)

    AudioDecoder();
    virtual ~AudioDecoder();

    /*!
     * Returns a list of file extensions this decoder supports.
     * i.e. "flac,mp3"
     */
    [[nodiscard]] virtual QStringList extensions() const = 0;
    /*!
     * Returns @c true if track is seekable.
     * @note Called only after `init()` succeeds.
     */
    [[nodiscard]] virtual bool isSeekable() const = 0;
    /*!
     * Returns @c true if the current track is being repeated/looped forever.
     * @note Called only after `init()` succeeds.
     */
    [[nodiscard]] bool isRepeatingTrack() const;
    /*!
     * Returns @c true if the track passed to `init()` has changed in some way.
     * Useful for properties like duration which may change due to loop count.
     * @note Called only after `init()` succeeds.
     */
    [[nodiscard]] virtual bool trackHasChanged() const;
    /*!
     * Returns the changed track.
     * @note Called only when `trackHasChanged()` returns true.
     */
    [[nodiscard]] virtual Track changedTrack() const;
    /*!
     * Returns the current variable/dynamic bitrate.
     * @note this should return 0 if the file isn't encoded with VBR.
     * @note the base class implementation of this function returns 0.
     */
    [[nodiscard]] virtual int bitrate() const;

    /*!
     * Returns current playback hint flags for this decoder instance.
     */
    [[nodiscard]] PlaybackHints playbackHints() const;
    /*!
     * Updates playback hint flags for this decoder instance.
     */
    void setPlaybackHints(PlaybackHints hints);

    /*!
     * Initialise decoder for `track` and `source`.
     * Called once per track/subsong before decoding starts.
     * @returns a valid AudioFormat if track can be decoded.
     */
    virtual std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) = 0;
    /*!
     * Optional start hook after `init()`.
     * Base class implementation does nothing.
     */
    virtual void start();
    /*!
     * Stop and deinitialise decoder state.
     * Should reset to pre-`init()` state.
     * Called on playback stop and track changes.
     */
    virtual void stop() = 0;

    /*!
     * Seek to `pos` milliseconds in current stream.
     */
    virtual void seek(uint64_t pos) = 0;

    /*!
     * Read up to `bytes` of interleaved PCM in the format returned by `init()`.
     */
    virtual AudioBuffer readBuffer(size_t bytes) = 0;

private:
    std::unique_ptr<AudioDecoderPrivate> p;
};
using DecoderCreator = std::function<std::unique_ptr<AudioDecoder>()>;

/*!
 * Metadata/tag reader/writer interface.
 */
class FYCORE_EXPORT AudioReader
{
    Q_GADGET

public:
    enum WriteFlag : uint8_t
    {
        Metadata = 0,
        //! Persist rating field when supported.
        Rating = 1 << 0,
        //! Persist play count field when supported.
        Playcount = 1 << 1,
        //! Preserve file timestamps (atime/mtime).
        PreserveTimestamps = 1 << 2,
    };
    Q_DECLARE_FLAGS(WriteOptions, WriteFlag)
    Q_FLAG(WriteOptions)

    virtual ~AudioReader() = default;

    /*!
     * Returns a list of file extensions this reader supports.
     * i.e. "flac,mp3"
     */
    [[nodiscard]] virtual QStringList extensions() const = 0;
    //! True when embedded cover art can be read.
    [[nodiscard]] virtual bool canReadCover() const = 0;
    //! True when embedded cover art can be written.
    [[nodiscard]] virtual bool canWriteCover() const;
    //! True when metadata writing is supported.
    [[nodiscard]] virtual bool canWriteMetaData() const = 0;
    /*!
     * Returns the number of subsongs contained in a file.
     * Called after `init()` and before read/write operations.
     * Base class implementation returns `1` (no subsongs).
     */
    [[nodiscard]] virtual int subsongCount() const;
    /*!
     * Prepares the audio source @p source for reading.
     * If the source has subsongs, expose count via `subsongCount()`.
     * @returns true if source is supported and ready for reads.
     * @note Base class implementation returns true.
     */
    virtual bool init(const AudioSource& source);

    /*!
     * Reads metadata/tags for the given Track @p track.
     * Called only after successful `init()`.
     * @returns whether the track was read successfully.
     */
    [[nodiscard]] virtual bool readTrack(const AudioSource& source, Track& track) = 0;
    /*!
     * Reads embedded artwork for the given Track @p track.
     * Called only after successful `init()`.
     * @returns image data.
     * @note Base class implementation returns empty data.
     */
    [[nodiscard]] virtual QByteArray readCover(const AudioSource& source, const Track& track, Track::Cover cover);
    /*!
     * Writes the metadata/tags in the given Track @p track to file.
     * Called only after successful `init()`.
     * @returns whether the track was written successfully.
     */
    [[nodiscard]] virtual bool writeTrack(const AudioSource& source, const Track& track, WriteOptions options);
    /*!
     * Writes the cover for the given Track @p track to file.
     * Called only after successful `init()`.
     * @returns whether the cover was written successfully.
     */
    [[nodiscard]] virtual bool writeCover(const AudioSource& source, const Track& track, const TrackCovers& covers,
                                          WriteOptions options);
};
using ReaderCreator = std::function<std::unique_ptr<AudioReader>()>;

/*!
 * Reader interface for archive containers that expose files as virtual entries.
 */
class FYCORE_EXPORT ArchiveReader
{
public:
    using ReadEntryCallback = std::function<void(const QString&, QIODevice*)>;

    virtual ~ArchiveReader() = default;

    /*!
     * Returns a list of file extensions this reader supports.
     * i.e. "zip,7z"
     */
    [[nodiscard]] virtual QStringList extensions() const = 0;
    /*!
     * Returns the current file type.
     * i.e. "zip"
     * @note Called only after `init()` returns true.
     */
    [[nodiscard]] virtual QString type() const = 0;

    /*!
     * Prepares the file @p file for reading.
     * @returns true if file is supported and ready for access.
     */
    virtual bool init(const QString& file) = 0;
    /*!
     * Returns a QIODevice for the file within the archive at @p file.
     * If the file can't be found, this should return nullptr.
     * @note Called only after `init()` returns true.
     */
    virtual std::unique_ptr<QIODevice> entry(const QString& file) = 0;
    /*!
     * Reads all files in the archive.
     * The callback @p readEntry should be used to read each file in the archive.
     * @returns true if tracks were read successfully.
     * @note Called only after `init()` returns true.
     */
    virtual bool readTracks(ReadEntryCallback readEntry) = 0;
    /*!
     * Reads artwork within the archive for the given Track @p track.
     * @returns image data.
     * @note Called only after `init()` returns true.
     */
    virtual QByteArray readCover(const Track& track, Track::Cover cover) = 0;
};
using ArchiveReaderCreator = std::function<std::unique_ptr<ArchiveReader>()>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::AudioDecoder::DecoderOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::AudioDecoder::PlaybackHints)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::AudioReader::WriteOptions)
