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

#define NOMINMAX
#include "wasapioutput.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audioformat.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QThread>

#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

Q_LOGGING_CATEGORY(WASAPI, "WASAPI")

using namespace Qt::StringLiterals;

namespace {
QString resultToError(const HRESULT& result)
{
    switch(result) {
        case(AUDCLNT_E_NOT_INITIALIZED):
            return u"Audio client not initialized"_s;
        case(AUDCLNT_E_ALREADY_INITIALIZED):
            return u"Audio client already initialized"_s;
        case(AUDCLNT_E_WRONG_ENDPOINT_TYPE):
            return u"Wrong endpoint type"_s;
        case(AUDCLNT_E_DEVICE_INVALIDATED):
            return u"Device invalidated"_s;
        case(AUDCLNT_E_NOT_STOPPED):
            return u"Audio client not stopped"_s;
        case(AUDCLNT_E_BUFFER_TOO_LARGE):
            return u"Buffer too large"_s;
        case(AUDCLNT_E_OUT_OF_ORDER):
            return u"Out of order operation"_s;
        case(AUDCLNT_E_UNSUPPORTED_FORMAT):
            return u"Unsupported format"_s;
        case(AUDCLNT_E_INVALID_SIZE):
            return u"Invalid buffer size"_s;
        case(AUDCLNT_E_DEVICE_IN_USE):
            return u"Device in use"_s;
        case(AUDCLNT_E_BUFFER_OPERATION_PENDING):
            return u"Buffer operation pending"_s;
        case(AUDCLNT_E_THREAD_NOT_REGISTERED):
            return u"Thread not registered"_s;
        case(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED):
            return u"Exclusive mode not allowed"_s;
        case(AUDCLNT_E_ENDPOINT_CREATE_FAILED):
            return u"Endpoint creation failed"_s;
        case(AUDCLNT_E_SERVICE_NOT_RUNNING):
            return u"Audio service not running"_s;
        case(AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED):
            return u"Event handle not expected"_s;
        case(AUDCLNT_E_EXCLUSIVE_MODE_ONLY):
            return u"Exclusive mode only"_s;
        case(AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL):
            return u"Buffer duration period mismatch"_s;
        case(AUDCLNT_E_EVENTHANDLE_NOT_SET):
            return u"Event handle not set"_s;
        case(AUDCLNT_E_INCORRECT_BUFFER_SIZE):
            return u"Incorrect buffer size"_s;
        case(AUDCLNT_E_BUFFER_SIZE_ERROR):
            return u"Buffer size error"_s;
        case(AUDCLNT_E_CPUUSAGE_EXCEEDED):
            return u"CPU usage exceeded"_s;
        case(AUDCLNT_E_BUFFER_ERROR):
            return u"Buffer error"_s;
        case(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED):
            return u"Buffer size not aligned"_s;
        case(AUDCLNT_E_INVALID_DEVICE_PERIOD):
            return u"Invalid device period"_s;
        case(AUDCLNT_E_INVALID_STREAM_FLAG):
            return u"Invalid stream flag"_s;
        case(AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE):
            return u"Endpoint offload not capable"_s;
        case(AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES):
            return u"Out of offload resources"_s;
        case(AUDCLNT_E_OFFLOAD_MODE_ONLY):
            return u"Offload mode only"_s;
        case(AUDCLNT_E_NONOFFLOAD_MODE_ONLY):
            return u"Non-offload mode only"_s;
        case(AUDCLNT_E_RESOURCES_INVALIDATED):
            return u"Resources invalidated"_s;
        case(AUDCLNT_E_RAW_MODE_UNSUPPORTED):
            return u"Raw mode unsupported"_s;
        case(AUDCLNT_E_ENGINE_PERIODICITY_LOCKED):
            return u"Engine periodicity locked"_s;
        case(AUDCLNT_E_ENGINE_FORMAT_LOCKED):
            return u"Engine format locked"_s;
        case(AUDCLNT_E_HEADTRACKING_ENABLED):
            return u"Headtracking enabled"_s;
        case(AUDCLNT_E_HEADTRACKING_UNSUPPORTED):
            return u"Headtracking unsupported"_s;
        case(AUDCLNT_E_EFFECT_NOT_AVAILABLE):
            return u"Effect not available"_s;
        case(AUDCLNT_E_EFFECT_STATE_READ_ONLY):
            return u"Effect state read-only"_s;
        case(AUDCLNT_E_POST_VOLUME_LOOPBACK_UNSUPPORTED):
            return u"Post-volume loopback unsupported"_s;
        case(AUDCLNT_S_BUFFER_EMPTY):
            return u"Buffer empty (success)"_s;
        case(AUDCLNT_S_THREAD_ALREADY_REGISTERED):
            return u"Thread already registered (success)"_s;
        case(AUDCLNT_S_POSITION_STALLED):
            return u"Position stalled (success)"_s;
        default:
            return u"Unknown error: %1"_s.arg(result);
    }
}

} // namespace

namespace Fooyin {
WasapiOutput::WasapiOutput()
    : m_volume{1.0}
    , m_deviceEnumerator{nullptr}
    , m_device{nullptr}
    , m_audioClient{nullptr}
    , m_renderClient{nullptr}
    , m_audioVolume{nullptr}
    , m_bufferSize{0}
    , m_bufferFrameCount{0}
    , m_initialised{false}
    , m_running{false}
    , m_draining{false}
{ }

WasapiOutput::~WasapiOutput()
{
    uninit();
}

bool WasapiOutput::init(const AudioFormat& format)
{
    m_format = format;
    m_error.clear();

    if(!initDevice()) {
        return false;
    }

    m_initialised = true;
    return true;
}

void WasapiOutput::uninit()
{
    m_running     = false;
    m_initialised = false;

    uninitDevice();
}

void WasapiOutput::reset()
{
    if(m_audioClient) {
        m_audioClient->Reset();
    }
}

void WasapiOutput::start()
{
    if(!m_initialised) {
        return;
    }

    m_running = true;

    if(m_audioClient) {
        HRESULT hr = m_audioClient->Start();
        if(FAILED(hr) && hr != AUDCLNT_E_NOT_STOPPED) {
            m_error = u"Failed to start audio client: %1"_s.arg(resultToError(hr));
            qCWarning(WASAPI) << m_error;
            return;
        }
    }
}

void WasapiOutput::drain()
{
    if(!m_initialised || !m_audioClient) {
        return;
    }

    m_draining = true;

    int sleepMs{500};

    while(m_draining && sleepMs > 0) {
        Sleep(50);
        sleepMs -= 50;
    }
}

bool WasapiOutput::initialised() const
{
    return m_initialised;
}

QString WasapiOutput::device() const
{
    return m_deviceId;
}

int WasapiOutput::bufferSize() const
{
    return m_bufferSize;
}

OutputState WasapiOutput::currentState()
{
    OutputState state;

    if(!m_initialised || !m_audioClient) {
        state.queuedSamples = 0;
        state.freeSamples   = 0;
        return state;
    }

    UINT32 paddingFrames{0};
    HRESULT hr = m_audioClient->GetCurrentPadding(&paddingFrames);

    if(FAILED(hr)) {
        qCWarning(WASAPI) << "Failed to get current padding:" << resultToError(hr);
        state.queuedSamples = 0;
        state.freeSamples   = 0;
        return state;
    }

    UINT32 availableFrames = m_bufferFrameCount - paddingFrames;

    state.queuedSamples = m_bufferSize - availableFrames;
    state.freeSamples   = availableFrames;

    return state;
}

OutputDevices WasapiOutput::getAllDevices(bool isCurrentOutput)
{
    OutputDevices devices;

    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  &enumerator);
    if(FAILED(hr)) {
        return devices;
    }

    ComPtr<IMMDeviceCollection> deviceCollection;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
    if(FAILED(hr)) {
        return devices;
    }

    UINT count = 0;
    deviceCollection->GetCount(&count);

    for(UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        hr = deviceCollection->Item(i, &device);
        if(FAILED(hr)) {
            continue;
        }

        LPWSTR deviceId = nullptr;
        device->GetId(&deviceId);
        QString id = QString::fromWCharArray(deviceId);
        CoTaskMemFree(deviceId);

        QString name = getDeviceName(device.Get());
        if(name.isEmpty()) {
            continue;
        }

        // Always add shared mode entry
        devices.emplace_back(id + u"|shared"_s, name);

        // Add exclusive mode entry if supported
        if(supportsExclusiveMode(device.Get())) {
            devices.emplace_back(id + u"|exclusive"_s, name + u" (Exclusive)"_s);
        }
    }

    return devices;
}

int WasapiOutput::write(const AudioBuffer& buffer)
{
    if(!m_initialised || !m_renderClient) {
        return 0;
    }

    UINT32 paddingFrames{0};
    HRESULT hr = m_audioClient->GetCurrentPadding(&paddingFrames);
    if(FAILED(hr)) {
        qCWarning(WASAPI) << "Failed to get padding:" << resultToError(hr);
        return 0;
    }

    UINT32 availableFrames = m_bufferFrameCount - paddingFrames;

    // Only write what we can fit AND what's actually in the buffer
    const UINT32 framesToWrite = std::min(static_cast<UINT32>(buffer.frameCount()), availableFrames);

    if(framesToWrite == 0) {
        // Mark drain as complete if buffer is empty
        if(m_draining && paddingFrames == 0) {
            m_draining = false;
        }
        return 0;
    }

    BYTE* data{nullptr};
    hr = m_renderClient->GetBuffer(framesToWrite, &data);

    if(FAILED(hr)) {
        qCWarning(WASAPI) << "Failed to get buffer:" << resultToError(hr);
        return 0;
    }

    // Copy only the bytes for the frames we're actually writing
    const UINT32 bytesToWrite = framesToWrite * m_format.bytesPerFrame();
    std::memcpy(data, buffer.data(), bytesToWrite);

    hr = m_renderClient->ReleaseBuffer(framesToWrite, 0);
    if(FAILED(hr)) {
        qCWarning(WASAPI) << "Failed to release buffer:" << resultToError(hr);
        return 0;
    }

    return static_cast<int>(framesToWrite * buffer.format().channelCount());
}

void WasapiOutput::setPaused(bool pause)
{
    if(!m_audioClient) {
        return;
    }

    if(pause) {
        m_audioClient->Stop();
    }
    else {
        m_audioClient->Start();
    }
}

void WasapiOutput::setVolume(double volume)
{
    m_volume = volume;

    if(m_audioVolume && m_initialised) {
        m_audioVolume->SetMasterVolume(static_cast<float>(volume), nullptr);
    }
}

void WasapiOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_deviceId = device;

        // Reinitialize if already initialized
        if(m_initialised) {
            uninit();
            init(m_format);
        }
    }
}

QString WasapiOutput::error() const
{
    return m_error;
}

AudioFormat WasapiOutput::format() const
{
    return m_format;
}

bool WasapiOutput::initDevice()
{
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  &m_deviceEnumerator);

    if(FAILED(hr)) {
        m_error = u"Failed to create device enumerator: %1"_s.arg(hr);
        qCWarning(WASAPI) << m_error;
        return false;
    }

    // Get default or specified device
    if(m_deviceId.isEmpty() || m_deviceId == "default"_L1) {
        hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_device);
    }
    else {
        hr = m_deviceEnumerator->GetDevice(m_deviceId.toStdWString().c_str(), &m_device);
    }

    if(FAILED(hr)) {
        m_error = u"Failed to get audio device: %1"_s.arg(hr);
        qCWarning(WASAPI) << m_error;
        return false;
    }

    return initAudioClient();
}

bool WasapiOutput::initAudioClient()
{
    if(!m_device) {
        return false;
    }

    HRESULT hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &m_audioClient);

    if(FAILED(hr)) {
        m_error = u"Failed to activate audio device: %1"_s.arg(hr);
        qCWarning(WASAPI) << m_error;
        return false;
    }

    // Create wave format
    m_waveFormat = createWaveFormat(m_format);

    DWORD flags{0};
    // Enable auto-convert if format not supported
    flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    // Enable sample rate conversion
    flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

    double bufferLengthSeconds  = 0.25;
    REFERENCE_TIME hundredNanos = REFERENCE_TIME(1000.0 * 1000.0 * 10.0 * bufferLengthSeconds);
    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, hundredNanos, 0, &m_waveFormat.Format, nullptr);
    if(FAILED(hr)) {
        m_error = u"Failed to initialize audio client: %1"_s.arg(hr);
        qCWarning(WASAPI) << m_error;
        return false;
    }

    // Get the actual buffer size that was set
    hr = m_audioClient->GetBufferSize(&m_bufferFrameCount);
    if(FAILED(hr)) {
        m_error = u"Failed to get buffer size: %1"_s.arg(hr);
        qCWarning(WASAPI) << m_error;
        return false;
    }

    // Calculate buffer size in samples (frames * channels)
    m_bufferSize = static_cast<int>(m_bufferFrameCount) * m_format.channelCount();

    m_latency = static_cast<double>(m_bufferFrameCount) / m_format.sampleRate();

    // Get render client
    hr = m_audioClient->GetService(__uuidof(IAudioRenderClient), &m_renderClient);
    if(FAILED(hr)) {
        m_error = u"Failed to get render client: %1"_s.arg(hr);
        qCWarning(WASAPI) << m_error;
        return false;
    }

    // Get volume control
    hr = m_audioClient->GetService(__uuidof(ISimpleAudioVolume), &m_audioVolume);
    if(FAILED(hr)) {
        // Volume control is optional, so just log warning
        qCWarning(WASAPI) << "Failed to get volume control:" << hr;
    }

    return true;
}

void WasapiOutput::uninitDevice()
{
    m_initialised = false;
}

QString WasapiOutput::getDeviceName(IMMDevice* device)
{
    ComPtr<IPropertyStore> props;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &props);
    if(FAILED(hr)) {
        return {};
    }

    PROPVARIANT varName;
    PropVariantInit(&varName);

    hr = props->GetValue(PKEY_Device_FriendlyName, &varName);
    QString name;
    if(SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
        name = QString::fromWCharArray(varName.pwszVal);
    }

    PropVariantClear(&varName);
    return name;
}

bool WasapiOutput::supportsExclusiveMode(IMMDevice* device)
{
    ComPtr<IAudioClient> audioClient;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient);
    if(FAILED(hr)) {
        return false;
    }

    // Test common formats for exclusive mode support
    static const std::array<std::pair<DWORD, WORD>, 4> testFormats
        = {{{44100, 16}, {48000, 16}, {44100, 24}, {48000, 24}}};

    for(const auto& [sampleRate, bitsPerSample] : testFormats) {
        WAVEFORMATEXTENSIBLE wfx        = {};
        wfx.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels            = 2;
        wfx.Format.nSamplesPerSec       = sampleRate;
        wfx.Format.wBitsPerSample       = bitsPerSample;
        wfx.Format.nBlockAlign          = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
        wfx.Format.nAvgBytesPerSec      = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize               = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.Samples.wValidBitsPerSample = bitsPerSample;
        wfx.dwChannelMask               = KSAUDIO_SPEAKER_STEREO;
        wfx.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;

        hr = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &wfx.Format, nullptr);
        if(hr == S_OK) {
            return true;
        }
    }

    return false;
}

WAVEFORMATEXTENSIBLE WasapiOutput::createWaveFormat(const AudioFormat& format)
{
    WAVEFORMATEXTENSIBLE waveFormat = {};

    waveFormat.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    waveFormat.Format.nChannels       = static_cast<WORD>(format.channelCount());
    waveFormat.Format.nSamplesPerSec  = format.sampleRate();
    waveFormat.Format.wBitsPerSample  = format.bitsPerSample();
    waveFormat.Format.nBlockAlign     = static_cast<WORD>(format.bytesPerFrame());
    waveFormat.Format.nAvgBytesPerSec = format.sampleRate() * format.bytesPerFrame();
    waveFormat.Format.cbSize          = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

    switch(format.sampleFormat()) {
        case(SampleFormat::U8):
            waveFormat.Samples.wValidBitsPerSample = 8;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case(SampleFormat::S16):
            waveFormat.Samples.wValidBitsPerSample = 16;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case(SampleFormat::S24):
            waveFormat.Samples.wValidBitsPerSample = 24;
            waveFormat.Format.wBitsPerSample       = 32;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case(SampleFormat::S32):
            waveFormat.Samples.wValidBitsPerSample = 32;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;
            break;
        case(SampleFormat::F32):
            waveFormat.Samples.wValidBitsPerSample = 32;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
        case(SampleFormat::F64):
            waveFormat.Samples.wValidBitsPerSample = 64;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
        default:
            waveFormat.Samples.wValidBitsPerSample = 16;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;
            break;
    }

    // Set channel mask
    switch(format.channelCount()) {
        case 1:
            waveFormat.dwChannelMask = KSAUDIO_SPEAKER_MONO;
            break;
        case 2:
            waveFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
            break;
        case 4:
            waveFormat.dwChannelMask = KSAUDIO_SPEAKER_QUAD;
            break;
        case 5:
            waveFormat.dwChannelMask = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER
                                        | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT);
            break;
        case 6:
            waveFormat.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
            break;
        case 7:
            waveFormat.dwChannelMask
                = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY
                   | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_BACK_CENTER);
            break;
        case 8:
            waveFormat.dwChannelMask = KSAUDIO_SPEAKER_7POINT1;
            break;
        default:
            waveFormat.dwChannelMask = 0;
            break;
    }

    return waveFormat;
}
} // namespace Fooyin