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
#include "rgscannerdefs.h"

#ifdef HAVE_EBUR128
#include "ebur128scanner.h"
#endif

#include <core/coresettings.h>

using namespace Qt::StringLiterals;

namespace Fooyin::RGScanner {
RGWorker::RGWorker(QObject* parent)
    : Worker{parent}
{ }

RGScanner::RGScanner(const std::shared_ptr<AudioLoader>& audioLoader, QObject* parent)
    : QObject{parent}
{
    const FySettings settings;
    const auto scanner = settings.value(ScannerOption, u"libebur128"_s).toString();

#ifdef HAVE_EBUR128
    if(scanner == "libebur128"_L1) {
        m_worker = std::make_unique<Ebur128Scanner>(audioLoader);
    }
    else {
        m_worker = std::make_unique<FFmpegScanner>();
    }
#else
    m_worker = std::make_unique<FFmpegScanner>();
#endif

    m_worker->moveToThread(&m_scanThread);
    m_scanThread.start();

    QObject::connect(m_worker.get(), &RGWorker::startingCalculation, this, &RGScanner::startingCalculation);
    QObject::connect(m_worker.get(), &RGWorker::calculationFinished, this, &RGScanner::calculationFinished);
    QObject::connect(m_worker.get(), &RGWorker::closed, this, &RGScanner::deleteLater);
}

RGScanner::~RGScanner()
{
    m_scanThread.quit();
    m_scanThread.wait();
}

void RGScanner::close()
{
    m_worker->closeThread();
}

QStringList RGScanner::scannerNames()
{
    QStringList scanners{
        u"FFmpeg"_s,
#ifdef HAVE_EBUR128
        u"libebur128"_s,
#endif
    };
    return scanners;
}

void RGScanner::calculatePerTrack(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() {
        const FySettings settings;
        m_worker->calculatePerTrack(tracks, settings.value(TruePeakSetting, false).toBool());
    });
}

void RGScanner::calculateAsAlbum(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() {
        const FySettings settings;
        m_worker->calculateAsAlbum(tracks, settings.value(TruePeakSetting, false).toBool());
    });
}

void RGScanner::calculateByAlbumTags(const TrackList& tracks)
{
    QMetaObject::invokeMethod(m_worker.get(), [this, tracks]() {
        const FySettings settings;
        m_worker->calculateByAlbumTags(
            tracks, settings.value(AlbumGroupScriptSetting, u"%albumartist% - %date% - %album%"_s).toString(),
            settings.value(TruePeakSetting, false).toBool());
    });
}
} // namespace Fooyin::RGScanner

#include "moc_rgscanner.cpp"
