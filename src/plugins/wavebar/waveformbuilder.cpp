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

#include "waveformbuilder.h"

#include <core/engine/audiodecoder.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

namespace Fooyin::WaveBar {
WaveformBuilder::WaveformBuilder(std::unique_ptr<AudioDecoder> decoder, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_generator{std::move(decoder)}
    , m_width{0}
{
    m_rescaler.changeDownmix(static_cast<DownmixOption>(m_settings->value<Settings::WaveBar::Downmix>()));

    m_generator.moveToThread(&m_generatorThread);
    m_rescaler.moveToThread(&m_rescalerThread);

    QObject::connect(&m_generator, &WaveformGenerator::generatingWaveform, this, &WaveformBuilder::generatingWaveform);
    QObject::connect(&m_generator, &WaveformGenerator::waveformGenerated, this, &WaveformBuilder::waveformGenerated);
    QObject::connect(&m_generator, &WaveformGenerator::waveformGenerated, &m_rescaler,
                     [this](const auto& data) { m_rescaler.rescale(data, m_width); });
    QObject::connect(&m_rescaler, &WaveformRescaler::waveformRescaled, this, &WaveformBuilder::waveformRescaled);

    m_settings->subscribe<Settings::WaveBar::Downmix>(this, [this](const int downMix) {
        QMetaObject::invokeMethod(&m_rescaler, "changeDownmix", Q_ARG(int, downMix));
    });

    m_generatorThread.start();
    m_rescalerThread.start();

    QMetaObject::invokeMethod(&m_generator, &Worker::initialiseThread);
}

WaveformBuilder::~WaveformBuilder()
{
    m_generator.stopThread();
    m_rescaler.stopThread();

    m_generatorThread.quit();
    m_generatorThread.wait();

    m_rescalerThread.quit();
    m_rescalerThread.wait();
}

void WaveformBuilder::generate(const Track& track)
{
    m_generator.stopThread();
    m_rescaler.stopThread();

    QMetaObject::invokeMethod(&m_generator, "generate", Q_ARG(const Track&, track));
}

void WaveformBuilder::rescale(const int width)
{
    if(std::exchange(m_width, width) != width) {
        QMetaObject::invokeMethod(&m_rescaler, [this]() { m_rescaler.rescale(m_width); });
    }
}
} // namespace Fooyin::WaveBar
