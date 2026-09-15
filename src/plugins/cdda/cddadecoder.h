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
 */

#pragma once

#include "cddadrivemanager.h"
#include "cddadrivesettings.h"

#include <core/engine/audioinput.h>

#include <QByteArray>

#include <memory>
#include <mutex>
#include <optional>

namespace Fooyin::Cdda {
class CddaDecoder : public AudioDecoder
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::Cdda::CddaDecoder)

public:
    explicit CddaDecoder(std::shared_ptr<CdDriveManager> driveManager,
                         std::shared_ptr<const CdDriveSettingsProvider> settingsProvider = {});
    ~CddaDecoder() override;

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] QStringList supportedSchemes() const override;
    [[nodiscard]] bool isSeekable() const override;
    [[nodiscard]] bool allowsConcurrentDecoding() const override;
    [[nodiscard]] int playbackPrebufferMs() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void stop() override;
    void seek(uint64_t pos) override;
    ReadResult readAudio(size_t bytes) override;
    AudioBuffer readBuffer(size_t bytes) override;
    [[nodiscard]] QStringList takeWarnings() override;

protected:
    void interruptRead() override;

private:
    [[nodiscard]] bool acquireLease();
    void releaseLease();
    void setReadError(CdError error);
    [[nodiscard]] uint64_t currentTimestamp() const;

    std::shared_ptr<CdDriveManager> m_driveManager;
    std::shared_ptr<const CdDriveSettingsProvider> m_settingsProvider;

    AudioFormat m_format;
    QString m_discId;
    QString m_observedDriveId;
    QString m_error;
    QStringList m_warnings;
    CdToc m_toc;
    CdTocTrack m_track;
    int m_subsong;
    int m_nextSector;
    uint64_t m_nextLogicalFrame;
    uint64_t m_consumedFrames;
    QByteArray m_buffer;
    qsizetype m_bufferPosition;
    std::optional<CdDriveLease> m_lease;
    std::unique_ptr<CddaSectorReader> m_sectorReader;

    mutable std::mutex m_sessionMutex;
    CdDriveSession* m_activeSession;

    bool m_initialised;
    bool m_readFailed;
    bool m_forConversion;
    int m_readOffsetFrames;
};
} // namespace Fooyin::Cdda
