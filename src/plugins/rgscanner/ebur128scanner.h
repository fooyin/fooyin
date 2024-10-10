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

#include "rgscanner.h"

#include <core/scripting/scriptparser.h>

#include <QFile>
#include <QFutureWatcher>

#include <ebur128.h>

namespace Fooyin::RGScanner {
class Ebur128Scanner : public RGWorker
{
    Q_OBJECT

public:
    explicit Ebur128Scanner(std::shared_ptr<AudioLoader> audioLoader, QObject* parent = nullptr);

    void closeThread() override;

    void calculatePerTrack(const TrackList& tracks, bool truePeak) override;
    void calculateAsAlbum(const TrackList& tracks, bool truePeak) override;
    void calculateByAlbumTags(const TrackList& tracks, const QString& groupScript, bool truePeak) override;

private:
    struct EburStateDeleter
    {
        void operator()(ebur128_state* state) const
        {
            if(state) {
                ebur128_destroy(&state);
            }
        }
    };
    using EburStatePtr = std::unique_ptr<ebur128_state, EburStateDeleter>;

    using Albums        = std::unordered_map<QString, TrackList>;
    using AlbumWatchers = std::unordered_map<QString, QFutureWatcher<void>*>;
    using AlbumStates   = std::unordered_map<QString, std::vector<EburStatePtr>>;

    void scanTrack(Track& track, bool truePeak, const QString& album = {});
    void scanAlbum(bool truePeak);

    std::shared_ptr<AudioLoader> m_audioLoader;
    ScriptParser m_parser;

    TrackList m_tracks;
    TrackList m_scannedTracks;

    Albums m_albums;
    Albums::iterator m_currentAlbum;
    AlbumWatchers m_albumWatchers;
    AlbumStates m_albumStates;

    QFutureWatcher<void>* m_watcher;
    AudioDecoder* m_decoder;
    AudioFormat m_format;

    std::mutex m_mutex;
    std::atomic_int m_runningWatchers;
};
} // namespace Fooyin::RGScanner
