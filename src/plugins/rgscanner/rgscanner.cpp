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

#include "rgscanner.h"

#include "ffmpegscanner.h"

#include <core/coresettings.h>
#include <utils/settings/settingsmanager.h>

namespace Fooyin::RGScanner {
RGWorker::RGWorker(QObject* parent)
    : Worker{parent}
{ }

RGScanner::RGScanner(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_worker{std::make_unique<FFmpegScanner>()}
{
    m_worker->moveToThread(&m_scanThread);
    m_scanThread.start();

    QObject::connect(m_worker.get(), &RGWorker::startingCalculation, this, &RGScanner::startingCalculation);
    QObject::connect(m_worker.get(), &RGWorker::calculationFinished, this, &RGScanner::calculationFinished);
}

RGScanner::~RGScanner()
{
    m_worker->closeThread();
    m_scanThread.quit();
    m_scanThread.wait();
}

void RGScanner::calculatePerTrack(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() {
        m_worker->calculatePerTrack(tracks, m_settings->value<Settings::Core::RGTruePeak>());
    });
}

void RGScanner::calculateAsAlbum(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() {
        m_worker->calculateAsAlbum(tracks, m_settings->value<Settings::Core::RGTruePeak>());
    });
}

void RGScanner::calculateByAlbumTags(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() {
        m_worker->calculateByAlbumTags(tracks, m_settings->value<Settings::Core::RGAlbumGroupScript>(),
                                       m_settings->value<Settings::Core::RGTruePeak>());
    });
}
} // namespace Fooyin::RGScanner

#include "moc_rgscanner.cpp"
