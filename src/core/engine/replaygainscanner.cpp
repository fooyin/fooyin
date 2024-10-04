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

#include "replaygainscanner.h"

#include "ffmpeg/ffmpegreplaygain.h"

namespace Fooyin {
ReplayGainWorker::ReplayGainWorker(QObject* parent)
    : Worker{parent}
{ }

ReplayGainScanner::ReplayGainScanner(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_worker{std::make_unique<FFmpegReplayGain>(m_settings)}
{
    m_worker->moveToThread(&m_scanThread);
    m_scanThread.start();

    QObject::connect(m_worker.get(), &ReplayGainWorker::startingCalculation, this,
                     &ReplayGainScanner::startingCalculation);
    QObject::connect(m_worker.get(), &ReplayGainWorker::calculationFinished, this,
                     &ReplayGainScanner::calculationFinished);
}

ReplayGainScanner::~ReplayGainScanner()
{
    m_worker->closeThread();
    m_scanThread.quit();
    m_scanThread.wait();
}

void ReplayGainScanner::calculatePerTrack(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() { m_worker->calculatePerTrack(tracks); });
}

void ReplayGainScanner::calculateAsAlbum(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() { m_worker->calculateAsAlbum(tracks); });
}

void ReplayGainScanner::calculateByAlbumTags(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() { m_worker->calculateByAlbumTags(tracks); });
}
} // namespace Fooyin

#include "moc_replaygainscanner.cpp"
