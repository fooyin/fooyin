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

#include <QLoggingCategory>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <array>
#include <avrt.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functiondiscoverykeys_devpkey.h>
#include <limits>
#include <propvarutil.h>
#include <vector>

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

bool ensureComInitialised(ComInitialisation& init)
{
    const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if(SUCCEEDED(result)) {
        init.initialised = true;
        return true;
    }

    return result == RPC_E_CHANGED_MODE;
}
} // namespace

namespace Fooyin::Wasapi {
class DeviceNotificationClient final : public IMMNotificationClient
{
public:
    explicit DeviceNotificationClient(WasapiOutput* owner)
        : m_owner{owner}
    { }

    void detach()
    {
        const std::scoped_lock lock{m_ownerMutex};
        m_owner = nullptr;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void** object) override
    {
        if(!object) {
            return E_POINTER;
        }

        if(interfaceId == __uuidof(IUnknown) || interfaceId == __uuidof(IMMNotificationClient)) {
            *object = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }

        *object = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return m_refCount.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG refs = m_refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if(refs == 0) {
            delete this;
        }
        return refs;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR deviceId, DWORD state) override
    {
        const std::scoped_lock lock{m_ownerMutex};
        if(m_owner) {
            m_owner->handleDeviceStateChanged(deviceId ? QString::fromWCharArray(deviceId) : QString{}, state);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR /*deviceId*/) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR deviceId) override
    {
        const std::scoped_lock lock{m_ownerMutex};
        if(m_owner) {
            m_owner->handleDeviceRemoved(deviceId ? QString::fromWCharArray(deviceId) : QString{});
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR deviceId) override
    {
        const std::scoped_lock lock{m_ownerMutex};
        if(m_owner) {
            m_owner->handleDefaultDeviceChanged(flow, role, deviceId ? QString::fromWCharArray(deviceId) : QString{});
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR /*deviceId*/, const PROPERTYKEY /*key*/) override
    {
        return S_OK;
    }

private:
    std::atomic<ULONG> m_refCount{1};
    std::mutex m_ownerMutex;
    WasapiOutput* m_owner;
};

WasapiOutput::WasapiOutput()
    : m_deviceSelection{u"default|shared"_s}
    , m_volume{1.0}
    , m_comInitialised{false}
    , m_exclusiveMode{false}
    , m_started{false}
    , m_bufferEvent{nullptr}
    , m_mmcssHandle{nullptr}
    , m_mmcssTaskIndex{0}
    , m_followsDefaultDevice{false}
    , m_disconnectNotified{false}
    , m_bufferFrames{0}
    , m_bufferFrameCount{0}
    , m_latency{0.0}
    , m_clockFrequency{0}
    , m_clockBasePosition{0}
    , m_clockSubmittedFrames{0}
    , m_clockBaseValid{false}
    , m_bufferEventAvailable{false}
    , m_packetSubmitted{false}
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
    m_disconnectNotified.store(false, std::memory_order_release);

    if(!m_format.isValid()) {
        m_error = u"Invalid audio format requested"_s;
        return false;
    }

    if(m_format.sampleFormat() == SampleFormat::F64) {
        m_format.setSampleFormat(SampleFormat::F32);
    }

    if(!initialiseCom()) {
        return false;
    }

    initialiseMmcss();

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
    else {
        resetClockTracking();
        if(m_bufferEvent) {
            ResetEvent(m_bufferEvent);
            m_bufferEventAvailable = true;
            m_packetSubmitted      = false;
        }
    }

    m_started = false;
}

void WasapiOutput::start()
{
    if(!m_audioClient || m_started) {
        return;
    }

    if(m_bufferEvent && !m_packetSubmitted) {
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

    bool startedForDrain{false};

    if(m_exclusiveMode && m_bufferEvent) {
        if(m_bufferEventAvailable) {
            return;
        }

        if(!m_started) {
            const HRESULT startResult = m_audioClient->Start();
            if(FAILED(startResult) && startResult != AUDCLNT_E_NOT_STOPPED) {
                handleError(startResult, u"Failed to start audio client while draining"_s, true);
                return;
            }
            m_started       = true;
            startedForDrain = true;
        }

        const DWORD waitResult = WaitForSingleObject(m_bufferEvent, 2000);
        if(waitResult == WAIT_OBJECT_0) {
            m_bufferEventAvailable = true;
            m_packetSubmitted      = false;
        }
        else if(waitResult == WAIT_TIMEOUT) {
            handleError(HRESULT_FROM_WIN32(ERROR_TIMEOUT), u"Timed out waiting for buffered audio to drain"_s, true);
        }
        else {
            handleError(HRESULT_FROM_WIN32(GetLastError()), u"Failed while waiting for buffered audio to drain"_s,
                        true);
        }

        if(startedForDrain) {
            const HRESULT stopResult = m_audioClient->Stop();
            if(FAILED(stopResult) && stopResult != AUDCLNT_E_NOT_STOPPED) {
                handleError(stopResult, u"Failed to stop audio client after draining"_s, true);
            }
            m_started = false;
        }
        return;
    }

    UINT32 lastQueuedFrames{std::numeric_limits<UINT32>::max()};
    auto lastProgress = std::chrono::steady_clock::now();

    while(true) {
        UINT32 paddingFrames{0};
        const HRESULT result = m_audioClient->GetCurrentPadding(&paddingFrames);
        if(FAILED(result)) {
            handleError(result, u"Failed to query buffered frames"_s, true);
            break;
        }

        const UINT32 queuedFrames = clockQueuedFrames(paddingFrames).value_or(paddingFrames);
        if(queuedFrames == 0) {
            break;
        }

        if(!m_started) {
            const HRESULT startResult = m_audioClient->Start();
            if(FAILED(startResult) && startResult != AUDCLNT_E_NOT_STOPPED) {
                handleError(startResult, u"Failed to start audio client while draining"_s, true);
                break;
            }
            m_started       = true;
            startedForDrain = true;
            lastProgress    = std::chrono::steady_clock::now();
        }

        if(queuedFrames < lastQueuedFrames) {
            lastQueuedFrames = queuedFrames;
            lastProgress     = std::chrono::steady_clock::now();
        }
        else if(std::chrono::steady_clock::now() - lastProgress >= std::chrono::seconds{2}) {
            handleError(HRESULT_FROM_WIN32(ERROR_TIMEOUT), u"Timed out waiting for buffered audio to drain"_s, true);
            break;
        }

        const int sleepMs = std::clamp(
            static_cast<int>(std::ceil((static_cast<double>(queuedFrames) * 1000.0) / m_format.sampleRate() / 4.0)), 1,
            20);
        QThread::msleep(static_cast<unsigned long>(sleepMs));
    }

    if(startedForDrain) {
        const HRESULT stopResult = m_audioClient->Stop();
        if(FAILED(stopResult) && stopResult != AUDCLNT_E_NOT_STOPPED) {
            handleError(stopResult, u"Failed to stop audio client after draining"_s, true);
        }
        m_started = false;
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

    if(m_bufferEvent && !m_bufferEventAvailable) {
        const DWORD waitResult = WaitForSingleObject(m_bufferEvent, 0);
        if(waitResult == WAIT_OBJECT_0) {
            m_bufferEventAvailable = true;
            m_packetSubmitted      = false;
        }
        else if(waitResult == WAIT_FAILED) {
            handleError(HRESULT_FROM_WIN32(GetLastError()), u"Failed to query WASAPI buffer event"_s, true);
            return state;
        }
    }

    if(m_exclusiveMode && m_bufferEvent) {
        state.freeFrames          = m_bufferEventAvailable ? m_bufferFrames : 0;
        const UINT32 queuedFrames = clockQueuedFrames(0).value_or(m_bufferEventAvailable ? 0U : m_bufferFrameCount);
        state.queuedFrames        = static_cast<int>(std::min(queuedFrames, m_bufferFrameCount));
        if(m_format.sampleRate() > 0) {
            state.delay = static_cast<double>(state.queuedFrames) / static_cast<double>(m_format.sampleRate());
        }
        return state;
    }

    UINT32 paddingFrames{0};
    const HRESULT result = m_audioClient->GetCurrentPadding(&paddingFrames);
    if(FAILED(result)) {
        handleError(result, u"Failed to query current padding"_s, true);
        return state;
    }

    const UINT32 measuredQueuedFrames = clockQueuedFrames(paddingFrames).value_or(paddingFrames);
    const int queuedFrames            = static_cast<int>(std::min(measuredQueuedFrames, m_bufferFrameCount));
    state.queuedFrames                = std::max(0, queuedFrames);
    const int availableFrames
        = std::max(0, m_bufferFrames - static_cast<int>(std::min(paddingFrames, m_bufferFrameCount)));
    state.freeFrames = !m_bufferEvent || m_bufferEventAvailable ? availableFrames : 0;

    if(m_format.sampleRate() > 0) {
        const double queuedDelay = static_cast<double>(state.queuedFrames) / static_cast<double>(m_format.sampleRate());
        state.delay = m_audioClock && m_clockFrequency > 0 ? queuedDelay : std::max(m_latency, queuedDelay);
    }

    return state;
}

OutputDevices WasapiOutput::getAllDevices(bool /*isCurrentOutput*/)
{
    OutputDevices devices;
    ComInitialisation com;
    if(!ensureComInitialised(com)) {
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

    if(m_exclusiveMode && m_bufferEvent) {
        if(!m_bufferEventAvailable) {
            return 0;
        }

        const UINT32 suppliedFrames = std::min(static_cast<UINT32>(requestedFrames), m_bufferFrameCount);
        BYTE* buffer{nullptr};
        const HRESULT getBufferResult = m_renderClient->GetBuffer(m_bufferFrameCount, &buffer);
        if(FAILED(getBufferResult)) {
            handleError(getBufferResult, u"Failed to acquire exclusive render buffer"_s, true);
            return 0;
        }

        const size_t suppliedBytes = static_cast<size_t>(suppliedFrames) * static_cast<size_t>(bytesPerFrame);
        std::memcpy(buffer, data.data(), suppliedBytes);

        const size_t bufferBytes = static_cast<size_t>(m_bufferFrameCount) * static_cast<size_t>(bytesPerFrame);
        if(suppliedBytes < bufferBytes) {
            const int silence = m_format.sampleFormat() == SampleFormat::U8 ? 0x80 : 0;
            std::memset(buffer + suppliedBytes, silence, bufferBytes - suppliedBytes);
        }

        const bool initialPacket    = !m_started && !m_packetSubmitted;
        const HRESULT releaseResult = m_renderClient->ReleaseBuffer(m_bufferFrameCount, 0);
        if(FAILED(releaseResult)) {
            handleError(releaseResult, u"Failed to release exclusive render buffer"_s, true);
            return 0;
        }

        if(initialPacket) {
            ResetEvent(m_bufferEvent);
        }
        m_bufferEventAvailable = false;
        m_packetSubmitted      = true;
        m_clockSubmittedFrames
            = std::min<UINT64>(std::numeric_limits<UINT64>::max() - m_bufferFrameCount, m_clockSubmittedFrames)
            + m_bufferFrameCount;
        return static_cast<int>(suppliedFrames);
    }

    if(m_bufferEvent && !m_bufferEventAvailable) {
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

    const bool initialPacket    = !m_started && !m_packetSubmitted;
    const HRESULT releaseResult = m_renderClient->ReleaseBuffer(framesToWrite, 0);
    if(FAILED(releaseResult)) {
        handleError(releaseResult, u"Failed to release render buffer"_s, true);
        return 0;
    }

    if(m_bufferEvent) {
        if(initialPacket) {
            ResetEvent(m_bufferEvent);
        }
        m_bufferEventAvailable = false;
        m_packetSubmitted      = true;
    }

    m_clockSubmittedFrames
        = std::min<UINT64>(std::numeric_limits<UINT64>::max() - framesToWrite, m_clockSubmittedFrames) + framesToWrite;

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

    if(m_bufferEvent && !m_bufferEventAvailable) {
        const DWORD waitResult = WaitForSingleObject(m_bufferEvent, 0);
        if(waitResult == WAIT_OBJECT_0) {
            m_bufferEventAvailable = true;
            m_packetSubmitted      = false;
        }
        else if(waitResult == WAIT_FAILED) {
            handleError(HRESULT_FROM_WIN32(GetLastError()), u"Failed to query WASAPI buffer event while resuming"_s,
                        true);
            return;
        }
    }

    if(m_packetSubmitted) {
        start();
    }
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

void WasapiOutput::initialiseMmcss()
{
    if(m_mmcssHandle) {
        return;
    }

    m_mmcssTaskIndex = 0;
    m_mmcssHandle    = AvSetMmThreadCharacteristicsW(L"Pro Audio", &m_mmcssTaskIndex);
    if(!m_mmcssHandle) {
        qCWarning(WASAPI) << "Failed to register WASAPI thread with MMCSS:" << GetLastError();
        return;
    }

    if(!AvSetMmThreadPriority(m_mmcssHandle, AVRT_PRIORITY_HIGH)) {
        qCWarning(WASAPI) << "Failed to raise WASAPI MMCSS priority:" << GetLastError();
    }
}

void WasapiOutput::uninitialiseMmcss()
{
    if(!m_mmcssHandle) {
        return;
    }

    if(!AvRevertMmThreadCharacteristics(m_mmcssHandle)) {
        qCWarning(WASAPI) << "Failed to unregister WASAPI thread from MMCSS:" << GetLastError();
    }
    m_mmcssHandle    = nullptr;
    m_mmcssTaskIndex = 0;
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

    QString activeEndpointId;
    LPWSTR rawDeviceId{nullptr};
    if(SUCCEEDED(m_audioDevice->GetId(&rawDeviceId)) && rawDeviceId) {
        activeEndpointId = QString::fromWCharArray(rawDeviceId);
    }
    if(rawDeviceId) {
        CoTaskMemFree(rawDeviceId);
    }

    {
        const std::scoped_lock lock{m_deviceStateMutex};
        m_activeEndpointId     = activeEndpointId;
        m_followsDefaultDevice = selection.isDefault;
    }

    if(!initAudioClient()) {
        return false;
    }

    registerDeviceNotifications(selection.isDefault);
    return true;
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

    DWORD flags{AUDCLNT_STREAMFLAGS_EVENTCALLBACK};
    if(!m_exclusiveMode) {
        flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
        flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    }

    const auto shareMode = m_exclusiveMode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
    static constexpr REFERENCE_TIME HundredNanosPerMillisecond = 10000;
    static constexpr REFERENCE_TIME RequestedBufferDurationMs  = 250;
    REFERENCE_TIME bufferDuration = m_exclusiveMode ? RequestedBufferDurationMs * HundredNanosPerMillisecond : 0;

    HRESULT formatResult{S_OK};
    WAVEFORMATEX* closestMatch{nullptr};
    if(m_exclusiveMode) {
        if(!findExclusiveFormat()) {
            return handleError(AUDCLNT_E_UNSUPPORTED_FORMAT,
                               u"Exclusive mode does not support the requested format or a compatible fallback"_s);
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
        if(devicePeriod <= 0) {
            devicePeriod = bufferDuration;
        }
    }

    HRESULT initResult
        = m_audioClient->Initialize(shareMode, flags, bufferDuration, devicePeriod, &m_waveFormat.Format, nullptr);

    static constexpr int MaxAlignmentAttempts = 4;
    for(int attempt{1};
        m_exclusiveMode && initResult == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED && attempt <= MaxAlignmentAttempts;
        ++attempt) {
        UINT32 alignedFrameCount{0};
        const HRESULT alignedSizeResult = m_audioClient->GetBufferSize(&alignedFrameCount);
        if(FAILED(alignedSizeResult) || alignedFrameCount == 0 || m_format.sampleRate() <= 0) {
            qCWarning(WASAPI) << "Failed to obtain aligned exclusive buffer size:" << resultToError(alignedSizeResult)
                              << "frames=" << alignedFrameCount;
            break;
        }

        const long double alignedDurationExact = (10000.0L * 1000.0L / static_cast<long double>(m_format.sampleRate()))
                                               * static_cast<long double>(alignedFrameCount);
        const auto alignedDuration             = static_cast<REFERENCE_TIME>(alignedDurationExact + 0.5L);

        qCInfo(WASAPI) << "Retrying exclusive initialization with aligned buffer:"
                       << "attempt=" << attempt << "frames=" << alignedFrameCount
                       << "duration100ns=" << alignedDuration;

        m_audioClient.Reset();
        const HRESULT reactivateResult
            = m_audioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &m_audioClient);
        if(FAILED(reactivateResult)) {
            initResult = reactivateResult;
            break;
        }

        bufferDuration = alignedDuration;
        devicePeriod   = alignedDuration;
        initResult
            = m_audioClient->Initialize(shareMode, flags, bufferDuration, devicePeriod, &m_waveFormat.Format, nullptr);
    }
    if(FAILED(initResult)) {
        return handleError(initResult, u"Failed to initialise audio client"_s);
    }

    const HRESULT sizeResult = m_audioClient->GetBufferSize(&m_bufferFrameCount);
    if(FAILED(sizeResult)) {
        return handleError(sizeResult, u"Failed to query buffer size"_s);
    }
    m_bufferFrames = static_cast<int>(m_bufferFrameCount);

    m_bufferEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if(!m_bufferEvent) {
        return handleError(HRESULT_FROM_WIN32(GetLastError()), u"Failed to create WASAPI buffer event"_s);
    }

    const HRESULT eventResult = m_audioClient->SetEventHandle(m_bufferEvent);
    if(FAILED(eventResult)) {
        return handleError(eventResult, u"Failed to configure WASAPI buffer event"_s);
    }
    m_bufferEventAvailable = true;
    m_packetSubmitted      = false;

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

    qCInfo(WASAPI) << "WASAPI stream configured:"
                   << (m_exclusiveMode ? "exclusive event-driven" : "shared event-driven")
                   << "bufferFrames=" << m_bufferFrameCount << "bufferMs=" << (m_latency * 1000.0)
                   << "devicePeriod100ns=" << devicePeriod;

    const HRESULT renderResult = m_audioClient->GetService(__uuidof(IAudioRenderClient), &m_renderClient);
    if(FAILED(renderResult)) {
        return handleError(renderResult, u"Failed to acquire render client"_s);
    }

    const HRESULT clockResult = m_audioClient->GetService(__uuidof(IAudioClock), &m_audioClock);
    if(FAILED(clockResult)) {
        qCWarning(WASAPI) << "Failed to get WASAPI clock:" << resultToError(clockResult);
    }
    else {
        const HRESULT frequencyResult = m_audioClock->GetFrequency(&m_clockFrequency);
        if(FAILED(frequencyResult) || m_clockFrequency == 0) {
            qCWarning(WASAPI) << "Failed to get WASAPI clock frequency:" << resultToError(frequencyResult);
            m_audioClock.Reset();
            m_clockFrequency = 0;
        }
        else {
            resetClockTracking();
        }
    }

    if(!m_exclusiveMode) {
        const HRESULT volumeResult = m_audioClient->GetService(__uuidof(ISimpleAudioVolume), &m_audioVolume);
        if(FAILED(volumeResult)) {
            qCWarning(WASAPI) << "Failed to get WASAPI volume control:" << resultToError(volumeResult);
        }
    }

    return true;
}

bool WasapiOutput::findExclusiveFormat()
{
    if(!m_audioClient) {
        return false;
    }

    if(m_audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &m_waveFormat.Format, nullptr) == S_OK) {
        return true;
    }

    std::vector<int> sampleRates;
    const auto addSampleRate = [&sampleRates](const int sampleRate) {
        if(sampleRate > 0 && std::ranges::find(sampleRates, sampleRate) == sampleRates.cend()) {
            sampleRates.push_back(sampleRate);
        }
    };

    addSampleRate(m_format.sampleRate());

    WAVEFORMATEX* mixFormat{nullptr};
    if(SUCCEEDED(m_audioClient->GetMixFormat(&mixFormat)) && mixFormat) {
        addSampleRate(static_cast<int>(mixFormat->nSamplesPerSec));
    }
    if(mixFormat) {
        CoTaskMemFree(mixFormat);
    }

    static constexpr std::array CommonSampleRates{44100,  48000, 88200,  96000,  176400, 192000, 352800,
                                                  384000, 22050, 24000,  32000,  16000,  12000,  11025,
                                                  8000,   64000, 128000, 256000, 705600, 768000};
    for(const int sampleRate : CommonSampleRates) {
        addSampleRate(sampleRate);
    }

    std::vector<SampleFormat> sampleFormats;
    const auto addSampleFormat = [&sampleFormats](const SampleFormat sampleFormat) {
        if(std::ranges::find(sampleFormats, sampleFormat) == sampleFormats.cend()) {
            sampleFormats.push_back(sampleFormat);
        }
    };

    addSampleFormat(m_format.sampleFormat());
    for(const SampleFormat sampleFormat :
        {SampleFormat::S32, SampleFormat::F32, SampleFormat::S24In32, SampleFormat::S16, SampleFormat::U8}) {
        addSampleFormat(sampleFormat);
    }

    const AudioFormat requestedFormat = m_format;
    for(const SampleFormat sampleFormat : sampleFormats) {
        for(const int sampleRate : sampleRates) {
            AudioFormat candidate = requestedFormat;
            candidate.setSampleFormat(sampleFormat);
            candidate.setSampleRate(sampleRate);

            const WAVEFORMATEXTENSIBLE waveFormat = createWaveFormat(candidate);
            if(m_audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &waveFormat.Format, nullptr) != S_OK) {
                continue;
            }

            m_format     = candidate;
            m_waveFormat = waveFormat;
            qCInfo(WASAPI) << "Using compatible exclusive format:" << m_format.prettyFormat() << m_format.sampleRate()
                           << "Hz," << m_format.channelCount() << "channels";
            return true;
        }
    }

    return false;
}

void WasapiOutput::registerDeviceNotifications(const bool followsDefaultDevice)
{
    if(!m_deviceEnumerator || m_notificationClient) {
        return;
    }

    {
        const std::scoped_lock lock{m_deviceStateMutex};
        m_followsDefaultDevice = followsDefaultDevice;
    }

    m_notificationClient.Attach(new DeviceNotificationClient{this});
    const HRESULT result = m_deviceEnumerator->RegisterEndpointNotificationCallback(m_notificationClient.Get());
    if(FAILED(result)) {
        qCWarning(WASAPI) << "Failed to register endpoint notifications:" << resultToError(result);
        static_cast<DeviceNotificationClient*>(m_notificationClient.Get())->detach();
        m_notificationClient.Reset();
    }
}

void WasapiOutput::unregisterDeviceNotifications()
{
    if(!m_notificationClient) {
        return;
    }

    static_cast<DeviceNotificationClient*>(m_notificationClient.Get())->detach();
    if(m_deviceEnumerator) {
        const HRESULT result = m_deviceEnumerator->UnregisterEndpointNotificationCallback(m_notificationClient.Get());
        if(FAILED(result)) {
            qCWarning(WASAPI) << "Failed to unregister endpoint notifications:" << resultToError(result);
        }
    }
    m_notificationClient.Reset();
}

void WasapiOutput::handleDeviceStateChanged(const QString& deviceId, const DWORD state)
{
    if(state == DEVICE_STATE_ACTIVE) {
        return;
    }

    bool isActiveDevice{false};
    {
        const std::scoped_lock lock{m_deviceStateMutex};
        isActiveDevice = !deviceId.isEmpty() && deviceId == m_activeEndpointId;
    }

    if(isActiveDevice) {
        notifyDisconnected(u"WASAPI output device became unavailable"_s);
    }
}

void WasapiOutput::handleDeviceRemoved(const QString& deviceId)
{
    bool isActiveDevice{false};
    {
        const std::scoped_lock lock{m_deviceStateMutex};
        isActiveDevice = !deviceId.isEmpty() && deviceId == m_activeEndpointId;
    }

    if(isActiveDevice) {
        notifyDisconnected(u"WASAPI output device was removed"_s);
    }
}

void WasapiOutput::handleDefaultDeviceChanged(const EDataFlow flow, const ERole role, const QString& deviceId)
{
    if(flow != eRender || role != eConsole) {
        return;
    }

    bool defaultChanged{false};
    {
        const std::scoped_lock lock{m_deviceStateMutex};
        defaultChanged = m_followsDefaultDevice && deviceId != m_activeEndpointId;
    }

    if(defaultChanged) {
        notifyDisconnected(u"Default WASAPI output device changed"_s);
    }
}

void WasapiOutput::notifyDisconnected(const QString& reason)
{
    if(m_disconnectNotified.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    if(!reason.isEmpty()) {
        qCWarning(WASAPI) << reason;
    }
    QMetaObject::invokeMethod(
        this, [this]() { Q_EMIT stateChanged(AudioOutput::State::Disconnected); }, Qt::QueuedConnection);
}

void WasapiOutput::resetClockTracking()
{
    m_clockSubmittedFrames = 0;
    m_clockBasePosition    = 0;
    m_clockBaseValid       = false;

    if(!m_audioClock || m_clockFrequency == 0) {
        return;
    }

    UINT64 position{0};
    if(SUCCEEDED(m_audioClock->GetPosition(&position, nullptr))) {
        m_clockBasePosition = position;
        m_clockBaseValid    = true;
    }
}

std::optional<UINT32> WasapiOutput::clockQueuedFrames(const UINT32 paddingFrames)
{
    if(!m_audioClock || m_clockFrequency == 0 || m_format.sampleRate() <= 0) {
        return std::nullopt;
    }

    UINT64 position{0};
    if(FAILED(m_audioClock->GetPosition(&position, nullptr))) {
        return std::nullopt;
    }

    if(!m_clockBaseValid || position < m_clockBasePosition) {
        m_clockBasePosition    = position;
        m_clockSubmittedFrames = paddingFrames;
        m_clockBaseValid       = true;
        return paddingFrames;
    }

    const UINT64 positionDelta = position - m_clockBasePosition;
    const long double played = static_cast<long double>(positionDelta) * static_cast<long double>(m_format.sampleRate())
                             / static_cast<long double>(m_clockFrequency);
    const UINT64 playedFrames = played >= static_cast<long double>(std::numeric_limits<UINT64>::max())
                                  ? std::numeric_limits<UINT64>::max()
                                  : static_cast<UINT64>(played);

    if(playedFrames >= m_clockSubmittedFrames) {
        m_clockBasePosition    = position;
        m_clockSubmittedFrames = paddingFrames;
        return paddingFrames;
    }

    const UINT64 outstanding = m_clockSubmittedFrames - playedFrames;
    const UINT64 queued      = std::max<UINT64>(paddingFrames, outstanding);
    return static_cast<UINT32>(std::min<UINT64>(queued, m_bufferFrameCount));
}

void WasapiOutput::uninitDevice()
{
    unregisterDeviceNotifications();

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

    if(m_bufferEvent) {
        CloseHandle(m_bufferEvent);
        m_bufferEvent = nullptr;
    }
    m_audioDevice.Reset();
    m_deviceEnumerator.Reset();

    m_started              = false;
    m_exclusiveMode        = false;
    m_bufferFrames         = 0;
    m_bufferFrameCount     = 0;
    m_latency              = 0.0;
    m_clockFrequency       = 0;
    m_clockBasePosition    = 0;
    m_clockSubmittedFrames = 0;
    m_clockBaseValid       = false;
    m_bufferEventAvailable = false;
    m_packetSubmitted      = false;
    m_waveFormat           = {};

    {
        const std::scoped_lock lock{m_deviceStateMutex};
        m_activeEndpointId.clear();
        m_followsDefaultDevice = false;
    }

    if(m_comInitialised) {
        CoUninitialize();
        m_comInitialised = false;
    }

    uninitialiseMmcss();
}

bool WasapiOutput::handleError(const HRESULT result, const QString& action, const bool runtime)
{
    m_error = u"%1: %2"_s.arg(action, resultToError(result));
    qCWarning(WASAPI) << m_error;

    if(runtime) {
        if(result == AUDCLNT_E_DEVICE_INVALIDATED || result == AUDCLNT_E_RESOURCES_INVALIDATED) {
            notifyDisconnected({});
        }
        else {
            QMetaObject::invokeMethod(
                this, [this]() { Q_EMIT stateChanged(AudioOutput::State::Error); }, Qt::QueuedConnection);
        }
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

    WAVEFORMATEX* mixFormat{nullptr};
    if(SUCCEEDED(audioClient->GetMixFormat(&mixFormat)) && mixFormat) {
        const bool supportsMixFormat
            = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, mixFormat, nullptr) == S_OK;
        CoTaskMemFree(mixFormat);
        if(supportsMixFormat) {
            return true;
        }
    }

    static constexpr std::array sampleRates{44100, 48000, 96000, 192000, 88200,  176400, 32000,  24000, 22050,
                                            16000, 12000, 11025, 8000,   352800, 384000, 705600, 768000};
    static constexpr std::array channelCounts{2, 1, 4, 6, 8, 3, 5, 7};
    static constexpr std::array sampleFormats{SampleFormat::S16, SampleFormat::S24In32, SampleFormat::S32,
                                              SampleFormat::F32, SampleFormat::U8};

    for(const int sampleRate : sampleRates) {
        for(const int channelCount : channelCounts) {
            for(const SampleFormat sampleFormat : sampleFormats) {
                const WAVEFORMATEXTENSIBLE waveFormat
                    = createWaveFormat(AudioFormat{sampleFormat, sampleRate, channelCount});
                if(audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE, &waveFormat.Format, nullptr) == S_OK) {
                    return true;
                }
            }
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
            waveFormat.Samples.wValidBitsPerSample = 64;
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
