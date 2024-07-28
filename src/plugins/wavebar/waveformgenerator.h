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

#include "wavebardatabase.h"

#include <core/engine/audioinput.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/worker.h>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(WAVEBAR)

namespace Fooyin {
class AudioLoader;

namespace WaveBar {
class WaveformGenerator : public Worker
{
    Q_OBJECT

public:
    explicit WaveformGenerator(std::shared_ptr<AudioLoader> audioLoader, DbConnectionPoolPtr dbPool,
                               QObject* parent = nullptr);

signals:
    void generatingWaveform();
    void waveformGenerated(const Fooyin::WaveBar::WaveformData<float>& data);

public slots:
    void initialiseThread() override;
    void generate(const Fooyin::Track& track, int samplesPerChannel, bool update = false);
    void generateAndRender(const Fooyin::Track& track, int samplesPerChannel, bool update = false);

private:
    QString setup(const Track& track, int samplesPerChannel);
    void processBuffer(const AudioBuffer& buffer);

    std::shared_ptr<AudioLoader> m_audioLoader;
    AudioDecoder* m_decoder;
    DbConnectionPoolPtr m_dbPool;
    std::unique_ptr<DbConnectionHandler> m_dbHandler;
    WaveBarDatabase m_waveDb;

    Track m_track;
    AudioFormat m_format;
    AudioFormat m_requiredFormat;
    int m_samplesPerChannel;
    WaveformData<float> m_data;
};
} // namespace WaveBar
} // namespace Fooyin
