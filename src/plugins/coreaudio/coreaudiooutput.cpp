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

#include "coreaudiooutput.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <QByteArray>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QString>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <limits>
#include <optional>
#include <string>

Q_LOGGING_CATEGORY(COREAUDIO, "fy.coreaudio")

using namespace Qt::StringLiterals;

constexpr auto TargetBufferLengthMs = 200;
constexpr auto QueueBufferCount     = 4;
constexpr auto DefaultDeviceId      = "default";

namespace {
QString cfStringToQString(CFStringRef string)
{
    if(!string) {
        return {};
    }

    const CFIndex length       = CFStringGetLength(string);
    const CFIndex maxByteCount = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string buffer(static_cast<size_t>(std::max<CFIndex>(1, maxByteCount)), '\0');

    if(!CFStringGetCString(string, buffer.data(), maxByteCount, kCFStringEncodingUTF8)) {
        return {};
    }

    return QString::fromUtf8(buffer.c_str());
}

CFStringRef qStringToCfString(const QString& string)
{
    const QByteArray utf8 = string.toUtf8();
    return CFStringCreateWithCString(kCFAllocatorDefault, utf8.constData(), kCFStringEncodingUTF8);
}

QString osStatusToString(const OSStatus status)
{
    const auto code = static_cast<uint32_t>(status);
    std::array<char, 5> fourcc{};

    fourcc[0] = static_cast<char>((code >> 24U) & 0xFFU);
    fourcc[1] = static_cast<char>((code >> 16U) & 0xFFU);
    fourcc[2] = static_cast<char>((code >> 8U) & 0xFFU);
    fourcc[3] = static_cast<char>(code & 0xFFU);

    const bool printable = std::ranges::all_of(
        std::span{fourcc}.first(4), [](const char ch) { return std::isprint(static_cast<unsigned char>(ch)) != 0; });

    return printable ? u"'%1'"_s.arg(QString::fromLatin1(fourcc.data(), 4)) : QString::number(status);
}

std::optional<QString> readStringProperty(AudioObjectID objectId, const AudioObjectPropertyAddress& address)
{
    CFStringRef value{nullptr};
    UInt32 size = sizeof(CFStringRef);

    if(AudioObjectGetPropertyData(objectId, &address, 0, nullptr, &size, &value) != noErr || !value) {
        return {};
    }

    QString result = cfStringToQString(value);
    CFRelease(value);
    return result;
}

bool deviceHasOutput(AudioDeviceID deviceId)
{
    const AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain,
    };

    UInt32 size = 0;
    if(AudioObjectGetPropertyDataSize(deviceId, &address, 0, nullptr, &size) != noErr || size == 0) {
        return false;
    }

    std::vector<std::byte> storage(size);
    auto* bufferList = reinterpret_cast<AudioBufferList*>(storage.data());
    if(AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, bufferList) != noErr) {
        return false;
    }

    UInt32 channels = 0;
    for(UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
        channels += bufferList->mBuffers[i].mNumberChannels;
    }

    return channels > 0;
}

Fooyin::OutputDevices enumerateOutputDevices()
{
    Fooyin::OutputDevices devices;
    devices.emplace_back(QString::fromLatin1(DefaultDeviceId), u"Default"_s);

    const AudioObjectPropertyAddress devicesAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };

    UInt32 size{0};
    if(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &devicesAddress, 0, nullptr, &size) != noErr
       || size == 0) {
        return devices;
    }

    const size_t deviceCount = size / sizeof(AudioDeviceID);
    std::vector<AudioDeviceID> deviceIds(deviceCount);
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject, &devicesAddress, 0, nullptr, &size, deviceIds.data())
       != noErr) {
        return devices;
    }

    const AudioObjectPropertyAddress uidAddress = {
        kAudioDevicePropertyDeviceUID,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    const AudioObjectPropertyAddress nameAddress = {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };

    for(const AudioDeviceID deviceId : deviceIds) {
        if(!deviceHasOutput(deviceId)) {
            continue;
        }

        const auto uid  = readStringProperty(deviceId, uidAddress);
        const auto name = readStringProperty(deviceId, nameAddress);
        if(!uid || uid->isEmpty() || !name || name->isEmpty()) {
            continue;
        }

        devices.emplace_back(*uid, *name);
    }

    return devices;
}

bool findSampleFormat(Fooyin::SampleFormat format, AudioStreamBasicDescription& desc)
{
    desc.mFormatID        = kAudioFormatLinearPCM;
    desc.mFramesPerPacket = 1;
    desc.mFormatFlags     = kAudioFormatFlagIsPacked;
    desc.mReserved        = 0;

    switch(format) {
        case Fooyin::SampleFormat::U8:
            desc.mBitsPerChannel = 8;
            break;
        case Fooyin::SampleFormat::S16:
            desc.mBitsPerChannel = 16;
            desc.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
            break;
        case Fooyin::SampleFormat::S24In32:
        case Fooyin::SampleFormat::S32:
            desc.mBitsPerChannel = 32;
            desc.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
            break;
        case Fooyin::SampleFormat::F32:
            desc.mBitsPerChannel = 32;
            desc.mFormatFlags |= kAudioFormatFlagIsFloat;
            break;
        case Fooyin::SampleFormat::F64:
        case Fooyin::SampleFormat::Unknown:
        default:
            return false;
    }

    return true;
}
} // namespace

namespace Fooyin::CoreAudio {
CoreAudioOutput::CoreAudioOutput()
    : m_device{QString::fromLatin1(DefaultDeviceId)}
    , m_volume{1.0}
    , m_ringBufferFrames{0}
    , m_queueBufferFrames{0}
    , m_queue{nullptr}
    , m_enqueuedBytes{0}
    , m_initialised{false}
    , m_started{false}
    , m_paused{false}
    , m_draining{false}
    , m_stopping{false}
{ }

bool CoreAudioOutput::init(const AudioFormat& format)
{
    m_error.clear();
    m_format = format;

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(!m_format.isValid() || bytesPerFrame <= 0 || m_format.sampleRate() <= 0 || m_format.channelCount() <= 0) {
        m_error = u"Invalid output format for CoreAudio"_s;
        qCWarning(COREAUDIO) << m_error;
        return false;
    }

    m_queueBufferFrames = std::max(1, m_format.framesForDuration(TargetBufferLengthMs) / QueueBufferCount);
    m_ringBufferFrames  = std::max(m_queueBufferFrames * QueueBufferCount * 2, m_queueBufferFrames);

    const auto capacityBytes64 = static_cast<uint64_t>(m_ringBufferFrames) * static_cast<uint64_t>(bytesPerFrame);
    static constexpr auto maxCapacity = std::numeric_limits<size_t>::max() - 1;
    const size_t capacityBytes
        = std::clamp<uint64_t>(capacityBytes64, static_cast<uint64_t>(bytesPerFrame), maxCapacity);

    m_buffer = std::make_unique<LockFreeRingBuffer<std::byte>>(capacityBytes);

    if(!initQueue()) {
        disposeQueue();
        m_buffer.reset();
        m_ringBufferFrames  = 0;
        m_queueBufferFrames = 0;
        return false;
    }

    m_initialised = true;
    return true;
}

void CoreAudioOutput::uninit()
{
    if(!m_initialised && !m_queue && !m_buffer) {
        return;
    }

    disposeQueue();
    m_buffer.reset();
    m_ringBufferFrames  = 0;
    m_queueBufferFrames = 0;
    m_initialised       = false;
}

void CoreAudioOutput::reset()
{
    if(!m_queue) {
        if(m_buffer) {
            m_buffer->requestReset();
        }
        resetQueueState();
        return;
    }

    m_stopping = true;
    AudioQueuePause(m_queue);
    AudioQueueReset(m_queue);
    m_stopping = false;

    if(m_buffer) {
        m_buffer->requestReset();
    }
    resetQueueState();
}

void CoreAudioOutput::start()
{
    if(!m_queue) {
        return;
    }

    m_draining = false;
    if(!primeQueue(true)) {
        qCDebug(COREAUDIO) << "CoreAudio queue start skipped; no buffers could be primed";
    }

    if(m_started && !m_paused) {
        return;
    }

    const OSStatus status = AudioQueueStart(m_queue, nullptr);
    if(status != noErr) {
        handleQueueError(status, "start output queue");
        return;
    }

    m_started = true;
    m_paused  = false;
}

void CoreAudioOutput::drain()
{
    if(!m_queue) {
        return;
    }

    if(!m_started) {
        start();
    }

    m_draining = true;
    {
        std::unique_lock lock{m_stateMutex};
        m_stateCv.wait_for(lock, std::chrono::seconds{2}, [this]() {
            const size_t softwareQueued = m_buffer ? m_buffer->readAvailable() : 0;
            return softwareQueued == 0 && queuedHardwareBytes() == 0;
        });
    }
    m_draining = false;
}

bool CoreAudioOutput::initialised() const
{
    return m_initialised;
}

QString CoreAudioOutput::device() const
{
    return m_device;
}

int CoreAudioOutput::bufferSize() const
{
    return m_ringBufferFrames;
}

OutputState CoreAudioOutput::currentState()
{
    OutputState state;

    const int bytesPerFrame = m_format.bytesPerFrame();
    const int sampleRate    = m_format.sampleRate();
    if(bytesPerFrame <= 0 || sampleRate <= 0) {
        return state;
    }

    const size_t softwareQueued = m_buffer ? m_buffer->readAvailable() : 0;
    const size_t hardwareQueued = queuedHardwareBytes();
    const size_t totalQueued    = softwareQueued + hardwareQueued;

    state.queuedFrames = static_cast<int>(totalQueued / static_cast<size_t>(bytesPerFrame));
    state.freeFrames = m_buffer ? static_cast<int>(m_buffer->writeAvailable() / static_cast<size_t>(bytesPerFrame)) : 0;
    state.delay      = static_cast<double>(state.queuedFrames) / static_cast<double>(sampleRate);

    return state;
}

OutputDevices CoreAudioOutput::getAllDevices(bool /*isCurrentOutput*/)
{
    return enumerateOutputDevices();
}

int CoreAudioOutput::write(const std::span<const std::byte> data, const int frameCount)
{
    if(!m_buffer || frameCount <= 0) {
        return 0;
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return 0;
    }

    const size_t availableFrames = data.size() / static_cast<size_t>(bytesPerFrame);
    const int framesToWrite      = std::min(frameCount, static_cast<int>(availableFrames));
    if(framesToWrite <= 0) {
        return 0;
    }

    const int freeFrames     = static_cast<int>(m_buffer->writeAvailable() / static_cast<size_t>(bytesPerFrame));
    const int acceptedFrames = std::min(framesToWrite, freeFrames);
    if(acceptedFrames <= 0) {
        return 0;
    }

    const size_t acceptedBytes = static_cast<size_t>(acceptedFrames) * static_cast<size_t>(bytesPerFrame);
    auto writer                = m_buffer->writer();
    const size_t writtenBytes  = writer.write(data.data(), acceptedBytes, RingBufferOverflowPolicy::DropNewest);
    const int writtenFrames    = static_cast<int>(writtenBytes / static_cast<size_t>(bytesPerFrame));

    if(writtenFrames > 0 && m_started && !m_paused) {
        primeQueue(false);
    }

    return writtenFrames;
}

void CoreAudioOutput::setPaused(bool pause)
{
    if(!m_queue) {
        return;
    }

    if(pause) {
        AudioQueuePause(m_queue);
        m_paused = true;
        return;
    }

    if(!m_started) {
        start();
        return;
    }

    const OSStatus status = AudioQueueStart(m_queue, nullptr);
    if(status != noErr) {
        handleQueueError(status, "resume output queue");
        return;
    }

    m_paused = false;
}

void CoreAudioOutput::setVolume(double volume)
{
    m_volume = std::clamp(volume, 0.0, 1.0);
    applyQueueVolume();
}

bool CoreAudioOutput::supportsVolumeControl() const
{
    return true;
}

void CoreAudioOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

AudioFormat CoreAudioOutput::negotiateFormat(const AudioFormat& requested) const
{
    if(!requested.isValid()) {
        return requested;
    }

    AudioFormat negotiated = requested;
    switch(requested.sampleFormat()) {
        case SampleFormat::F64:
            negotiated.setSampleFormat(SampleFormat::F32);
            break;
        case SampleFormat::Unknown:
            negotiated.setSampleFormat(SampleFormat::S16);
            break;
        default:
            break;
    }

    return negotiated;
}

QString CoreAudioOutput::error() const
{
    return m_error;
}

AudioFormat CoreAudioOutput::format() const
{
    return m_format;
}

bool CoreAudioOutput::initQueue()
{
    AudioStreamBasicDescription desc{};
    desc.mSampleRate       = static_cast<Float64>(m_format.sampleRate());
    desc.mChannelsPerFrame = static_cast<UInt32>(m_format.channelCount());
    desc.mBytesPerFrame    = static_cast<UInt32>(m_format.bytesPerFrame());
    desc.mBytesPerPacket   = desc.mBytesPerFrame;

    if(!findSampleFormat(m_format.sampleFormat(), desc)) {
        m_error = u"Unsupported CoreAudio sample format"_s;
        qCWarning(COREAUDIO) << m_error << m_format.prettyFormat();
        return false;
    }

    const OSStatus createStatus = AudioQueueNewOutput(&desc, outputCallback, this, nullptr, nullptr, 0, &m_queue);
    if(createStatus != noErr) {
        handleQueueError(createStatus, "create output queue");
        return false;
    }

    if(m_device != QLatin1String(DefaultDeviceId)) {
        CFStringRef deviceUid = qStringToCfString(m_device);
        if(!deviceUid) {
            m_error = u"Failed to convert CoreAudio device UID"_s;
            return false;
        }

        const OSStatus propertyStatus = AudioQueueSetProperty(m_queue, kAudioQueueProperty_CurrentDevice, &deviceUid,
                                                              static_cast<UInt32>(sizeof(CFStringRef)));
        CFRelease(deviceUid);

        if(propertyStatus != noErr) {
            handleQueueError(propertyStatus, "set output device");
            return false;
        }
    }

    if(!applyQueueVolume()) {
        return false;
    }

    const auto bufferByteCount
        = static_cast<UInt32>(static_cast<size_t>(m_queueBufferFrames) * static_cast<size_t>(m_format.bytesPerFrame()));
    m_queueBuffers.resize(QueueBufferCount);

    for(auto& state : m_queueBuffers) {
        const OSStatus allocStatus = AudioQueueAllocateBuffer(m_queue, bufferByteCount, &state.buffer);

        if(allocStatus != noErr || !state.buffer) {
            handleQueueError(allocStatus, "allocate output buffer");
            return false;
        }
    }

    resetQueueState();
    return true;
}

void CoreAudioOutput::resetQueueState()
{
    const std::scoped_lock lock{m_stateMutex};

    m_enqueuedBytes.store(0, std::memory_order_release);

    for(auto& state : m_queueBuffers) {
        state.queuedBytes = 0;
    }

    m_started  = false;
    m_paused   = false;
    m_draining = false;

    m_stateCv.notify_all();
}

void CoreAudioOutput::disposeQueue()
{
    if(!m_queue) {
        resetQueueState();
        return;
    }

    m_stopping = true;
    AudioQueueStop(m_queue, true);
    AudioQueueDispose(m_queue, true);
    m_stopping = false;

    m_queue = nullptr;
    m_queueBuffers.clear();
    resetQueueState();
}

bool CoreAudioOutput::primeQueue(bool allowSilence)
{
    if(!m_queue) {
        return false;
    }

    bool queuedAny{false};
    const std::scoped_lock lock{m_stateMutex};

    for(auto& state : m_queueBuffers) {
        if(state.queuedBytes != 0) {
            continue;
        }

        queuedAny = refillBuffer(state, allowSilence) || queuedAny;
    }

    return queuedAny;
}

bool CoreAudioOutput::refillBuffer(QueueBufferState& state, bool allowSilence)
{
    if(!state.buffer || !m_queue) {
        return false;
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(bytesPerFrame <= 0 || state.buffer->mAudioDataBytesCapacity == 0) {
        return false;
    }

    const size_t capacity = state.buffer->mAudioDataBytesCapacity;
    size_t readBytes{0};

    if(m_buffer) {
        auto reader = m_buffer->reader();
        readBytes   = reader.read(reinterpret_cast<std::byte*>(state.buffer->mAudioData), capacity);
    }

    if(allowSilence && readBytes < capacity) {
        std::memset(static_cast<std::byte*>(state.buffer->mAudioData) + static_cast<std::ptrdiff_t>(readBytes), 0,
                    capacity - readBytes);
        readBytes = capacity;
    }

    if(readBytes == 0) {
        state.buffer->mAudioDataByteSize = 0;
        return false;
    }

    state.buffer->mAudioDataByteSize = static_cast<UInt32>(readBytes);

    const OSStatus status = AudioQueueEnqueueBuffer(m_queue, state.buffer, 0, nullptr);
    if(status != noErr) {
        handleQueueError(status, "enqueue output buffer");
        state.buffer->mAudioDataByteSize = 0;
        return false;
    }

    state.queuedBytes = readBytes;
    m_enqueuedBytes.fetch_add(readBytes, std::memory_order_acq_rel);
    return true;
}

void CoreAudioOutput::onBufferCompleted(AudioQueueBufferRef buffer)
{
    if(m_stopping) {
        return;
    }

    const std::scoped_lock lock{m_stateMutex};
    auto it = std::ranges::find_if(m_queueBuffers,
                                   [buffer](const QueueBufferState& state) { return state.buffer == buffer; });
    if(it == m_queueBuffers.end()) {
        return;
    }

    if(it->queuedBytes > 0) {
        m_enqueuedBytes.fetch_sub(it->queuedBytes, std::memory_order_acq_rel);
        it->queuedBytes = 0;
    }

    const bool allowSilence = !m_draining;
    refillBuffer(*it, allowSilence);

    m_stateCv.notify_all();
}

void CoreAudioOutput::handleQueueError(OSStatus status, const char* action)
{
    m_error = u"Failed to %1: %2"_s.arg(QString::fromLatin1(action), osStatusToString(status));
    qCWarning(COREAUDIO) << m_error;

    QMetaObject::invokeMethod(this, [this]() { Q_EMIT stateChanged(AudioOutput::State::Error); }, Qt::QueuedConnection);
}

bool CoreAudioOutput::applyQueueVolume()
{
    if(!m_queue) {
        return true;
    }

    const auto value = static_cast<AudioQueueParameterValue>(m_volume);

    const OSStatus status = AudioQueueSetParameter(m_queue, kAudioQueueParam_Volume, value);
    if(status != noErr) {
        handleQueueError(status, "set output volume");
        return false;
    }

    return true;
}

size_t CoreAudioOutput::queuedHardwareBytes() const
{
    return m_enqueuedBytes.load(std::memory_order_acquire);
}

void CoreAudioOutput::outputCallback(void* userData, AudioQueueRef /*queue*/, AudioQueueBufferRef buffer)
{
    if(auto* self = static_cast<CoreAudioOutput*>(userData)) {
        self->onBufferCompleted(buffer);
    }
}
} // namespace Fooyin::CoreAudio
