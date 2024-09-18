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

namespace Fooyin {
class ArchiveReader;

struct AudioSource
{
    // Filepath used to open device
    QString filepath;
    // An open device
    QIODevice* device{nullptr};
    // Only valid if file is in an archive. Useful for retrieving files in same archive.
    ArchiveReader* archiveReader{nullptr};
};

class FYCORE_EXPORT AudioDecoder
{
    Q_GADGET

public:
    enum DecoderFlag : uint8_t
    {
        None = 0,
        // Decoder will never call seek
        NoSeeking = 1 << 0,
        // Disable all looping
        NoLooping = 1 << 1,
        // If decoder is set to infinitely loop, use a default loop count instead.
        NoInfiniteLooping = 1 << 2,
        // Support updating tracks on init.
        // Useful if properties like loop count can change duration.
        UpdateTracks = 1 << 3,
    };
    Q_DECLARE_FLAGS(DecoderOptions, DecoderFlag)
    Q_FLAG(DecoderOptions)

    virtual ~AudioDecoder() = default;

    /*!
     * Returns a list of file extensions this decoder supports.
     * i.e. "flac,mp3"
     */
    [[nodiscard]] virtual QStringList extensions() const = 0;
    /*!
     * Returns @c true if track is seekable.
     * @note this will only be called after a valid AudioFormat is returned from @fn init.
     */
    [[nodiscard]] virtual bool isSeekable() const = 0;
    /*!
     * Returns @c true if the track passed to @fn init has changed in some way.
     * Useful for properties like duration which may change due to loop count.
     * @note this will only be called after a valid AudioFormat is returned from @fn init.
     */
    [[nodiscard]] virtual bool trackHasChanged() const;
    /*!
     * Returns the changed track.
     * @note this will only be called if @fn trackHasChanged returns @c true.
     */
    [[nodiscard]] virtual Track changedTrack() const;
    /*!
     * Returns the current variable/dynamic bitrate.
     * @note this should return 0 if the file isn't encoded with VBR.
     * @note the base class implementation of this function returns 0.
     */
    [[nodiscard]] virtual int bitrate() const;

    /*!
     * Setup the decoder for the given Track @p track.
     * Will be called for every track (including subsongs) before decoding starts.
     * @returns a valid AudioFormat if track can be decoded.
     */
    virtual std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) = 0;
    /*!
     * Start decoding the track passed to @fn init.
     * @note the base class implementation of this function does nothing.
     */
    virtual void start();
    /*!
     * Stop and deinit the decoder.
     * Should reset the decoder to a state prior to an @fn init call.
     * @note this will be called on playback stop and when track is changed.
     */
    virtual void stop() = 0;

    /*!
     * Seek to the given position @p pos in milliseconds.
     * @see AudioFormat for converting @p pos to a sample or byte count.
     */
    virtual void seek(uint64_t pos) = 0;

    /*!
     * Read a buffer interleaved audio data of size @p bytes.
     * Audio should be in the format returned by @fn init.
     * @see AudioFormat for converting @p bytes to a sample or byte count.
     */
    virtual AudioBuffer readBuffer(size_t bytes) = 0;
};
using DecoderCreator = std::function<std::unique_ptr<AudioDecoder>()>;

class FYCORE_EXPORT AudioReader
{
    Q_GADGET

public:
    enum WriteFlag : uint8_t
    {
        Metadata = 0,
        // Write rating to file (if supported)
        Rating = 1 << 0,
        // Write playcount to file (if supported)
        Playcount = 1 << 1,
    };
    Q_DECLARE_FLAGS(WriteOptions, WriteFlag)
    Q_FLAG(WriteOptions)

    virtual ~AudioReader() = default;

    /*!
     * Returns a list of file extensions this reader supports.
     * i.e. "flac,mp3"
     */
    [[nodiscard]] virtual QStringList extensions() const = 0;
    /* Returns @c true if embedded album artwork can be read. */
    [[nodiscard]] virtual bool canReadCover() const = 0;
    /* Returns @c true if this reader supports writing metadata/tags to file. */
    [[nodiscard]] virtual bool canWriteMetaData() const = 0;
    /*!
     * Returns the number of subsongs contained in a file.
     * Called after @fn init and before any @fn readTrack, @fn readCover or @fn writeTrack calls.
     * @note the base class implementation of this function returns 1/no subsongs.
     */
    [[nodiscard]] virtual int subsongCount() const;

    /*!
     * Prepares the audio source @p source for reading.
     * If a track can have subsongs, the subsong count should be set here.
     * @returns true if init was successful/file is supported.
     * @note the base class implementation of this function returns @c true.
     */
    virtual bool init(const AudioSource& source);

    /*!
     * Reads metadata/tags for the given Track @p track.
     * Will only be called after a successful @fn init call.
     * @returns whether the track was read successfully.
     */
    [[nodiscard]] virtual bool readTrack(const AudioSource& source, Track& track) = 0;
    /*!
     * Reads embedded artwork for the given Track @p track.
     * Will only be called after a successful @fn init call.
     * @returns the image data as a bytearray.
     * @note the base class implementation of this function returns nothing.
     */
    [[nodiscard]] virtual QByteArray readCover(const AudioSource& source, const Track& track, Track::Cover cover);
    /*!
     * Writes the metadata/tags in the given Track @p track to file.
     * Will only be called after a successful @fn init call.
     * @returns whether the track was written successfully.
     */
    [[nodiscard]] virtual bool writeTrack(const AudioSource& source, const Track& track, WriteOptions options);
};
using ReaderCreator = std::function<std::unique_ptr<AudioReader>()>;

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
     * @note this will only be called after @fn init returns @c true.
     */
    [[nodiscard]] virtual QString type() const = 0;

    /*!
     * Prepares the file @p file for reading.
     * If a track can have subsongs, the subsong count should be set here.
     * @returns true if init was successful/file is supported.
     */
    virtual bool init(const QString& file) = 0;
    /*!
     * Returns a QIODevice for the file within the archive at @p file.
     * If the file can't be found, this should return nullptr.
     * @note this will only be called after @fn init returns @c true.
     */
    virtual std::unique_ptr<QIODevice> entry(const QString& file) = 0;
    /*!
     * Reads all files in the archive.
     * The callback @p readEntry should be used to read each file in the archive.
     * @returns true if tracks were read successfully.
     * @note this will only be called after @fn init returns @c true.
     */
    virtual bool readTracks(ReadEntryCallback readEntry) = 0;
    /*!
     * Reads artwork within the archive for the given Track @p track.
     * @returns the image data as a bytearray.
     * @note this will only be called after @fn init returns @c true.
     * @note the base class implementation of this function returns nothing.
     */
    virtual QByteArray readCover(const Track& track, Track::Cover cover) = 0;
};
using ArchiveReaderCreator = std::function<std::unique_ptr<ArchiveReader>()>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::AudioDecoder::DecoderOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::AudioReader::WriteOptions)
