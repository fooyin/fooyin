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
#include "waveformdata.h"
#include "waveformgenerator.h"
#include "waveformrescaler.h"

#include <core/track.h>

#include <QObject>
#include <QThread>

namespace Fooyin {
class AudioBuffer;
class DecoderProvider;
class SettingsManager;

namespace WaveBar {
class WaveformBuilder : public QObject
{
    Q_OBJECT

public:
    explicit WaveformBuilder(std::shared_ptr<DecoderProvider> decoderProvider, DbConnectionPoolPtr dbPool,
                             SettingsManager* settings, QObject* parent = nullptr);
    ~WaveformBuilder() override;

    void generate(const Track& track, bool update = false);
    void generateAndScale(const Track& track, bool update = false);
    void rescale(int width);

signals:
    void generatingWaveform();
    void waveformGenerated();
    void waveformRescaled(const WaveformData<float>& data);

private:
    void updateRescaler();

    SettingsManager* m_settings;

    QThread m_generatorThread;
    QThread m_rescalerThread;

    WaveformGenerator m_generator;
    WaveformRescaler m_rescaler;

    int m_width;
    int m_samplesPerChannel;
    bool m_rescale;
};
} // namespace WaveBar
} // namespace Fooyin
