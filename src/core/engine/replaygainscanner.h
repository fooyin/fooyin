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

#include "fycore_export.h"

#include <core/track.h>
#include <utils/worker.h>

#include <QObject>
#include <QThread>

namespace Fooyin {
class SettingsManager;

class FYCORE_EXPORT ReplayGainWorker : public Worker
{
    Q_OBJECT

public:
    explicit ReplayGainWorker(QObject* parent = nullptr);

    virtual void calculatePerTrack(const TrackList& tracks)    = 0;
    virtual void calculateAsAlbum(const TrackList& tracks)     = 0;
    virtual void calculateByAlbumTags(const TrackList& tracks) = 0;

signals:
    void startingCalculation(const QString& filepath);
    void calculationFinished(const TrackList& tracks);
};

class FYCORE_EXPORT ReplayGainScanner : public QObject
{
    Q_OBJECT

public:
    explicit ReplayGainScanner(SettingsManager* settings, QObject* parent = nullptr);
    ~ReplayGainScanner() override;

    void calculatePerTrack(const TrackList& tracks);
    void calculateAsAlbum(const TrackList& tracks);
    void calculateByAlbumTags(const TrackList& tracks);

signals:
    void startingCalculation(const QString& filepath);
    void calculationFinished(const TrackList& tracks);

private:
    SettingsManager* m_settings;
    QThread m_scanThread;
    std::unique_ptr<ReplayGainWorker> m_worker;
};
} // namespace Fooyin
