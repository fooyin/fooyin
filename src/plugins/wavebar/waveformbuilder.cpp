/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

namespace Fooyin::WaveBar {
WaveformBuilder::WaveformBuilder(std::shared_ptr<AudioLoader> decoderProvider, DbConnectionPoolPtr dbPool,
                                 SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_generator{std::move(decoderProvider), std::move(dbPool)}
    , m_width{0}
    , m_samplesPerChannel{settings->value<Settings::WaveBar::NumSamples>()}
    , m_sampleWidth{1}
    , m_supersampleFactor{1}
    , m_downmix{false}
    , m_peakDisplayMode{PeakDisplayMode::Maximum}
    , m_normaliseToPeak{false}
    , m_decibelScale{false}
    , m_rescale{false}
{
    updateRescaler();

    m_generator.moveToThread(&m_generatorThread);
    m_rescaler.moveToThread(&m_rescalerThread);

    QObject::connect(&m_generator, &WaveformGenerator::generatingWaveform, this, &WaveformBuilder::generatingWaveform);
    QObject::connect(&m_generator, &WaveformGenerator::waveformGenerated, this,
                     [this](const Track& track, const auto& data, std::stop_token token) {
                         if(token.stop_requested()) {
                             return;
                         }
                         Q_EMIT waveformGenerated(track);
                         if(m_rescale && !token.stop_requested()) {
                             QMetaObject::invokeMethod(&m_rescaler, [worker = &m_rescaler, data, width = m_width,
                                                                     token = m_rescaler.prepareWork()] {
                                 if(worker->startWork(token)) {
                                     worker->rescale(data, width);
                                 }
                             });
                         }
                     });
    QObject::connect(&m_rescaler, &WaveformRescaler::waveformRescaled, this,
                     [this](const auto& data, std::stop_token token) {
                         if(!token.stop_requested()) {
                             Q_EMIT waveformRescaled(data);
                         }
                     });

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

    QMetaObject::invokeMethod(&m_generator, [worker = &m_generator, track, update, samples = m_samplesPerChannel,
                                             token = m_generator.prepareWork()]() {
        worker->generate(track, samples, false, update, token);
    });
}

void WaveformBuilder::generateAndScale(const Track& track, bool update)
{
    m_generator.stopThread();
    m_rescaler.stopThread();
    m_rescale = true;

    QMetaObject::invokeMethod(
        &m_generator, [worker = &m_generator, track, update, samples = m_samplesPerChannel,
                       token = m_generator.prepareWork()]() { worker->generate(track, samples, true, update, token); });
}

void WaveformBuilder::rescale(const int width)
{
    if(std::exchange(m_width, width) != width) {
        QMetaObject::invokeMethod(&m_rescaler,
                                  [worker = &m_rescaler, width = m_width, token = m_rescaler.prepareWork()] {
                                      if(worker->startWork(token)) {
                                          worker->rescale(width);
                                      }
                                  });
    }
}

void WaveformBuilder::setSampleWidth(int width)
{
    const int validatedWidth = std::max(1, width);
    if(std::exchange(m_sampleWidth, validatedWidth) != validatedWidth) {
        updateRescaler();
    }
}

void WaveformBuilder::setDownmix(DownmixOption option)
{
    if(std::exchange(m_downmix, option) != option) {
        updateRescaler();
    }
}

void WaveformBuilder::setSupersampleFactor(int factor)
{
    factor = std::max(1, factor);
    if(std::exchange(m_supersampleFactor, factor) != factor) {
        updateRescaler();
    }
}

void WaveformBuilder::setPeakDisplayMode(PeakDisplayMode mode)
{
    if(std::exchange(m_peakDisplayMode, mode) != mode) {
        updateRescaler();
    }
}

void WaveformBuilder::setNormaliseToPeak(bool normalise)
{
    if(std::exchange(m_normaliseToPeak, normalise) != normalise) {
        updateRescaler();
    }
}

void WaveformBuilder::setDecibelScale(bool decibelScale)
{
    if(std::exchange(m_decibelScale, decibelScale) != decibelScale) {
        updateRescaler();
    }
}

void WaveformBuilder::updateRescaler()
{
    m_rescaler.stopThread();
    QMetaObject::invokeMethod(&m_rescaler, [worker = &m_rescaler, sampleWidth = m_sampleWidth, downmix = m_downmix,
                                            supersampleFactor = m_supersampleFactor,
                                            peakDisplayMode = m_peakDisplayMode, normaliseToPeak = m_normaliseToPeak,
                                            decibelScale = m_decibelScale, token = m_rescaler.prepareWork()] {
        if(!worker->startWork(token)) {
            return;
        }
        worker->changeSampleWidth(sampleWidth);
        worker->changeDownmix(downmix);
        worker->changeSupersampleFactor(supersampleFactor);
        worker->changePeakDisplayMode(peakDisplayMode);
        worker->changeNormaliseToPeak(normaliseToPeak);
        worker->changeDecibelScale(decibelScale);
        worker->setState(Worker::Idle);
    });
}
} // namespace Fooyin::WaveBar
