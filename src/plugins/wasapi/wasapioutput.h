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

#include <core/engine/audiooutput.h>

#include <QString>
#include <QTimer>
#include <QWidget>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace Fooyin {
class WasapiOutput : public AudioOutput
{
    Q_OBJECT

public:
    WasapiOutput();
    ~WasapiOutput() override;

    bool init(const AudioFormat& format) override;
    void uninit() override;
    void reset() override;
    void start() override;
    void drain() override;

    bool initialised() const override;
    QString device() const override;
    [[nodiscard]] int bufferSize() const override;
    OutputState currentState() override;
    OutputDevices getAllDevices(bool isCurrentOutput) override;

    int write(const AudioBuffer& buffer) override;
    void setPaused(bool pause) override;
    void setVolume(double volume) override;
    void setDevice(const QString& device) override;

    QString error() const override;
    AudioFormat format() const override;

private:
    bool initDevice();
    bool initAudioClient();
    void uninitDevice();
    QString getDeviceName(IMMDevice* device);
    bool supportsExclusiveMode(IMMDevice* device);
    static WAVEFORMATEXTENSIBLE createWaveFormat(const AudioFormat& format);

    AudioFormat m_format;

    QString m_deviceId;
    QString m_error;
    double m_volume;

    // WASAPI COM objects
    ComPtr<IMMDeviceEnumerator> m_deviceEnumerator;
    ComPtr<IMMDevice> m_device;
    ComPtr<IAudioClient> m_audioClient;
    ComPtr<IAudioClock> m_audioClock;
    ComPtr<IAudioRenderClient> m_renderClient;
    ComPtr<ISimpleAudioVolume> m_audioVolume;

    // Audio processing
    int m_bufferSize;
    UINT32 m_bufferFrameCount;
    double m_latency;
    std::atomic<bool> m_initialised;
    std::atomic<bool> m_running;
    std::atomic<bool> m_draining;

    WAVEFORMATEXTENSIBLE m_waveFormat;
};
} // namespace Fooyin