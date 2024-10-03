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

#include <core/library/musiclibrary.h>
#include <utils/worker.h>

#include <QFile>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(REPLAYGAIN)

namespace Fooyin {
class FFmpegReplayGainPrivate;
class SettingsManager;

class FYCORE_EXPORT FFmpegReplayGain : public Worker
{
    Q_OBJECT

public:
    explicit FFmpegReplayGain(MusicLibrary* library, SettingsManager* settings, QObject* parent = nullptr);
    ~FFmpegReplayGain() override;
    void calculate(const TrackList& tracks, bool asAlbum);

private:
    std::unique_ptr<FFmpegReplayGainPrivate> p;
};
} // namespace Fooyin
