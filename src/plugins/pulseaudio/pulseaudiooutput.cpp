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

#include "pulseaudiooutput.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cstddef>
#include <utility>

Q_LOGGING_CATEGORY(PULSE, "fy.pulseaudio")

using namespace Qt::StringLiterals;

constexpr auto PulseTargetBufferMs = 200;
constexpr auto PulsePeriodMs       = 40;

namespace Fooyin::PulseAudio {
namespace {
pa_sample_format findPulseFormat(SampleFormat format)
{
    switch(format) {
        case SampleFormat::U8:
            return PA_SAMPLE_U8;
        case SampleFormat::S16:
            return PA_SAMPLE_S16NE;
        case SampleFormat::S24In32:
        case SampleFormat::S32:
            return PA_SAMPLE_S32NE;
        case SampleFormat::F32:
            return PA_SAMPLE_FLOAT32NE;
        case SampleFormat::F64:
        case SampleFormat::Unknown:
        default:
            return PA_SAMPLE_INVALID;
    }
}

AudioFormat::ChannelPosition channelPositionFromPulse(pa_channel_position_t pos)
{
    using P = AudioFormat::ChannelPosition;

    switch(pos) {
        case PA_CHANNEL_POSITION_MONO:
            return P::FrontCenter;
        case PA_CHANNEL_POSITION_FRONT_LEFT:
            return P::FrontLeft;
        case PA_CHANNEL_POSITION_FRONT_RIGHT:
            return P::FrontRight;
        case PA_CHANNEL_POSITION_FRONT_CENTER:
            return P::FrontCenter;
        case PA_CHANNEL_POSITION_LFE:
            return P::LFE;
        case PA_CHANNEL_POSITION_REAR_LEFT:
            return P::BackLeft;
        case PA_CHANNEL_POSITION_REAR_RIGHT:
            return P::BackRight;
        case PA_CHANNEL_POSITION_REAR_CENTER:
            return P::BackCenter;
        case PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER:
            return P::FrontLeftOfCenter;
        case PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER:
            return P::FrontRightOfCenter;
        case PA_CHANNEL_POSITION_SIDE_LEFT:
            return P::SideLeft;
        case PA_CHANNEL_POSITION_SIDE_RIGHT:
            return P::SideRight;
        case PA_CHANNEL_POSITION_TOP_CENTER:
            return P::TopCenter;
        case PA_CHANNEL_POSITION_TOP_FRONT_LEFT:
            return P::TopFrontLeft;
        case PA_CHANNEL_POSITION_TOP_FRONT_RIGHT:
            return P::TopFrontRight;
        case PA_CHANNEL_POSITION_TOP_FRONT_CENTER:
            return P::TopFrontCenter;
        case PA_CHANNEL_POSITION_TOP_REAR_LEFT:
            return P::TopBackLeft;
        case PA_CHANNEL_POSITION_TOP_REAR_RIGHT:
            return P::TopBackRight;
        case PA_CHANNEL_POSITION_TOP_REAR_CENTER:
            return P::TopBackCenter;
        default:
            return P::UnknownPosition;
    }
}

AudioFormat::ChannelLayout channelLayoutFromPulseMap(const pa_channel_map& map)
{
    if(map.channels == 0) {
        return {};
    }

    AudioFormat::ChannelLayout layout;
    layout.reserve(map.channels);

    for(uint8_t i{0}; i < map.channels; ++i) {
        layout.push_back(channelPositionFromPulse(map.map[i]));
    }

    return layout;
}

struct SinkContext
{
    OutputDevices devices;
    pa_threaded_mainloop* mainLoop;
};

void sinkInfoRequestCallback(pa_context* /*c*/, const pa_sink_info* i, int /*eol*/, void* userdata)
{
    auto* context = static_cast<SinkContext*>(userdata);
    if(!context) {
        return;
    }

    if(context->devices.empty()) {
        context->devices.emplace_back(u"default"_s, u"Default"_s);
    }

    if(i && i->name) {
        context->devices.emplace_back(QString::fromUtf8(i->name),
                                      i->description ? QString::fromUtf8(i->description) : QString::fromUtf8(i->name));
    }

    pa_threaded_mainloop_signal(context->mainLoop, 0);
}

struct SinkInfoContext
{
    bool deviceFound{false};
    pa_threaded_mainloop* mainloop;
    pa_channel_map map;

    SinkInfoContext()
    {
        pa_channel_map_init(&map);
    }
};

void sinkInfoCallback(pa_context* /*c*/, const pa_sink_info* i, int /*eol*/, void* userdata)
{
    auto* context = static_cast<SinkInfoContext*>(userdata);
    if(!context) {
        return;
    }

    if(i) {
        context->deviceFound = true;
        context->map         = i->channel_map;
    }

    pa_threaded_mainloop_signal(context->mainloop, 0);
}

void streamRequestCallback(pa_stream* /*s*/, size_t /*length*/, void* userdata)
{
    auto* m = static_cast<pa_threaded_mainloop*>(userdata);
    pa_threaded_mainloop_signal(m, 0);
}

void streamStateCallback(pa_stream* /*s*/, void* userdata)
{
    auto* m = static_cast<pa_threaded_mainloop*>(userdata);
    pa_threaded_mainloop_signal(m, 0);
}

void successCallback(pa_stream* /*s*/, int /*success*/, void* userdata)
{
    auto* loop = static_cast<pa_threaded_mainloop*>(userdata);
    pa_threaded_mainloop_signal(loop, 0);
}

} // namespace

PulseAudioOutput::PulseAudioOutput()
    : m_initialised{false}
    , m_volume{1.0}
    , m_device{u"default"_s}
    , m_stream{nullptr}
    , m_targetBufferFrames{0}
{ }

PulseAudioOutput::~PulseAudioOutput()
{
    resetPulse();
}

bool PulseAudioOutput::init(const AudioFormat& format)
{
    resetPulse();
    m_format = format;

    if(!initPulse()) {
        uninit();
        return false;
    }

    m_initialised = true;
    return true;
}

void PulseAudioOutput::uninit()
{
    resetPulse();
}

void PulseAudioOutput::reset()
{
    if(!m_mainLoop || !m_stream) {
        return;
    }

    pa_threaded_mainloop_lock(m_mainLoop.get());
    waitForOperation(pa_stream_flush(m_stream, successCallback, m_mainLoop.get()), m_mainLoop.get(),
                     u"Failed to flush PulseAudio stream"_s);
    waitForOperation(pa_stream_cork(m_stream, true, successCallback, m_mainLoop.get()), m_mainLoop.get(),
                     u"Failed to cork PulseAudio stream"_s);
    pa_threaded_mainloop_unlock(m_mainLoop.get());
}

void PulseAudioOutput::start()
{
    if(!m_mainLoop || !m_stream) {
        return;
    }

    pa_threaded_mainloop_lock(m_mainLoop.get());
    waitForOperation(pa_stream_cork(m_stream, false, successCallback, m_mainLoop.get()), m_mainLoop.get(),
                     u"Failed to uncork PulseAudio stream"_s);
    pa_threaded_mainloop_unlock(m_mainLoop.get());
}

void PulseAudioOutput::drain()
{
    if(!m_mainLoop || !m_stream) {
        return;
    }

    pa_threaded_mainloop_lock(m_mainLoop.get());
    waitForOperation(pa_stream_drain(m_stream, successCallback, m_mainLoop.get()), m_mainLoop.get(),
                     u"Failed to drain PulseAudio stream"_s);
    pa_threaded_mainloop_unlock(m_mainLoop.get());
}

bool PulseAudioOutput::initialised() const
{
    return m_initialised;
}

QString PulseAudioOutput::device() const
{
    return m_device;
}

int PulseAudioOutput::bufferSize() const
{
    return std::max(1, m_targetBufferFrames);
}

OutputState PulseAudioOutput::currentState()
{
    if(!m_mainLoop || !m_stream) {
        return {};
    }

    OutputState state;

    pa_threaded_mainloop_lock(m_mainLoop.get());

    const size_t writableBytes = pa_stream_writable_size(m_stream);
    if(std::cmp_not_equal(writableBytes, -1)) {
        state.freeFrames = std::min(bufferSize(), m_format.framesForBytes(writableBytes));
    }

    pa_usec_t latency{0};
    int negative{0};
    if(pa_stream_get_latency(m_stream, &latency, &negative) >= 0 && !negative) {
        state.delay = static_cast<double>(latency) / 1'000'000.0;
    }

    pa_threaded_mainloop_unlock(m_mainLoop.get());

    state.queuedFrames = std::max(0, bufferSize() - state.freeFrames);
    if(state.delay <= 0.0 && m_format.sampleRate() > 0) {
        state.delay = static_cast<double>(state.queuedFrames) / static_cast<double>(m_format.sampleRate());
    }

    return state;
}

OutputDevices PulseAudioOutput::getAllDevices(bool /*isCurrentOutput*/)
{
    if(!m_initialised && !initContext()) {
        return {};
    }

    SinkContext devicesContext;
    devicesContext.mainLoop = m_mainLoop.get();

    pa_threaded_mainloop_lock(m_mainLoop.get());

    waitForOperation(pa_context_get_sink_info_list(m_context.get(), sinkInfoRequestCallback, &devicesContext),
                     m_mainLoop.get(), u"Failed to enumerate PulseAudio sinks"_s);

    pa_threaded_mainloop_unlock(m_mainLoop.get());

    if(!m_initialised) {
        pa_threaded_mainloop_stop(m_mainLoop.get());
        m_context.reset(nullptr);
        m_mainLoop.reset(nullptr);
    }

    return devicesContext.devices;
}

int PulseAudioOutput::write(const std::span<const std::byte> data, const int frameCount)
{
    if(!m_mainLoop || !m_stream) {
        return 0;
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(frameCount <= 0 || bytesPerFrame <= 0) {
        return 0;
    }

    const size_t availableFrames = data.size() / static_cast<size_t>(bytesPerFrame);
    const int framesToWrite      = std::min(frameCount, static_cast<int>(availableFrames));
    if(framesToWrite <= 0) {
        return 0;
    }

    pa_threaded_mainloop_lock(m_mainLoop.get());

    const size_t writableBytes = pa_stream_writable_size(m_stream);
    if(std::cmp_equal(writableBytes, -1)) {
        setPulseError(u"Failed to query PulseAudio writable size"_s);
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return 0;
    }

    const int writableFrames = m_format.framesForBytes(writableBytes);
    const int acceptedFrames = std::min(framesToWrite, writableFrames);
    if(acceptedFrames <= 0) {
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return 0;
    }

    const size_t byteCount = static_cast<size_t>(acceptedFrames) * static_cast<size_t>(bytesPerFrame);
    const int error        = pa_stream_write(m_stream, data.data(), byteCount, nullptr, 0, PA_SEEK_RELATIVE);

    pa_threaded_mainloop_unlock(m_mainLoop.get());

    if(error != 0) {
        setPulseError(u"Failed to write to PulseAudio stream"_s);
        return 0;
    }

    return acceptedFrames;
}

void PulseAudioOutput::setPaused(bool pause)
{
    if(!m_mainLoop || !m_stream) {
        return;
    }

    pa_threaded_mainloop_lock(m_mainLoop.get());

    waitForOperation(pa_stream_cork(m_stream, pause, successCallback, m_mainLoop.get()), m_mainLoop.get(),
                     pause ? u"Failed to pause PulseAudio stream"_s : u"Failed to resume PulseAudio stream"_s);

    pa_threaded_mainloop_unlock(m_mainLoop.get());
}

void PulseAudioOutput::setVolume(double volume)
{
    m_volume = volume;
}

bool PulseAudioOutput::supportsVolumeControl() const
{
    return false;
}

void PulseAudioOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

QString PulseAudioOutput::error() const
{
    return m_error;
}

AudioFormat PulseAudioOutput::format() const
{
    return m_format;
}

void PulseAudioOutput::resetPulse()
{
    if(m_mainLoop) {
        pa_threaded_mainloop_lock(m_mainLoop.get());
    }
    if(m_stream) {
        pa_stream_disconnect(m_stream);
        pa_stream_unref(m_stream);
        m_stream = nullptr;
    }
    if(m_mainLoop) {
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        pa_threaded_mainloop_stop(m_mainLoop.get());
    }

    m_context.reset(nullptr);
    m_mainLoop.reset(nullptr);

    m_targetBufferFrames = 0;
    m_initialised        = false;
}

void PulseAudioOutput::setError(const QString& message)
{
    m_error = message;
    qCWarning(PULSE) << message;
}

void PulseAudioOutput::setPulseError(const QString& message)
{
    if(m_context) {
        const int err = pa_context_errno(m_context.get());
        if(err != 0) {
            setError(u"%1: %2"_s.arg(message, QString::fromUtf8(pa_strerror(err))));
            return;
        }
    }

    setError(message);
}

bool PulseAudioOutput::initContext()
{
    m_error.clear();

    pa_threaded_mainloop* rawLoop{pa_threaded_mainloop_new()};
    if(rawLoop == nullptr) {
        setError(u"Failed to allocate PulseAudio main loop"_s);
        return false;
    }
    m_mainLoop = {rawLoop, PaMainLoopDeleter()};

    pa_context* rawContext{pa_context_new(pa_threaded_mainloop_get_api(m_mainLoop.get()), "fooyin")};
    if(rawContext == nullptr) {
        setError(u"Failed to allocate PulseAudio context"_s);
        return false;
    }
    m_context = {rawContext, PaContextDeleter()};

    pa_context_set_state_callback(m_context.get(), contextStateCallback, m_mainLoop.get());

    if(pa_context_connect(m_context.get(), nullptr, static_cast<pa_context_flags_t>(0), nullptr) < 0) {
        setPulseError(u"Failed to connect PulseAudio context"_s);
        return false;
    }

    pa_threaded_mainloop_lock(m_mainLoop.get());

    if(pa_threaded_mainloop_start(m_mainLoop.get()) < 0) {
        setError(u"Failed to start PulseAudio main loop"_s);
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return false;
    }

    while(pa_context_get_state(m_context.get()) != PA_CONTEXT_READY
          && PA_CONTEXT_IS_GOOD(pa_context_get_state(m_context.get()))) {
        pa_threaded_mainloop_wait(m_mainLoop.get());
    }

    const auto contextState = pa_context_get_state(m_context.get());
    if(contextState != PA_CONTEXT_READY) {
        setPulseError(u"PulseAudio context failed to enter a ready state"_s);
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return false;
    }

    pa_threaded_mainloop_unlock(m_mainLoop.get());

    return true;
}

bool PulseAudioOutput::initPulse()
{
    if(!initContext()) {
        return false;
    }

    pa_threaded_mainloop_lock(m_mainLoop.get());

    const pa_sample_format pulseFormat = findPulseFormat(m_format.sampleFormat());
    if(pulseFormat == PA_SAMPLE_INVALID) {
        setError(u"Unsupported PulseAudio sample format: %1"_s.arg(m_format.prettyFormat()));
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return false;
    }

    SinkInfoContext infoContext;
    infoContext.mainloop = m_mainLoop.get();

    const bool isDefault        = (m_device == u"default"_s);
    const QByteArray deviceName = m_device.toLocal8Bit();
    waitForOperation(pa_context_get_sink_info_by_name(m_context.get(), isDefault ? nullptr : deviceName.constData(),
                                                      sinkInfoCallback, &infoContext),
                     m_mainLoop.get(), u"Failed to query PulseAudio sink"_s);
    if(!infoContext.deviceFound) {
        setError(u"PulseAudio sink not found: %1"_s.arg(m_device));
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return false;
    }

    pa_sample_spec spec;
    spec.channels = m_format.channelCount();
    spec.format   = pulseFormat;
    spec.rate     = m_format.sampleRate();
    if(!pa_sample_spec_valid(&spec)) {
        setError(u"Invalid PulseAudio sample spec for %1"_s.arg(m_format.prettyFormat()));
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return false;
    }

    pa_channel_map requestedMap = infoContext.map;
    if(static_cast<int>(requestedMap.channels) != m_format.channelCount()) {
        pa_channel_map_init_auto(&requestedMap, spec.channels, PA_CHANNEL_MAP_DEFAULT);
        if(!pa_channel_map_valid(&requestedMap)) {
            setError(u"Failed to create PulseAudio channel map for %1"_s.arg(m_format.prettyFormat()));
            pa_threaded_mainloop_unlock(m_mainLoop.get());
            return false;
        }
    }

    m_stream = pa_stream_new(m_context.get(), "fooyin stream", &spec, &requestedMap);
    if(!m_stream) {
        setPulseError(u"Failed to create PulseAudio stream"_s);
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return false;
    }

    pa_stream_set_state_callback(m_stream, streamStateCallback, m_mainLoop.get());
    pa_stream_set_write_callback(m_stream, streamRequestCallback, m_mainLoop.get());

    m_targetBufferFrames      = std::max(1, m_format.framesForDuration(PulseTargetBufferMs));
    const int periodFrames    = std::max(1, m_format.framesForDuration(PulsePeriodMs));
    const auto targetBytes    = static_cast<uint32_t>(m_format.bytesForFrames(m_targetBufferFrames));
    const auto periodBytes    = static_cast<uint32_t>(m_format.bytesForFrames(periodFrames));
    const pa_buffer_attr attr = {
        .maxlength = static_cast<uint32_t>(-1),
        .tlength   = targetBytes,
        .prebuf    = 0,
        .minreq    = periodBytes,
        .fragsize  = static_cast<uint32_t>(-1),
    };
    const auto flags = static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY | PA_STREAM_START_CORKED);

    if(pa_stream_connect_playback(m_stream, isDefault ? nullptr : deviceName.constData(), &attr, flags, nullptr,
                                  nullptr)
       < 0) {
        setPulseError(u"Failed to connect PulseAudio playback stream"_s);
        pa_threaded_mainloop_unlock(m_mainLoop.get());
        return false;
    }

    while(true) {
        const pa_stream_state_t state = pa_stream_get_state(m_stream);
        if(state == PA_STREAM_READY) {
            break;
        }
        if(!PA_STREAM_IS_GOOD(state)) {
            setPulseError(u"PulseAudio stream failed to enter a ready state"_s);
            pa_threaded_mainloop_unlock(m_mainLoop.get());
            return false;
        }
        pa_threaded_mainloop_wait(m_mainLoop.get());
    }

    const auto* actualSpec = pa_stream_get_sample_spec(m_stream);
    if(actualSpec) {
        m_format.setSampleRate(static_cast<int>(actualSpec->rate));
        m_format.setChannelCount(static_cast<int>(actualSpec->channels));
    }

    const auto* actualMap = pa_stream_get_channel_map(m_stream);
    if(actualMap) {
        const auto layout = channelLayoutFromPulseMap(*actualMap);
        if(static_cast<int>(layout.size()) == m_format.channelCount()) {
            m_format.setChannelLayout(layout);
        }
    }

    qCDebug(PULSE) << "PulseAudio backend initialised with queue target:" << m_targetBufferFrames << "frames ("
                   << PulseTargetBufferMs << "ms)";

    pa_threaded_mainloop_unlock(m_mainLoop.get());
    return true;
}

void PulseAudioOutput::contextStateCallback(pa_context* c, void* userdata)
{
    auto* m = static_cast<pa_threaded_mainloop*>(userdata);
    switch(pa_context_get_state(c)) {
        case(PA_CONTEXT_READY):
        case(PA_CONTEXT_TERMINATED):
        case(PA_CONTEXT_UNCONNECTED):
        case(PA_CONTEXT_CONNECTING):
        case(PA_CONTEXT_AUTHORIZING):
        case(PA_CONTEXT_SETTING_NAME):
        case(PA_CONTEXT_FAILED):
            pa_threaded_mainloop_signal(m, 0);
            break;
    }
}

bool PulseAudioOutput::waitForOperation(pa_operation* op, pa_threaded_mainloop* mainloop, const QString& message)
{
    if(op == nullptr) {
        if(!message.isEmpty()) {
            setPulseError(message);
        }
        return false;
    }

    bool success{true};

    while(pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(mainloop);
    }

    if(pa_operation_get_state(op) != PA_OPERATION_DONE) {
        setPulseError(message.isEmpty() ? u"PulseAudio operation failed"_s : message);
        success = false;
    }

    pa_operation_unref(op);
    return success;
}
} // namespace Fooyin::PulseAudio
