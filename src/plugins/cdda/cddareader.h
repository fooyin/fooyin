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

#include <core/engine/audioinput.h>

#include <expected>
#include <memory>
#include <vector>

namespace Fooyin::Cdda {
[[nodiscard]] std::expected<TrackList, QString> tracksForDisc(const CdToc& toc, const QString& discId,
                                                              const CdText& cdText = {});

class CddaReader : public AudioReader
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::Cdda::CddaReader)

public:
    explicit CddaReader(std::shared_ptr<CdDriveManager> driveManager);

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] QStringList supportedSchemes() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;
    [[nodiscard]] int subsongCount() const override;

    bool init(const AudioSource& source) override;
    [[nodiscard]] bool readTrack(const AudioSource& source, Track& track) override;

    [[nodiscard]] QString lastError() const;

private:
    void clear();

    std::shared_ptr<CdDriveManager> m_driveManager;
    QString m_sourcePath;
    QString m_discId;
    QString m_error;
    CdToc m_toc;
    CdText m_cdText;
    std::vector<CdTocTrack> m_audioTracks;
};
} // namespace Fooyin::Cdda
