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

#include <core/library/playbackstatistics.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPromise>

namespace Fooyin {
class StatisticsProgress
{
public:
    explicit StatisticsProgress(QPromise<PlaybackStatisticsResult>* promise);

    void start(const QString& text, int maximum = 0);
    void update(int value);

private:
    QPromise<PlaybackStatisticsResult>* m_promise;
    QString m_text;
    int m_maximum;
    QElapsedTimer m_timer;
};

class PlaybackStatistics
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::PlaybackStatistics)

public:
    static QString validateScript(const QString& script);
    static QString evaluateKey(const PlaybackStatisticsConfig& config, const Track& track, bool* fallback = nullptr);
    static Track combineStatistics(const TrackList& tracks, const PlaybackStatisticsConfig& config);
};
} // namespace Fooyin
