/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <memory>

namespace Fooyin {
class CarrierDecoderPrivate;

class FYCORE_EXPORT CarrierDecoder final : public AudioDecoder
{
public:
    explicit CarrierDecoder(std::unique_ptr<AudioDecoder> decoder);
    ~CarrierDecoder() override;

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] QStringList preferredExtensions() const override;
    [[nodiscard]] QStringList supportedSchemes() const override;
    [[nodiscard]] bool supportsRemoteSources() const override;
    [[nodiscard]] bool needsMoreInput() const override;
    [[nodiscard]] bool isSeekable() const override;
    [[nodiscard]] bool allowsConcurrentDecoding() const override;
    [[nodiscard]] int playbackPrebufferMs() const override;
    [[nodiscard]] QStringList takeWarnings() override;
    [[nodiscard]] RepeatHandling repeatHandling() const override;
    [[nodiscard]] bool trackHasChanged() const override;
    [[nodiscard]] Track changedTrack() const override;
    [[nodiscard]] std::optional<TimedTrackChange> takeTimedTrackChange() override;
    [[nodiscard]] int bitrate() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void start() override;
    void stop() override;
    void seek(uint64_t pos) override;
    ReadResult readAudio(size_t bytes) override;
    AudioBuffer readBuffer(size_t bytes) override;

protected:
    void playbackHintsChanged(PlaybackHints hints) override;
    void interruptRead() override;

private:
    std::unique_ptr<CarrierDecoderPrivate> p;
};
} // namespace Fooyin
