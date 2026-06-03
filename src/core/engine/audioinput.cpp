/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/engine/audioinput.h>

#include <array>
#include <utility>

namespace Fooyin {
class AudioDecoderPrivate
{
public:
    AudioDecoder::PlaybackHints playbackHints{AudioDecoder::NoHints};
};

AudioDecoder::AudioDecoder()
    : p{std::make_unique<AudioDecoderPrivate>()}
{ }

AudioDecoder::~AudioDecoder() = default;

QStringList AudioDecoder::preferredExtensions() const
{
    return {};
}

bool AudioDecoder::supportsRemoteSources() const
{
    return false;
}

bool AudioDecoder::needsMoreInput() const
{
    return false;
}

AudioDecoder::PlaybackHints AudioDecoder::playbackHints() const
{
    return p->playbackHints;
}

void AudioDecoder::setPlaybackHints(PlaybackHints hints)
{
    p->playbackHints = hints;
}

bool AudioDecoder::isRepeatingTrack() const
{
    return playbackHints().testFlag(RepeatTrackEnabled);
}

bool AudioDecoder::trackHasChanged() const
{
    return false;
}

Track AudioDecoder::changedTrack() const
{
    return {};
}

int AudioDecoder::bitrate() const
{
    return 0;
}

void AudioDecoder::start() { }

AudioDecoder::ReadResult AudioDecoder::readAudio(size_t bytes)
{
    AudioBuffer buffer = readBuffer(bytes);
    if(buffer.isValid()) {
        return ReadResult::data(std::move(buffer));
    }
    if(needsMoreInput()) {
        return ReadResult::needMoreInput();
    }
    return ReadResult::endOfStream();
}

QStringList AudioReader::preferredExtensions() const
{
    return {};
}

bool AudioReader::supportsRemoteSources() const
{
    return false;
}

bool AudioReader::canWriteCover() const
{
    return canWriteMetaData();
}

int AudioReader::subsongCount() const
{
    return 1;
}

bool AudioReader::init(const AudioSource& /*source*/)
{
    return true;
}

QByteArray AudioReader::readCover(const AudioSource& /*source*/, const Track& /*track*/, Track::Cover /*cover*/)
{
    return {};
}

bool AudioReader::writeTrack(const AudioSource& /*source*/, const Track& /*track*/, WriteOptions /*options*/)
{
    return false;
}

bool AudioReader::writeCover(const AudioSource& /*source*/, const Track& /*track*/, const TrackCovers& /*covers*/,
                             WriteOptions /*options*/)
{
    return false;
}

bool ArchiveReader::copyEntryToDevice(const QString& file, QIODevice* device,
                                      const ShouldContinueCallback& shouldContinue)
{
    if(!device || !device->isWritable()) {
        return false;
    }

    ArchiveEntryData entryData = entry(file);
    if(!entryData.device) {
        return false;
    }

    std::array<char, 64UL * 1024> buffer{};
    while(shouldContinue()) {
        const qint64 read = entryData.device->read(buffer.data(), buffer.size());
        if(read == 0) {
            return true;
        }
        if(read < 0) {
            return false;
        }

        qint64 writtenTotal{0};
        while(writtenTotal < read) {
            if(!shouldContinue()) {
                return false;
            }
            const qint64 written = device->write(buffer.data() + writtenTotal, read - writtenTotal);
            if(written <= 0) {
                return false;
            }
            writtenTotal += written;
        }
    }

    return false;
}

bool ArchiveReader::readEntries(const ReadEntryInfoCallback& readEntry)
{
    bool keepReading{true};
    return readTracks([&readEntry, &keepReading](ArchiveEntryData&& entryData) {
        if(!readEntry || !keepReading) {
            return;
        }
        keepReading = readEntry(entryData.info);
    });
}
} // namespace Fooyin
