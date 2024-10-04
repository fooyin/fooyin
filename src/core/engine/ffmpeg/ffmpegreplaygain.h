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

#include "engine/replaygainscanner.h"

#include <QFile>
#include <QLoggingCategory>

namespace Fooyin {
class FFmpegReplayGainPrivate;
class SettingsManager;

class FFmpegReplayGain : public ReplayGainWorker
{
    Q_OBJECT

public:
    explicit FFmpegReplayGain(SettingsManager* settings, QObject* parent = nullptr);
    ~FFmpegReplayGain() override;

    void calculatePerTrack(const TrackList& tracks) override;
    void calculateAsAlbum(const TrackList& tracks) override;
    void calculateByAlbumTags(const TrackList& tracks) override;

private:
    std::unique_ptr<FFmpegReplayGainPrivate> p;
};
} // namespace Fooyin
