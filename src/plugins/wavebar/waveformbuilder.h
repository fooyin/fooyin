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

#include "settings/wavebarsettings.h"
#include "wavebardatabase.h"
#include "waveformdata.h"

#include <core/engine/audiodecoder.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/worker.h>

#include <QObject>

namespace Fooyin {
class AudioBuffer;
class SettingsManager;

namespace WaveBar {
class WaveformBuilder : public Worker
{
    Q_OBJECT

public:
    explicit WaveformBuilder(std::unique_ptr<AudioDecoder> decoder, SettingsManager* settings,
                             QObject* parent = nullptr);

signals:
    void buildingWaveform();
    void waveformBuilt();
    void waveformRescaled(const WaveformData<float>& data);

public slots:
    void initialiseThread() override;

    void setWidth(int width);
    void rebuild(const Fooyin::Track& track);
    void rescale();

private:
    void processBuffer(const AudioBuffer& buffer);
    int buildSample(WaveformSample& sample, int channel, int sampleSize, double start, double end);

    std::unique_ptr<AudioDecoder> m_decoder;
    SettingsManager* m_settings;

    DbConnectionPoolPtr m_dbPool;
    std::unique_ptr<DbConnectionHandler> m_dbHandler;
    WaveBarDatabase m_waveDb;

    Track m_track;
    AudioFormat m_format;
    AudioFormat m_requiredFormat;
    DownmixOption m_downMix;

    WaveformData<float> m_data;

    int m_channels;
    uint64_t m_duration;
    int m_width;
};
} // namespace WaveBar
} // namespace Fooyin
