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

#include <core/engine/audioinput.h>
#include <core/engine/audioloader.h>

#include <QIODevice>

namespace Fooyin {
class ArchiveDecoder : public AudioDecoder
{
public:
    explicit ArchiveDecoder(std::shared_ptr<AudioLoader> audioLoader);
    ~ArchiveDecoder() override;

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool isSeekable() const override;
    [[nodiscard]] bool trackHasChanged() const override;
    [[nodiscard]] Track changedTrack() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void start() override;
    void stop() override;

    void seek(uint64_t pos) override;

    AudioBuffer readBuffer(size_t bytes) override;

private:
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::unique_ptr<QIODevice> m_device;
    AudioDecoder* m_decoder;
};

class GeneralArchiveReader : public AudioReader
{
public:
    explicit GeneralArchiveReader(std::shared_ptr<AudioLoader> audioLoader);
    ~GeneralArchiveReader() override;

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;
    [[nodiscard]] int subsongCount() const override;

    bool init(const AudioSource& source) override;

    [[nodiscard]] bool readTrack(const AudioSource& source, Track& track) override;
    [[nodiscard]] QByteArray readCover(const AudioSource& source, const Track& track, Track::Cover cover) override;
    [[nodiscard]] bool writeTrack(const AudioSource& source, const Track& track, WriteOptions options) override;

private:
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::unique_ptr<QIODevice> m_device;
    ArchiveReader* m_archiveReader;
    AudioReader* m_reader;
};
} // namespace Fooyin
