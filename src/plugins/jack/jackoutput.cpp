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

#include "jackoutput.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QThread>

#include <cerrno>
#include <cstring>
#include <set>

Q_LOGGING_CATEGORY(JACK, "fy.jack")

using namespace Qt::StringLiterals;

constexpr auto JackTargetBufferMs = 200;
constexpr auto JackSampleSize     = sizeof(jack_default_audio_sample_t);

static_assert(std::atomic_bool::is_always_lock_free);
static_assert(std::atomic_int::is_always_lock_free);

namespace Fooyin::Jack {
namespace {
struct ClientHandle
{
    jack_client_t* client{nullptr};

    explicit ClientHandle(jack_client_t* client_)
        : client{client_}
    { }

    ~ClientHandle()
    {
        if(client) {
            jack_client_close(client);
        }
    }

    ClientHandle(const ClientHandle&)            = delete;
    ClientHandle& operator=(const ClientHandle&) = delete;

    explicit operator bool() const
    {
        return client != nullptr;
    }
};

struct PortList
{
    const char** ports{nullptr};

    explicit PortList(const char** ports_)
        : ports{ports_}
    { }

    ~PortList()
    {
        if(ports) {
            jack_free(static_cast<void*>(ports));
        }
    }

    PortList(const PortList&)            = delete;
    PortList& operator=(const PortList&) = delete;

    explicit operator bool() const
    {
        return ports != nullptr;
    }
};
} // namespace

JackOutput::JackOutput()
    : m_initialised{false}
    , m_active{false}
    , m_device{u"default"_s}
    , m_failureReported{false}
    , m_serverStopped{false}
    , m_serverRate{0}
    , m_paused{false}
    , m_resetting{false}
    , m_processing{false}
    , m_client{nullptr}
    , m_numPorts{0}
    , m_targetBufferFrames{0}
    , m_ports{}
    , m_ringbuffer{nullptr}
{ }

JackOutput::~JackOutput()
{
    closeClient();
}

bool JackOutput::init(const AudioFormat& format)
{
    closeClient();
    m_error.clear();
    m_failureReported = false;
    m_serverStopped.store(false, std::memory_order_release);
    m_paused.store(false, std::memory_order_release);

    if(!format.isValid()) {
        setError(u"Invalid JACK audio format"_s);
        return false;
    }

    m_client = jack_client_open("fooyin", JackNoStartServer, nullptr);
    if(!m_client) {
        setError(u"Unable to open JACK client"_s);
        return false;
    }

    m_format = format;
    m_format.setSampleFormat(SampleFormat::F32);
    m_format.setSampleRate(static_cast<int>(jack_get_sample_rate(m_client)));
    if(!m_format.isValid()) {
        setError(u"Invalid JACK sample rate"_s);
        closeClient();
        return false;
    }

    m_serverRate.store(m_format.sampleRate(), std::memory_order_release);
    m_targetBufferFrames = std::max(
        {1, m_format.framesForDuration(JackTargetBufferMs), static_cast<int>(jack_get_buffer_size(m_client))});

    if(!createPorts(m_format.channelCount()) || !createRingbuffer()) {
        closeClient();
        return false;
    }

    if(jack_set_process_callback(m_client, process, this) != 0
       || jack_set_sample_rate_callback(m_client, sampleRateCallback, this) != 0) {
        setError(u"Unable to register JACK callbacks"_s);
        closeClient();
        return false;
    }

    jack_on_shutdown(m_client, shutdownCallback, this);
    m_initialised = true;
    return true;
}

void JackOutput::uninit()
{
    closeClient();
}

void JackOutput::reset()
{
    m_paused.store(true, std::memory_order_release);
    m_resetting.store(true);

    while(m_processing.load()) {
        QThread::yieldCurrentThread();
    }
    if(m_ringbuffer) {
        jack_ringbuffer_reset(m_ringbuffer);
    }

    m_resetting.store(false);
}

void JackOutput::start()
{
    checkEvents();
    if(!m_initialised || m_failureReported) {
        return;
    }

    if(!m_active) {
        if(jack_activate(m_client) != 0) {
            setError(u"Unable to activate JACK client"_s);
            m_failureReported = true;
            Q_EMIT stateChanged(State::Error);
            return;
        }
        m_active = true;
        connectOutports();
    }
    m_paused.store(false, std::memory_order_release);
}

void JackOutput::drain()
{
    if(!m_active || m_paused.load(std::memory_order_acquire)) {
        return;
    }

    while(availableReadSpace() > 0) {
        checkEvents();
        if(m_failureReported || m_paused.load(std::memory_order_acquire)) {
            break;
        }
        QThread::usleep(1000);
    }
}

bool JackOutput::initialised() const
{
    return m_initialised;
}

QString JackOutput::device() const
{
    return m_device;
}

int JackOutput::bufferSize() const
{
    return m_targetBufferFrames;
}

OutputState JackOutput::currentState()
{
    checkEvents();
    if(!m_initialised || m_failureReported) {
        return {};
    }

    OutputState state;
    state.queuedFrames = static_cast<int>(availableReadSpace());
    state.freeFrames   = std::min(bufferSize() - state.queuedFrames, static_cast<int>(availableWriteSpace()));

    jack_nframes_t graphLatency{0};
    for(int i{0}; i < m_numPorts; ++i) {
        jack_latency_range_t range{};
        jack_port_get_latency_range(m_ports[static_cast<size_t>(i)], JackPlaybackLatency, &range);
        graphLatency = std::max(graphLatency, range.max);
    }

    state.delay = (static_cast<double>(state.queuedFrames) + static_cast<double>(graphLatency))
                / static_cast<double>(m_format.sampleRate());
    return state;
}

OutputDevices JackOutput::getAllDevices(bool /*isCurrentOutput*/)
{
    OutputDevices devices;
    devices.emplace_back(u"default"_s, u"Default"_s);

    const ClientHandle handle{jack_client_open("fooyin_devices", JackNoStartServer, nullptr)};
    if(!handle) {
        return devices;
    }

    const PortList list{jack_get_ports(handle.client, nullptr, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput)};
    if(list) {
        std::set<QString> clients;
        for(int i{0}; list.ports[i]; ++i) {
            const auto clientName = QString::fromUtf8(list.ports[i]).section(u':', 0, 0);
            if(clients.insert(clientName).second) {
                devices.emplace_back(clientName, clientName);
            }
        }
    }

    return devices;
}

int JackOutput::write(std::span<const std::byte> data, int frameCount)
{
    checkEvents();
    if(!m_initialised || m_failureReported || frameCount <= 0) {
        return 0;
    }

    const auto frameBytes     = static_cast<size_t>(m_format.bytesPerFrame());
    const int availableFrames = static_cast<int>(std::min(data.size() / frameBytes, static_cast<size_t>(frameCount)));
    const int queuedFrames    = static_cast<int>(availableReadSpace());
    const int acceptedFrames
        = std::min({availableFrames, bufferSize() - queuedFrames, static_cast<int>(availableWriteSpace())});

    const size_t written = jack_ringbuffer_write(m_ringbuffer, reinterpret_cast<const char*>(data.data()),
                                                 static_cast<size_t>(acceptedFrames) * frameBytes);
    return static_cast<int>(written / frameBytes);
}

void JackOutput::setPaused(bool pause)
{
    m_paused.store(pause, std::memory_order_release);
}

bool JackOutput::supportsVolumeControl() const
{
    return false;
}

void JackOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

QString JackOutput::error() const
{
    return m_error;
}

AudioFormat JackOutput::format() const
{
    return m_format;
}

int JackOutput::sampleRateCallback(jack_nframes_t rate, void* arg)
{
    auto* output = static_cast<JackOutput*>(arg);
    if(output) {
        output->m_serverRate.store(static_cast<int>(rate), std::memory_order_release);
    }
    return 0;
}

int JackOutput::process(jack_nframes_t nframes, void* arg)
{
    auto* output = static_cast<JackOutput*>(arg);
    if(!output) {
        return -1;
    }
    output->m_processing.store(true);

    const int channels    = output->channelCount();
    const auto frameBytes = static_cast<size_t>(channels) * JackSampleSize;
    const auto frames     = output->m_resetting.load() || output->m_paused.load(std::memory_order_acquire)
                              ? 0
                              : std::min(output->availableReadSpace(), nframes);
    jack_ringbuffer_data_t readable[2]{};
    if(frames > 0) {
        jack_ringbuffer_get_read_vector(output->m_ringbuffer, readable);
    }

    for(int ch{0}; ch < channels; ++ch) {
        auto* out = static_cast<jack_default_audio_sample_t*>(
            jack_port_get_buffer(output->m_ports[static_cast<size_t>(ch)], nframes));
        if(!out) {
            continue;
        }

        for(jack_nframes_t frame{0}; frame < frames; ++frame) {
            const size_t offset
                = (static_cast<size_t>(frame) * frameBytes) + (static_cast<size_t>(ch) * JackSampleSize);
            const char* sample
                = offset < readable[0].len ? readable[0].buf + offset : readable[1].buf + (offset - readable[0].len);
            std::memcpy(out + frame, sample, JackSampleSize);
        }
        std::fill(out + frames, out + nframes, 0.0F);
    }

    if(frames > 0) {
        jack_ringbuffer_read_advance(output->m_ringbuffer, static_cast<size_t>(frames) * frameBytes);
    }

    output->m_processing.store(false);
    return 0;
}

void JackOutput::shutdownCallback(void* arg)
{
    auto* output = static_cast<JackOutput*>(arg);
    if(output) {
        output->m_serverStopped.store(true, std::memory_order_release);
    }
}

bool JackOutput::createPorts(int portCount)
{
    for(int i{0}; i < portCount; ++i) {
        const QByteArray name = u"out_%1"_s.arg(i + 1).toUtf8();
        m_ports[static_cast<size_t>(i)]
            = jack_port_register(m_client, name.constData(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        if(!m_ports[static_cast<size_t>(i)]) {
            setError(u"Not enough JACK ports available"_s);
            return false;
        }
    }

    m_numPorts = portCount;
    return true;
}

bool JackOutput::createRingbuffer()
{
    // JACK reserves one byte and rounds capacity up to a power of two.
    const auto bytes = (static_cast<size_t>(m_targetBufferFrames) * static_cast<size_t>(m_format.bytesPerFrame())) + 1;
    m_ringbuffer     = jack_ringbuffer_create(bytes);
    if(!m_ringbuffer) {
        setError(u"Failed to allocate JACK ringbuffer"_s);
        return false;
    }
    jack_ringbuffer_mlock(m_ringbuffer);
    // Touch allocated pages before the realtime callback accesses them.
    std::memset(m_ringbuffer->buf, 0, m_ringbuffer->size);
    return true;
}

bool JackOutput::connectOutports()
{
    const bool useDefault     = m_device == u"default"_s;
    const unsigned long flags = JackPortIsInput | (useDefault ? JackPortIsPhysical : 0);

    const PortList list{jack_get_ports(m_client, nullptr, JACK_DEFAULT_AUDIO_TYPE, flags)};
    if(!list) {
        if(useDefault) {
            return true;
        }
        setError(u"JACK device unavailable: %1"_s.arg(m_device));
        Q_EMIT stateChanged(AudioOutput::State::Error);
        return false;
    }

    const QByteArray prefix = m_device.toUtf8() + ':';

    bool connected{true};
    int destination{0};
    for(int i{0}; list.ports[i] && destination < std::max(2, m_numPorts); ++i) {
        if(!useDefault && !QByteArray{list.ports[i]}.startsWith(prefix)) {
            continue;
        }

        const int channel = m_numPorts == 1 ? 0 : destination;
        const int result = jack_connect(m_client, jack_port_name(m_ports[static_cast<size_t>(channel)]), list.ports[i]);
        connected        = (result == 0 || result == EEXIST) && connected;
        ++destination;
    }

    if(!connected || (!useDefault && destination == 0)) {
        setError(u"Unable to connect JACK output to %1"_s.arg(m_device));
        Q_EMIT stateChanged(AudioOutput::State::Error);
        return false;
    }

    return true;
}

void JackOutput::closeClient()
{
    if(m_client) {
        if(m_active && !m_serverStopped.load(std::memory_order_acquire)) {
            jack_deactivate(m_client);
        }
        jack_client_close(m_client);
        m_client = nullptr;
    }

    if(m_ringbuffer) {
        jack_ringbuffer_free(m_ringbuffer);
        m_ringbuffer = nullptr;
    }

    m_ports.fill(nullptr);
    m_numPorts           = 0;
    m_targetBufferFrames = 0;
    m_active             = false;
    m_initialised        = false;
}

void JackOutput::checkEvents()
{
    if(!m_initialised || m_failureReported) {
        return;
    }

    const bool stopped = m_serverStopped.load(std::memory_order_acquire);
    if(stopped || m_serverRate.load(std::memory_order_acquire) != m_format.sampleRate()) {
        m_failureReported = true;
        setError(stopped ? u"JACK server disconnected"_s : u"JACK server sample rate changed"_s);

        QMetaObject::invokeMethod(this, [this]() { Q_EMIT stateChanged(State::Disconnected); }, Qt::QueuedConnection);
    }
}

jack_nframes_t JackOutput::availableReadSpace() const noexcept
{
    return m_ringbuffer ? static_cast<jack_nframes_t>(jack_ringbuffer_read_space(m_ringbuffer)
                                                      / static_cast<size_t>(m_format.bytesPerFrame()))
                        : 0;
}

jack_nframes_t JackOutput::availableWriteSpace() const noexcept
{
    return m_ringbuffer ? static_cast<jack_nframes_t>(jack_ringbuffer_write_space(m_ringbuffer)
                                                      / static_cast<size_t>(m_format.bytesPerFrame()))
                        : 0;
}

int JackOutput::channelCount() const noexcept
{
    return m_numPorts;
}

void JackOutput::setError(const QString& message)
{
    m_error = message;
    qCWarning(JACK) << message;
}
} // namespace Fooyin::Jack
