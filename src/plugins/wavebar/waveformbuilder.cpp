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

#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <utility>

namespace Fooyin::WaveBar {
WaveformBuilder::WaveformBuilder(std::shared_ptr<AudioLoader> decoderProvider, DbConnectionPoolPtr dbPool,
                                 SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_generator{std::move(decoderProvider), std::move(dbPool)}
    , m_width{0}
    , m_samplesPerChannel{settings->value<Settings::WaveBar::NumSamples>()}
    , m_rescale{false}
{
    updateRescaler();

    m_generator.moveToThread(&m_generatorThread);
    m_rescaler.moveToThread(&m_rescalerThread);

    QObject::connect(&m_generator, &WaveformGenerator::generatingWaveform, this, &WaveformBuilder::generatingWaveform);
    QObject::connect(&m_generator, &WaveformGenerator::waveformGenerated, this, &WaveformBuilder::waveformGenerated);
    QObject::connect(&m_generator, &WaveformGenerator::waveformGenerated, &m_rescaler, [this](const auto& data) {
        if(m_rescale) {
            m_rescaler.rescale(data, m_width);
        }
    });
    QObject::connect(&m_rescaler, &WaveformRescaler::waveformRescaled, this, &WaveformBuilder::waveformRescaled);

    m_settings->subscribe<Settings::WaveBar::BarWidth>(this, &WaveformBuilder::updateRescaler);
    m_settings->subscribe<Settings::WaveBar::BarGap>(this, &WaveformBuilder::updateRescaler);
    m_settings->subscribe<Settings::WaveBar::Downmix>(this, &WaveformBuilder::updateRescaler);
    m_settings->subscribe<Settings::WaveBar::NumSamples>(this, [this](const int num) { m_samplesPerChannel = num; });

    m_generatorThread.start();
    m_rescalerThread.start();

    QMetaObject::invokeMethod(&m_generator, &Worker::initialiseThread);
}

WaveformBuilder::~WaveformBuilder()
{
    m_generator.closeThread();
    m_rescaler.closeThread();

    m_generatorThread.quit();
    m_generatorThread.wait();

    m_rescalerThread.quit();
    m_rescalerThread.wait();
}

void WaveformBuilder::generate(const Track& track, bool update)
{
    m_rescale = false;

    QMetaObject::invokeMethod(
        &m_generator, [this, track, update]() { m_generator.generate(track, m_samplesPerChannel, false, update); });
}

void WaveformBuilder::generateAndScale(const Track& track, bool update)
{
    m_generator.stopThread();
    m_rescaler.stopThread();
    m_rescale = true;

    QMetaObject::invokeMethod(
        &m_generator, [this, track, update]() { m_generator.generate(track, m_samplesPerChannel, true, update); });
}

void WaveformBuilder::rescale(const int width)
{
    if(std::exchange(m_width, width) != width) {
        QMetaObject::invokeMethod(&m_rescaler, [this]() { m_rescaler.rescale(m_width); });
    }
}

void WaveformBuilder::updateRescaler()
{
    m_rescaler.stopThread();
    QMetaObject::invokeMethod(&m_rescaler, [this]() {
        m_rescaler.changeSampleWidth(m_settings->value<Settings::WaveBar::BarWidth>()
                                     + m_settings->value<Settings::WaveBar::BarGap>());
        m_rescaler.changeDownmix(static_cast<DownmixOption>(m_settings->value<Settings::WaveBar::Downmix>()));
    });
}
} // namespace Fooyin::WaveBar
