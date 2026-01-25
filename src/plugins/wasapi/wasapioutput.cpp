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

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>

Q_LOGGING_CATEGORY(WASAPI, "fy.wasapi")

using namespace Qt::StringLiterals;

namespace {
using ChannelPosition = Fooyin::AudioFormat::ChannelPosition;

struct DeviceSelection
{
    QString endpointId;
    bool isDefault{true};
    bool exclusive{false};
};

struct ComInitialisation
{
    bool initialised{false};

    ~ComInitialisation()
    {
        if(initialised) {
            CoUninitialize();
        }
    }
};

QString resultToError(const HRESULT result)
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
            return u"Buffer empty"_s;
        case(AUDCLNT_S_THREAD_ALREADY_REGISTERED):
            return u"Thread already registered"_s;
        case(AUDCLNT_S_POSITION_STALLED):
            return u"Position stalled"_s;
        case(RPC_E_CHANGED_MODE):
            return u"COM apartment mode mismatch"_s;
        default:
            return u"HRESULT 0x%1"_s.arg(static_cast<qulonglong>(static_cast<uint32_t>(result)), 8, 16, QChar{u'0'});
    }
}

DeviceSelection parseDeviceSelection(const QString& device)
{
    DeviceSelection selection;
    const QString raw = device.trimmed();

    if(raw.isEmpty() || raw == u"default"_s) {
        return selection;
    }

    const qsizetype separator = raw.lastIndexOf(u'|');
    if(separator >= 0) {
        const QString mode   = raw.sliced(separator + 1);
        selection.exclusive  = (mode == u"exclusive"_s);
        selection.endpointId = raw.first(separator);
    }
    else {
        selection.endpointId = raw;
    }

    selection.isDefault = selection.endpointId.isEmpty() || selection.endpointId == u"default"_s;
    if(selection.isDefault) {
        selection.endpointId.clear();
    }

    return selection;
}

DWORD channelMaskForPosition(const ChannelPosition position)
{
    switch(position) {
        case(ChannelPosition::FrontLeft):
            return SPEAKER_FRONT_LEFT;
        case(ChannelPosition::FrontRight):
            return SPEAKER_FRONT_RIGHT;
        case(ChannelPosition::FrontCenter):
            return SPEAKER_FRONT_CENTER;
        case(ChannelPosition::LFE):
            return SPEAKER_LOW_FREQUENCY;
        case(ChannelPosition::BackLeft):
            return SPEAKER_BACK_LEFT;
        case(ChannelPosition::BackRight):
            return SPEAKER_BACK_RIGHT;
        case(ChannelPosition::FrontLeftOfCenter):
            return SPEAKER_FRONT_LEFT_OF_CENTER;
        case(ChannelPosition::FrontRightOfCenter):
            return SPEAKER_FRONT_RIGHT_OF_CENTER;
        case(ChannelPosition::BackCenter):
            return SPEAKER_BACK_CENTER;
        case(ChannelPosition::SideLeft):
            return SPEAKER_SIDE_LEFT;
        case(ChannelPosition::SideRight):
            return SPEAKER_SIDE_RIGHT;
        case(ChannelPosition::TopCenter):
            return SPEAKER_TOP_CENTER;
        case(ChannelPosition::TopFrontLeft):
            return SPEAKER_TOP_FRONT_LEFT;
        case(ChannelPosition::TopFrontCenter):
            return SPEAKER_TOP_FRONT_CENTER;
        case(ChannelPosition::TopFrontRight):
            return SPEAKER_TOP_FRONT_RIGHT;
        case(ChannelPosition::TopBackLeft):
            return SPEAKER_TOP_BACK_LEFT;
        case(ChannelPosition::TopBackCenter):
            return SPEAKER_TOP_BACK_CENTER;
        case(ChannelPosition::TopBackRight):
            return SPEAKER_TOP_BACK_RIGHT;
        case(ChannelPosition::UnknownPosition):
        case(ChannelPosition::LFE2):
        case(ChannelPosition::TopSideLeft):
        case(ChannelPosition::TopSideRight):
        case(ChannelPosition::BottomFrontCenter):
        case(ChannelPosition::BottomFrontLeft):
        case(ChannelPosition::BottomFrontRight):
        default:
            return 0;
    }
}

bool ensureComInitialised(ComInitialisation& init, const bool ownsThreadApartment)
{
    if(!ownsThreadApartment) {
        return true;
    }

    const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(SUCCEEDED(result)) {
        init.initialised = true;
        return true;
    }

    return result == RPC_E_CHANGED_MODE;
}
} // namespace

namespace Fooyin::Wasapi {
WasapiOutput::WasapiOutput()
    : m_deviceSelection{u"default|shared"_s}
    , m_volume{1.0}
    , m_comInitialised{false}
    , m_exclusiveMode{false}
    , m_started{false}
    , m_bufferFrames{0}
    , m_bufferFrameCount{0}
    , m_latency{0.0}
    , m_initialised{false}
{ }

WasapiOutput::~WasapiOutput()
{
    uninit();
}

bool WasapiOutput::init(const AudioFormat& format)
{
    uninit();

    m_format = format;
    m_error.clear();

    if(!m_format.isValid()) {
        m_error = u"Invalid audio format requested"_s;
        return false;
    }

    if(!initialiseCom()) {
        return false;
    }

    if(!initDevice()) {
        uninit();
        return false;
    }

    m_initialised.store(true, std::memory_order_release);

    if(m_audioVolume) {
        setVolume(m_volume);
    }

    return true;
}

void WasapiOutput::uninit()
{
    m_initialised.store(false, std::memory_order_release);
    uninitDevice();
}

void WasapiOutput::reset()
{
    if(!m_audioClient) {
        return;
    }

    if(m_started) {
        const HRESULT stopResult = m_audioClient->Stop();
        if(FAILED(stopResult) && stopResult != AUDCLNT_E_NOT_STOPPED) {
            handleError(stopResult, u"Failed to stop audio client"_s, true);
        }
    }

    const HRESULT resetResult = m_audioClient->Reset();
    if(FAILED(resetResult) && resetResult != AUDCLNT_E_NOT_INITIALIZED) {
        handleError(resetResult, u"Failed to reset audio client"_s, true);
    }

    m_started = false;
}

void WasapiOutput::start()
{
    if(!m_audioClient || m_started) {
        return;
    }

    const HRESULT result = m_audioClient->Start();
    if(FAILED(result) && result != AUDCLNT_E_NOT_STOPPED) {
        handleError(result, u"Failed to start audio client"_s, true);
        return;
    }

    m_started = true;
}

void WasapiOutput::drain()
{
    if(!m_audioClient || !m_initialised.load(std::memory_order_acquire)) {
        return;
    }

    for(int attempt{0}; attempt < 200; ++attempt) {
        UINT32 paddingFrames{0};
        const HRESULT result = m_audioClient->GetCurrentPadding(&paddingFrames);
        if(FAILED(result)) {
            handleError(result, u"Failed to query buffered frames"_s, true);
            return;
        }

        if(paddingFrames == 0) {
            return;
        }

        const int sleepMs = std::clamp(
            static_cast<int>(std::ceil((static_cast<double>(paddingFrames) * 1000.0) / m_format.sampleRate() / 4.0)), 1,
            20);
        QThread::msleep(static_cast<unsigned long>(sleepMs));
    }
}

bool WasapiOutput::initialised() const
{
    return m_initialised.load(std::memory_order_acquire);
}

QString WasapiOutput::device() const
{
    return m_deviceSelection;
}

int WasapiOutput::bufferSize() const
{
    return m_bufferFrames;
}

OutputState WasapiOutput::currentState()
{
    OutputState state;

    if(!m_audioClient || !m_format.isValid()) {
        return state;
    }

    UINT32 paddingFrames{0};
    const HRESULT result = m_audioClient->GetCurrentPadding(&paddingFrames);
    if(FAILED(result)) {
        handleError(result, u"Failed to query current padding"_s, true);
        return state;
    }

    const int queuedFrames = static_cast<int>(std::min(paddingFrames, m_bufferFrameCount));
    state.queuedFrames     = std::max(0, queuedFrames);
    state.freeFrames       = std::max(0, m_bufferFrames - state.queuedFrames);

    if(m_format.sampleRate() > 0) {
        const double queuedDelay = static_cast<double>(state.queuedFrames) / static_cast<double>(m_format.sampleRate());
        state.delay              = std::max(m_latency, queuedDelay);
    }

    return state;
}

OutputDevices WasapiOutput::getAllDevices(bool /*isCurrentOutput*/)
{
    OutputDevices devices;
    const bool ownsThreadApartment
        = !QCoreApplication::instance() || QThread::currentThread() != QCoreApplication::instance()->thread();
    ComInitialisation com;
    if(!ensureComInitialised(com, ownsThreadApartment)) {
        return devices;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    const HRESULT createResult
        = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
    if(FAILED(createResult)) {
        return devices;
    }

    ComPtr<IMMDevice> defaultDevice;
    if(SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
        devices.emplace_back(u"default|shared"_s, u"Default"_s);
        if(supportsExclusiveMode(defaultDevice.Get())) {
            devices.emplace_back(u"default|exclusive"_s, u"Default (Exclusive)"_s);
        }
    }
    else {
        devices.emplace_back(u"default|shared"_s, u"Default"_s);
    }

    ComPtr<IMMDeviceCollection> collection;
    const HRESULT enumResult = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
    if(FAILED(enumResult)) {
        return devices;
    }

    UINT count{0};
    if(FAILED(collection->GetCount(&count))) {
        return devices;
    }

    for(UINT i{0}; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if(FAILED(collection->Item(i, &device))) {
            continue;
        }

        LPWSTR rawDeviceId{nullptr};
        if(FAILED(device->GetId(&rawDeviceId)) || !rawDeviceId) {
            continue;
        }

        const QString deviceId = QString::fromWCharArray(rawDeviceId);
        CoTaskMemFree(rawDeviceId);

        const QString name = getDeviceName(device.Get());
        if(name.isEmpty()) {
            continue;
        }

        devices.emplace_back(deviceId + u"|shared"_s, name);
        if(supportsExclusiveMode(device.Get())) {
            devices.emplace_back(deviceId + u"|exclusive"_s, name + u" (Exclusive)"_s);
        }
    }

    return devices;
}

int WasapiOutput::write(const std::span<const std::byte> data, int frameCount)
{
    if(!m_renderClient || !m_audioClient || frameCount <= 0) {
        return 0;
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return 0;
    }

    const int availableFrames = static_cast<int>(data.size() / static_cast<size_t>(bytesPerFrame));
    const int requestedFrames = std::min(frameCount, availableFrames);
    if(requestedFrames <= 0) {
        return 0;
    }

    UINT32 paddingFrames{0};
    const HRESULT paddingResult = m_audioClient->GetCurrentPadding(&paddingFrames);
    if(FAILED(paddingResult)) {
        handleError(paddingResult, u"Failed to query current padding"_s, true);
        return 0;
    }

    const UINT32 freeFrames    = (paddingFrames >= m_bufferFrameCount) ? 0 : (m_bufferFrameCount - paddingFrames);
    const UINT32 framesToWrite = std::min(static_cast<UINT32>(requestedFrames), freeFrames);
    if(framesToWrite == 0) {
        return 0;
    }

    BYTE* buffer{nullptr};
    const HRESULT getBufferResult = m_renderClient->GetBuffer(framesToWrite, &buffer);
    if(FAILED(getBufferResult)) {
        handleError(getBufferResult, u"Failed to acquire render buffer"_s, true);
        return 0;
    }

    const size_t bytesToWrite = static_cast<size_t>(framesToWrite) * static_cast<size_t>(bytesPerFrame);
    std::memcpy(buffer, data.data(), bytesToWrite);

    const HRESULT releaseResult = m_renderClient->ReleaseBuffer(framesToWrite, 0);
    if(FAILED(releaseResult)) {
        handleError(releaseResult, u"Failed to release render buffer"_s, true);
        return 0;
    }

    return static_cast<int>(framesToWrite);
}

void WasapiOutput::setPaused(bool pause)
{
    if(!m_audioClient) {
        return;
    }

    if(pause) {
        const HRESULT result = m_audioClient->Stop();
        if(FAILED(result) && result != AUDCLNT_E_NOT_STOPPED) {
            handleError(result, u"Failed to pause audio client"_s, true);
            return;
        }
        m_started = false;
        return;
    }

    start();
}

bool WasapiOutput::supportsVolumeControl() const
{
    return !parseDeviceSelection(m_deviceSelection).exclusive;
}

void WasapiOutput::setVolume(double volume)
{
    m_volume = volume;

    if(!m_audioVolume || !supportsVolumeControl()) {
        return;
    }

    const HRESULT result = m_audioVolume->SetMasterVolume(static_cast<float>(volume), nullptr);
    if(FAILED(result)) {
        qCWarning(WASAPI) << "Failed to set WASAPI volume:" << resultToError(result);
    }
}

void WasapiOutput::setDevice(const QString& device)
{
    if(device.isEmpty() || device == m_deviceSelection) {
        return;
    }

    m_deviceSelection = device;

    if(initialised()) {
        const AudioFormat currentFormat = m_format;
        uninit();
        init(currentFormat);
    }
}

AudioFormat WasapiOutput::negotiateFormat(const AudioFormat& requested) const
{
    if(!requested.isValid()) {
        return requested;
    }

    AudioFormat negotiated{requested};
    if(negotiated.sampleFormat() == SampleFormat::F64) {
        negotiated.setSampleFormat(SampleFormat::F32);
    }

    if(negotiated.hasChannelLayout() && channelMaskForFormat(negotiated) == 0) {
        const auto fallback = AudioFormat::defaultChannelLayoutForChannelCount(negotiated.channelCount());
        if(std::cmp_equal(fallback.size(), negotiated.channelCount())) {
            negotiated.setChannelLayout(fallback);
        }
        else {
            negotiated.clearChannelLayout();
        }
    }

    return negotiated;
}

QString WasapiOutput::error() const
{
    return m_error;
}

AudioFormat WasapiOutput::format() const
{
    return m_format;
}

bool WasapiOutput::initialiseCom()
{
    const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(SUCCEEDED(result)) {
        m_comInitialised = true;
        return true;
    }

    if(result == RPC_E_CHANGED_MODE) {
        return true;
    }

    return handleError(result, u"Failed to initialise COM"_s);
}

bool WasapiOutput::initDevice()
{
    const auto selection = parseDeviceSelection(m_deviceSelection);
    m_exclusiveMode      = selection.exclusive;

    const HRESULT createResult
        = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_deviceEnumerator));
    if(FAILED(createResult)) {
        return handleError(createResult, u"Failed to create device enumerator"_s);
    }

    HRESULT deviceResult = E_FAIL;
    if(selection.isDefault) {
        deviceResult = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_audioDevice);
    }
    else {
        deviceResult = m_deviceEnumerator->GetDevice(selection.endpointId.toStdWString().c_str(), &m_audioDevice);
    }

    if(FAILED(deviceResult)) {
        return handleError(deviceResult, u"Failed to open output device"_s);
    }

    return initAudioClient();
}

bool WasapiOutput::initAudioClient()
{
    if(!m_audioDevice) {
        m_error = u"No WASAPI device selected"_s;
        return false;
    }

    const HRESULT activateResult = m_audioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &m_audioClient);
    if(FAILED(activateResult)) {
        return handleError(activateResult, u"Failed to activate audio client"_s);
    }

    m_waveFormat = createWaveFormat(m_format);

    DWORD flags{0};
    if(!m_exclusiveMode) {
        flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
        flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    }

    const auto shareMode = m_exclusiveMode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
    static constexpr REFERENCE_TIME HundredNanosPerMillisecond = 10000;
    static constexpr REFERENCE_TIME RequestedBufferDurationMs  = 250;
    REFERENCE_TIME bufferDuration                              = RequestedBufferDurationMs * HundredNanosPerMillisecond;

    HRESULT formatResult{S_OK};
    WAVEFORMATEX* closestMatch{nullptr};
    if(m_exclusiveMode) {
        formatResult = m_audioClient->IsFormatSupported(shareMode, &m_waveFormat.Format, nullptr);
        if(formatResult != S_OK) {
            return handleError(formatResult, u"Exclusive mode does not support the requested format"_s);
        }
    }
    else {
        formatResult = m_audioClient->IsFormatSupported(shareMode, &m_waveFormat.Format, &closestMatch);
        if(closestMatch) {
            CoTaskMemFree(closestMatch);
        }
    }

    REFERENCE_TIME devicePeriod{0};
    if(m_exclusiveMode) {
        REFERENCE_TIME minimumPeriod{0};
        if(SUCCEEDED(m_audioClient->GetDevicePeriod(&devicePeriod, &minimumPeriod))) {
            if(devicePeriod <= 0) {
                devicePeriod = minimumPeriod;
            }
            if(devicePeriod > 0) {
                bufferDuration = devicePeriod;
            }
        }
    }

    const HRESULT initResult
        = m_audioClient->Initialize(shareMode, flags, bufferDuration, devicePeriod, &m_waveFormat.Format, nullptr);
    if(FAILED(initResult)) {
        return handleError(initResult, u"Failed to initialise audio client"_s);
    }

    const HRESULT sizeResult = m_audioClient->GetBufferSize(&m_bufferFrameCount);
    if(FAILED(sizeResult)) {
        return handleError(sizeResult, u"Failed to query buffer size"_s);
    }
    m_bufferFrames = static_cast<int>(m_bufferFrameCount);

    REFERENCE_TIME streamLatency{0};
    if(SUCCEEDED(m_audioClient->GetStreamLatency(&streamLatency))) {
        m_latency = static_cast<double>(streamLatency) / 10000000.0;
    }
    else if(m_format.sampleRate() > 0) {
        m_latency = static_cast<double>(m_bufferFrameCount) / static_cast<double>(m_format.sampleRate());
    }
    else {
        m_latency = 0.0;
    }

    const HRESULT renderResult = m_audioClient->GetService(__uuidof(IAudioRenderClient), &m_renderClient);
    if(FAILED(renderResult)) {
        return handleError(renderResult, u"Failed to acquire render client"_s);
    }

    const HRESULT clockResult = m_audioClient->GetService(__uuidof(IAudioClock), &m_audioClock);
    if(FAILED(clockResult)) {
        qCWarning(WASAPI) << "Failed to get WASAPI clock:" << resultToError(clockResult);
    }

    if(!m_exclusiveMode) {
        const HRESULT volumeResult = m_audioClient->GetService(__uuidof(ISimpleAudioVolume), &m_audioVolume);
        if(FAILED(volumeResult)) {
            qCWarning(WASAPI) << "Failed to get WASAPI volume control:" << resultToError(volumeResult);
        }
    }

    return true;
}

void WasapiOutput::uninitDevice()
{
    if(m_audioClient) {
        const HRESULT stopResult = m_audioClient->Stop();
        if(FAILED(stopResult) && stopResult != AUDCLNT_E_NOT_STOPPED && stopResult != AUDCLNT_E_NOT_INITIALIZED) {
            qCWarning(WASAPI) << "Failed to stop WASAPI client during teardown:" << resultToError(stopResult);
        }
    }

    m_audioVolume.Reset();
    m_renderClient.Reset();
    m_audioClock.Reset();
    m_audioClient.Reset();
    m_audioDevice.Reset();
    m_deviceEnumerator.Reset();

    m_started          = false;
    m_exclusiveMode    = false;
    m_bufferFrames     = 0;
    m_bufferFrameCount = 0;
    m_latency          = 0.0;
    m_waveFormat       = {};

    if(m_comInitialised) {
        CoUninitialize();
        m_comInitialised = false;
    }
}

bool WasapiOutput::handleError(const HRESULT result, const QString& action, const bool runtime)
{
    m_error = u"%1: %2"_s.arg(action, resultToError(result));
    qCWarning(WASAPI) << m_error;

    if(runtime) {
        const auto state = (result == AUDCLNT_E_DEVICE_INVALIDATED || result == AUDCLNT_E_RESOURCES_INVALIDATED)
                             ? AudioOutput::State::Disconnected
                             : AudioOutput::State::Error;
        QMetaObject::invokeMethod(this, [this, state]() { Q_EMIT stateChanged(state); }, Qt::QueuedConnection);
    }

    return false;
}

QString WasapiOutput::getDeviceName(IMMDevice* device)
{
    if(!device) {
        return {};
    }

    ComPtr<IPropertyStore> properties;
    const HRESULT openResult = device->OpenPropertyStore(STGM_READ, &properties);
    if(FAILED(openResult)) {
        return {};
    }

    PROPVARIANT value;
    PropVariantInit(&value);

    QString name;
    if(SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) {
        name = QString::fromWCharArray(value.pwszVal);
    }

    PropVariantClear(&value);
    return name;
}

bool WasapiOutput::supportsExclusiveMode(IMMDevice* device)
{
    if(!device) {
        return false;
    }

    ComPtr<IAudioClient> audioClient;
    const HRESULT activateResult = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &audioClient);
    if(FAILED(activateResult)) {
        return false;
    }

    static const std::array<std::pair<DWORD, WORD>, 4> testFormats
        = {{{44100, 16}, {48000, 16}, {44100, 24}, {48000, 24}}};

    for(const auto& [sampleRate, validBits] : testFormats) {
        WAVEFORMATEXTENSIBLE waveFormat{};
        waveFormat.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
        waveFormat.Format.nChannels            = 2;
        waveFormat.Format.nSamplesPerSec       = sampleRate;
        waveFormat.Format.wBitsPerSample       = (validBits == 24) ? 32 : validBits;
        waveFormat.Format.nBlockAlign          = waveFormat.Format.nChannels * (waveFormat.Format.wBitsPerSample / 8);
        waveFormat.Format.nAvgBytesPerSec      = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
        waveFormat.Format.cbSize               = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        waveFormat.Samples.wValidBitsPerSample = validBits;
        waveFormat.dwChannelMask               = KSAUDIO_SPEAKER_STEREO;
        waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;

        if(audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &waveFormat.Format, nullptr) == S_OK) {
            return true;
        }
    }

    return false;
}

DWORD WasapiOutput::channelMaskForFormat(const AudioFormat& format)
{
    if(format.channelCount() <= 0) {
        return 0;
    }

    if(format.hasChannelLayout()) {
        DWORD mask{0};
        for(int channel = 0; channel < format.channelCount(); ++channel) {
            const DWORD bit = channelMaskForPosition(format.channelPosition(channel));
            if(bit == 0 || (mask & bit) != 0) {
                return 0;
            }
            mask |= bit;
        }
        return mask;
    }

    switch(format.channelCount()) {
        case 1:
            return KSAUDIO_SPEAKER_MONO;
        case 2:
            return KSAUDIO_SPEAKER_STEREO;
        case 4:
            return KSAUDIO_SPEAKER_QUAD;
        case 5:
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT
                 | SPEAKER_BACK_RIGHT;
        case 6:
            return KSAUDIO_SPEAKER_5POINT1;
        case 7:
            return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY
                 | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT | SPEAKER_BACK_CENTER;
        case 8:
            return KSAUDIO_SPEAKER_7POINT1;
        default:
            return 0;
    }
}

WAVEFORMATEXTENSIBLE WasapiOutput::createWaveFormat(const AudioFormat& format)
{
    WAVEFORMATEXTENSIBLE waveFormat{};

    waveFormat.Format.wFormatTag      = WAVE_FORMAT_EXTENSIBLE;
    waveFormat.Format.nChannels       = static_cast<WORD>(format.channelCount());
    waveFormat.Format.nSamplesPerSec  = static_cast<DWORD>(format.sampleRate());
    waveFormat.Format.wBitsPerSample  = static_cast<WORD>(format.bitsPerSample());
    waveFormat.Format.nBlockAlign     = static_cast<WORD>(format.bytesPerFrame());
    waveFormat.Format.nAvgBytesPerSec = static_cast<DWORD>(format.sampleRate() * format.bytesPerFrame());
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
        case(SampleFormat::S24In32):
            waveFormat.Format.wBitsPerSample  = 32;
            waveFormat.Format.nBlockAlign     = static_cast<WORD>(format.channelCount() * 4);
            waveFormat.Format.nAvgBytesPerSec = static_cast<DWORD>(format.sampleRate() * waveFormat.Format.nBlockAlign);
            waveFormat.Samples.wValidBitsPerSample = 24;
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
            waveFormat.Format.wBitsPerSample  = 32;
            waveFormat.Format.nBlockAlign     = static_cast<WORD>(format.channelCount() * 4);
            waveFormat.Format.nAvgBytesPerSec = static_cast<DWORD>(format.sampleRate() * waveFormat.Format.nBlockAlign);
            waveFormat.Samples.wValidBitsPerSample = 32;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            break;
        case(SampleFormat::Unknown):
        default:
            waveFormat.Format.wBitsPerSample  = 16;
            waveFormat.Format.nBlockAlign     = static_cast<WORD>(format.channelCount() * 2);
            waveFormat.Format.nAvgBytesPerSec = static_cast<DWORD>(format.sampleRate() * waveFormat.Format.nBlockAlign);
            waveFormat.Samples.wValidBitsPerSample = 16;
            waveFormat.SubFormat                   = KSDATAFORMAT_SUBTYPE_PCM;
            break;
    }

    waveFormat.dwChannelMask = channelMaskForFormat(format);
    return waveFormat;
}
} // namespace Fooyin::Wasapi
