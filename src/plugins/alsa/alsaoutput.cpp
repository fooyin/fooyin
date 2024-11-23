/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "alsaoutput.h"

#include "alsasettings.h"

#include <alsa/asoundlib.h>

#include <QDebug>
#include <QLoggingCategory>

#include <ranges>

Q_LOGGING_CATEGORY(ALSA, "fy.alsa")

using namespace Qt::StringLiterals;

namespace {
snd_pcm_format_t findAlsaFormat(Fooyin::SampleFormat format)
{
    switch(format) {
        case(Fooyin::SampleFormat::U8):
            return SND_PCM_FORMAT_U8;
        case(Fooyin::SampleFormat::S16):
            return SND_PCM_FORMAT_S16;
        case(Fooyin::SampleFormat::S24):
        case(Fooyin::SampleFormat::S32):
            return SND_PCM_FORMAT_S32;
        case(Fooyin::SampleFormat::F32):
            return SND_PCM_FORMAT_FLOAT;
        case(Fooyin::SampleFormat::F64):
            return SND_PCM_FORMAT_FLOAT64;
        case(Fooyin::SampleFormat::Unknown):
        default:
            return SND_PCM_FORMAT_UNKNOWN;
    }
}

Fooyin::SampleFormat findSampleFormat(snd_pcm_format_t format)
{
    switch(format) {
        case(SND_PCM_FORMAT_U8):
            return Fooyin::SampleFormat::U8;
        case(SND_PCM_FORMAT_S16):
            return Fooyin::SampleFormat::S16;
        case(SND_PCM_FORMAT_S32):
            return Fooyin::SampleFormat::S32;
        case(SND_PCM_FORMAT_FLOAT):
            return Fooyin::SampleFormat::F32;
        case(SND_PCM_FORMAT_FLOAT64):
            return Fooyin::SampleFormat::F64;
        case(SND_PCM_FORMAT_UNKNOWN):
        default:
            return Fooyin::SampleFormat::Unknown;
    }
}

struct DeviceHint
{
    void** hints{nullptr};

    ~DeviceHint()
    {
        if(hints) {
            snd_device_name_free_hint(hints);
        }
    }
};

struct DeviceString
{
    char* str{nullptr};

    explicit DeviceString(char* s)
        : str{s}
    { }

    ~DeviceString()
    {
        if(str) {
            std::free(str);
        }
    }

    explicit operator bool() const
    {
        return !!str;
    }
};

void getPcmDevices(Fooyin::OutputDevices& devices)
{
    DeviceHint deviceHint;

    if(snd_device_name_hint(-1, "pcm", &deviceHint.hints) < 0) {
        return;
    }

    for(int n{0}; deviceHint.hints[n]; ++n) {
        const DeviceString name{snd_device_name_get_hint(deviceHint.hints[n], "NAME")};
        const DeviceString desc{snd_device_name_get_hint(deviceHint.hints[n], "DESC")};
        const DeviceString io{snd_device_name_get_hint(deviceHint.hints[n], "IOID")};

        if(!name || !desc || (io && strcmp(io.str, "Output") != 0)) {
            continue;
        }

        if(strcmp(name.str, "default") == 0) {
            devices.insert(devices.begin(), {QString::fromLatin1(name.str), QString::fromLatin1(desc.str)});
        }
        else {
            devices.emplace_back(QString::fromLatin1(name.str),
                                 u"%1 - %2"_s.arg(QString::fromLatin1(name.str), QString::fromLatin1(desc.str)));
        }
    }
}

struct PcmHandleDeleter
{
    void operator()(snd_pcm_t* handle) const
    {
        if(handle) {
            snd_pcm_close(handle);
        }
    }
};
using PcmHandleUPtr = std::unique_ptr<snd_pcm_t, PcmHandleDeleter>;

struct CtlHandleDeleter
{
    void operator()(snd_ctl_t* handle) const
    {
        if(handle) {
            snd_ctl_close(handle);
        }
    }
};
using CtlHandleUPtr = std::unique_ptr<snd_ctl_t, CtlHandleDeleter>;
} // namespace

namespace Fooyin::Alsa {
AlsaOutput::AlsaOutput()
    : m_initialised{false}
    , m_pausable{true}
    , m_started{false}
    , m_device{u"default"_s}
    , m_volume{1.0}
    , m_bufferSize{8192}
    , m_periodSize{1024}
{ }

AlsaOutput::~AlsaOutput()
{
    resetAlsa();
}

bool AlsaOutput::init(const AudioFormat& format)
{
    m_format = format;

    if(!initAlsa()) {
        uninit();
        return false;
    }

    m_initialised = true;
    return true;
}

void AlsaOutput::uninit()
{
    resetAlsa();
    m_initialised = false;
}

void AlsaOutput::reset()
{
    checkError(snd_pcm_drop(m_pcmHandle.get()), "ALSA drop error");
    checkError(snd_pcm_prepare(m_pcmHandle.get()), "ALSA prepare error");

    m_started = false;
    recoverState();
}

void AlsaOutput::start()
{
    m_started = true;
    snd_pcm_start(m_pcmHandle.get());
}

void AlsaOutput::drain()
{
    snd_pcm_drain(m_pcmHandle.get());
}

bool AlsaOutput::initialised() const
{
    return m_initialised;
}

QString AlsaOutput::device() const
{
    return m_device;
}

int AlsaOutput::bufferSize() const
{
    return static_cast<int>(m_bufferSize);
}

OutputState AlsaOutput::currentState()
{
    OutputState state;

    recoverState(&state);

    return state;
}

OutputDevices AlsaOutput::getAllDevices(bool /*isCurrentOutput*/)
{
    OutputDevices devices;

    getPcmDevices(devices);
    getHardwareDevices(devices);

    return devices;
}

int AlsaOutput::write(const AudioBuffer& buffer)
{
    if(!m_pcmHandle || !recoverState()) {
        return 0;
    }

    const int frameCount = buffer.frameCount();

    AudioBuffer adjustedBuff{buffer};
    adjustedBuff.scale(m_volume);

    snd_pcm_sframes_t err{0};
    err = snd_pcm_writei(m_pcmHandle.get(), adjustedBuff.constData().data(), frameCount);
    if(checkError(static_cast<int>(err), "Write error")) {
        return 0;
    }
    if(err != frameCount) {
        qCWarning(ALSA) << "Unexpected partial write";
    }
    return static_cast<int>(err);
}

void AlsaOutput::setPaused(bool pause)
{
    if(!m_pausable) {
        return;
    }

    if(!pause && !recoverState()) {
        return;
    }

    const auto state = snd_pcm_state(m_pcmHandle.get());
    if(state == SND_PCM_STATE_RUNNING && pause) {
        checkError(snd_pcm_pause(m_pcmHandle.get(), 1), "Couldn't pause device");
    }
    else if(state == SND_PCM_STATE_PAUSED && !pause) {
        checkError(snd_pcm_pause(m_pcmHandle.get(), 0), "Couldn't unpause device");
    }
}

void AlsaOutput::setVolume(double volume)
{
    m_volume = volume;
}

void AlsaOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

QString AlsaOutput::error() const
{
    return m_error;
}

AudioFormat AlsaOutput::format() const
{
    return m_format;
}

void AlsaOutput::resetAlsa()
{
    if(m_pcmHandle) {
        m_pcmHandle.reset();
    }
    m_started = false;
    m_error.clear();
}

bool AlsaOutput::initAlsa()
{
    int err{-1};
    {
        snd_pcm_t* rawHandle;
        err = snd_pcm_open(&rawHandle, m_device.toLocal8Bit().constData(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if(checkError(err, "Failed to open device")) {
            return false;
        }
        m_pcmHandle = {rawHandle, PcmHandleDeleter()};
    }
    snd_pcm_t* handle = m_pcmHandle.get();

    snd_pcm_hw_params_t* hwParams;
    snd_pcm_hw_params_alloca(&hwParams);

    err = snd_pcm_hw_params_any(handle, hwParams);
    if(checkError(err, "Failed to initialise hardware parameters")) {
        return false;
    }

    m_pausable = snd_pcm_hw_params_can_pause(hwParams);

    err = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(checkError(err, "Failed to set access mode")) {
        return false;
    }

    if(!setAlsaFormat(hwParams)) {
        return false;
    }

    err = snd_pcm_hw_params_set_rate_resample(handle, hwParams, 1);
    if(checkError(err, "Failed to setup resampling")) {
        return false;
    }

    auto sampleRate = static_cast<uint32_t>(m_format.sampleRate());

    err = snd_pcm_hw_params_set_rate_near(handle, hwParams, &sampleRate, nullptr);
    if(checkError(err, "Failed to set sample rate")) {
        return false;
    }

    if(std::cmp_not_equal(sampleRate, m_format.sampleRate())) {
        qCDebug(ALSA) << "Sample rate not supported:" << m_format.sampleRate() << "Hz";
        qCDebug(ALSA) << "Using sample rate:" << sampleRate << "Hz";
        m_format.setSampleRate(static_cast<int>(sampleRate));
    }

    uint32_t channelCount = m_format.channelCount();

    err = snd_pcm_hw_params_set_channels_near(handle, hwParams, &channelCount);
    if(checkError(err, "Failed to set channel count")) {
        return false;
    }

    if(std::cmp_not_equal(channelCount, m_format.channelCount())) {
        qCDebug(ALSA) << "Using channels:" << channelCount;
        m_format.setChannelCount(static_cast<int>(channelCount));
    }

    uint32_t maxBufferTime;
    err = snd_pcm_hw_params_get_buffer_time_max(hwParams, &maxBufferTime, nullptr);
    if(checkError(err, "Unable to get max buffer time")) {
        return false;
    }
    uint32_t maxPeriodTime;
    err = snd_pcm_hw_params_get_period_time_max(hwParams, &maxPeriodTime, nullptr);
    if(checkError(err, "Unable to get max period time")) {
        return false;
    }

    uint32_t bufferTime
        = std::min<uint32_t>(m_settings.value(BufferLengthSetting, DefaultBufferLength).toUInt() * 1000, maxBufferTime);
    err = snd_pcm_hw_params_set_buffer_time_near(handle, hwParams, &bufferTime, nullptr);
    if(checkError(err, "Unable to set buffer time")) {
        return false;
    }
    uint32_t periodTime
        = std::min<uint32_t>(m_settings.value(PeriodLengthSetting, DefaultPeriodLength).toUInt() * 1000, maxPeriodTime);
    err = snd_pcm_hw_params_set_period_time_near(handle, hwParams, &periodTime, nullptr);
    if(checkError(err, "Unable to set period time")) {
        return false;
    }

    m_bufferSize = m_format.framesForDuration(bufferTime / 1000);
    m_periodSize = m_format.framesForDuration(periodTime / 1000);

    err = snd_pcm_hw_params(handle, hwParams);
    if(checkError(err, "Failed to apply hardware parameters")) {
        return false;
    }

    snd_pcm_sw_params_t* swParams;
    snd_pcm_sw_params_alloca(&swParams);

    err = snd_pcm_sw_params_current(handle, swParams);
    if(checkError(err, "Unable to get sw-parameters")) {
        return false;
    }

    snd_pcm_uframes_t boundary;
    err = snd_pcm_sw_params_get_boundary(swParams, &boundary);
    if(checkError(err, "Unable to get boundary")) {
        return false;
    }

    // Play silence when underrun
    err = snd_pcm_sw_params_set_silence_size(handle, swParams, boundary);
    if(checkError(err, "Unable to set silence size")) {
        return false;
    }

    err = snd_pcm_sw_params_set_silence_threshold(handle, swParams, 0);
    if(checkError(err, "Unable to set silence threshold")) {
        return false;
    }

    err = snd_pcm_sw_params_set_start_threshold(handle, swParams, INT_MAX);
    if(checkError(err, "Unable to set start threshold")) {
        return false;
    }

    err = snd_pcm_sw_params_set_stop_threshold(handle, swParams, INT_MAX);
    if(checkError(err, "Unable to set stop threshold")) {
        return false;
    }

    err = snd_pcm_sw_params(handle, swParams);
    if(checkError(err, "Failed to apply software parameters")) {
        return false;
    }

    return !checkError(snd_pcm_prepare(m_pcmHandle.get()), "Prepare error");
}

bool AlsaOutput::checkError(int error, const char* message)
{
    if(error < 0) {
        m_error = QString::fromUtf8(message);
        qCWarning(ALSA) << message << ":" << snd_strerror(error);
        emit stateChanged(State::Error);
        return true;
    }
    return false;
}

bool AlsaOutput::setAlsaFormat(snd_pcm_hw_params_t* hwParams)
{
    auto alsaFormat = findAlsaFormat(m_format.sampleFormat());
    const int err   = snd_pcm_hw_params_set_format(m_pcmHandle.get(), hwParams, alsaFormat);

    if(err < 0) {
        qCDebug(ALSA) << "Format not supported:" << m_format.prettyFormat();
        qCDebug(ALSA) << "Trying all supported formats";

        static constexpr std::array<std::pair<snd_pcm_format_t, int>, 5> formats{{{SND_PCM_FORMAT_U8, 8},
                                                                                  {SND_PCM_FORMAT_S16, 16},
                                                                                  {SND_PCM_FORMAT_S32, 32},
                                                                                  {SND_PCM_FORMAT_FLOAT, 32},
                                                                                  {SND_PCM_FORMAT_FLOAT64, 64}}};

        snd_pcm_format_t compatFormat{SND_PCM_FORMAT_UNKNOWN};

        // Try formats with a equal/higher bps first
        for(const auto& [format, bps] : formats) {
            if(format != alsaFormat && bps >= m_format.bitsPerSample()) {
                if(snd_pcm_hw_params_set_format(m_pcmHandle.get(), hwParams, format) >= 0) {
                    compatFormat = format;
                    break;
                }
            }
        }

        for(const auto& [format, bps] : formats | std::views::reverse) {
            if(format != alsaFormat && bps < m_format.bitsPerSample()) {
                if(snd_pcm_hw_params_set_format(m_pcmHandle.get(), hwParams, format) >= 0) {
                    compatFormat = format;
                    break;
                }
            }
        }

        if(compatFormat == SND_PCM_FORMAT_UNKNOWN) {
            checkError(-1, "Fallback format could not be found");
            return false;
        }

        m_format.setSampleFormat(findSampleFormat(compatFormat));
        qCDebug(ALSA) << "Found compatible format:" << m_format.prettyFormat();
    }

    return true;
}

void AlsaOutput::getHardwareDevices(OutputDevices& devices)
{
    snd_pcm_stream_name(SND_PCM_STREAM_PLAYBACK);

    int card{-1};
    snd_ctl_card_info_t* cardinfo{nullptr};
    snd_ctl_card_info_alloca(&cardinfo);

    while(true) {
        int err = snd_card_next(&card);
        if(checkError(err, "Unable to get soundcard")) {
            break;
        }

        if(card < 0) {
            break;
        }

        char str[32];
        snprintf(str, sizeof(str) - 1, "hw:%d", card);

        snd_ctl_t* rawHandle;
        err = snd_ctl_open(&rawHandle, str, 0);
        if(checkError(err, "Unable to open soundcard")) {
            continue;
        }
        const CtlHandleUPtr handle = {rawHandle, CtlHandleDeleter()};

        err = snd_ctl_card_info(handle.get(), cardinfo);
        if(checkError(err, "Control failure for soundcard")) {
            continue;
        }

        int dev{-1};
        snd_pcm_info_t* pcminfo{nullptr};
        snd_pcm_info_alloca(&pcminfo);
        while(true) {
            err = snd_ctl_pcm_next_device(handle.get(), &dev);
            if(checkError(err, "Failed to get device for soundcard")) {
                continue;
            }
            if(dev < 0) {
                break;
            }

            snd_pcm_info_set_device(pcminfo, dev);
            snd_pcm_info_set_subdevice(pcminfo, 0);
            snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);

            err = snd_ctl_pcm_info(handle.get(), pcminfo);
            if(checkError(err, "Failed to get control info for soundcard (%1)")) {
                continue;
            }

            OutputDevice device;
            device.name = u"hw:%1,%2"_s.arg(card).arg(dev);
            device.desc = u"%1 - %2 %3"_s.arg(device.name, QLatin1String(snd_ctl_card_info_get_name(cardinfo)),
                                              QLatin1String(snd_pcm_info_get_name(pcminfo)));
            devices.emplace_back(device);
        }
    }
}

bool AlsaOutput::attemptRecovery(snd_pcm_status_t* status)
{
    if(!status) {
        return false;
    }

    bool autoRecoverAttempted{false};
    snd_pcm_state_t pcmst{SND_PCM_STATE_DISCONNECTED};

    // Give ALSA a number of chances to recover
    for(int n{0}; n < 5; ++n) {
        if(!m_pcmHandle) {
            return false;
        }
        int err = snd_pcm_status(m_pcmHandle.get(), status);
        if(err == -EPIPE || err == -EINTR || err == -ESTRPIPE) {
            if(!autoRecoverAttempted) {
                autoRecoverAttempted = true;
                snd_pcm_recover(m_pcmHandle.get(), err, 1);
                continue;
            }
            pcmst = SND_PCM_STATE_XRUN;
        }
        else {
            pcmst = snd_pcm_status_get_state(status);
        }

        if(pcmst == SND_PCM_STATE_RUNNING || pcmst == SND_PCM_STATE_PAUSED) {
            return true;
        }

        if(pcmst == SND_PCM_STATE_SETUP) {
            snd_pcm_prepare(m_pcmHandle.get());
            continue;
        }

        if(pcmst == SND_PCM_STATE_PREPARED) {
            if(m_started) {
                snd_pcm_start(m_pcmHandle.get());
            }
            return true;
        }

        switch(pcmst) {
            // Underrun
            case(SND_PCM_STATE_DRAINING):
            case(SND_PCM_STATE_XRUN):
                checkError(snd_pcm_prepare(m_pcmHandle.get()), "ALSA prepare error");
                continue;
            // Hardware suspend
            case(SND_PCM_STATE_SUSPENDED):
                qCInfo(ALSA) << "Suspended - attempting to resume…";
                err = snd_pcm_resume(m_pcmHandle.get());
                if(err == -EAGAIN) {
                    qCWarning(ALSA) << "Resume failed - retrying…";
                    continue;
                }
                if(err == -ENOSYS) {
                    qCWarning(ALSA) << "Resume not supported - trying prepare…";
                    err = snd_pcm_prepare(m_pcmHandle.get());
                }
                checkError(err, "Could not be resumed");
                continue;
            // Device lost
            case(SND_PCM_STATE_DISCONNECTED):
            case(SND_PCM_STATE_OPEN):
            default:
                qCWarning(ALSA) << "Device lost - stopping playback";
                QMetaObject::invokeMethod(this, [this]() { emit this->stateChanged(State::Disconnected); });
                return false;
        }
    }
    return false;
}

bool AlsaOutput::recoverState(OutputState* state)
{
    if(!m_pcmHandle) {
        return false;
    }

    snd_pcm_status_t* status;
    snd_pcm_status_alloca(&status);

    const bool recovered = attemptRecovery(status);

    if(!recovered) {
        qCWarning(ALSA) << "Could not recover";
    }

    if(state) {
        const auto delay   = snd_pcm_status_get_delay(status);
        state->delay       = static_cast<double>(std::max(delay, 0L)) / static_cast<double>(m_format.sampleRate());
        state->freeSamples = static_cast<int>(snd_pcm_status_get_avail(status));
        state->freeSamples = std::clamp(state->freeSamples, 0, static_cast<int>(m_bufferSize));
        // Align to period size
        state->freeSamples   = static_cast<int>(state->freeSamples / m_periodSize * m_periodSize);
        state->queuedSamples = static_cast<int>(m_bufferSize) - state->freeSamples;
    }

    return recovered;
}
} // namespace Fooyin::Alsa
