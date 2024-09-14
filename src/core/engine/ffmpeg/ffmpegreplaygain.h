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

#include "ffmpeginput.h"

#include <QFile>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(REPLAYGAIN)

namespace Fooyin {
struct ReplayGainFilter;

struct ReplayGainResult
{
    double peak;
    double gain;
};

class FYCORE_EXPORT FFmpegReplayGain : public Worker
{
    Q_OBJECT

public:
    explicit FFmpegReplayGain(MusicLibrary* library, QObject* parent = nullptr);
    void calculate(const TrackList& tracks, bool asAlbum);

signals:
    void calculationFinished();

private:
    bool setupTrack(const Track& track, ReplayGainFilter& filter);
    ReplayGainResult handleTrack(const Track& track, bool inAlbum);
    void handleAlbum(const TrackList& album);

    std::unique_ptr<QIODevice> m_file;
    std::unique_ptr<FFmpegDecoder> m_decoder;
    AudioFormat m_format;

    MusicLibrary* m_library;
};
} // namespace Fooyin
