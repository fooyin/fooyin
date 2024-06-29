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

#include <alsa/asoundlib.h>

#include <QDebug>

namespace {
void printError(const QString& message)
{
    qWarning() << QStringLiteral("[ALSA] %1").arg(message);
}

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
        case(Fooyin::SampleFormat::Unknown):
        default:
            return SND_PCM_FORMAT_UNKNOWN;
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
            devices.emplace_back(
                QString::fromLatin1(name.str),
                QStringLiteral("%1 - %2").arg(QString::fromLatin1(name.str), QString::fromLatin1(desc.str)));
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
struct AlsaOutput::Private
{
    AlsaOutput* m_self;

    AudioFormat m_format;

    bool m_initialised{false};

    PcmHandleUPtr m_pcmHandle{nullptr};
    snd_pcm_uframes_t m_bufferSize{8192};
    snd_pcm_uframes_t m_periodSize{1024};
    bool m_pausable{true};
    double m_volume{1.0};
    QString m_device{QStringLiteral("default")};
    bool m_started{false};
    QString m_error;

    explicit Private(AlsaOutput* self)
        : m_self{self}
    { }

    void reset()
    {
        if(m_pcmHandle) {
            m_pcmHandle.reset();
        }
        m_started = false;
        m_error.clear();
    }

    bool checkError(int error, const QString& message)
    {
        if(error < 0) {
            m_error = message;
            qWarning() << QStringLiteral("%1 - %2").arg(QString::fromLatin1(snd_strerror(error)), message);
            QMetaObject::invokeMethod(m_self, [this]() { emit m_self->stateChanged(State::Error); });
            return true;
        }
        return false;
    }

    bool formatSupported(snd_pcm_format_t requestedFormat, snd_pcm_hw_params_t* hwParams)
    {
        if(requestedFormat < 0) {
            return false;
        }

        snd_pcm_format_mask_t* mask;
        snd_pcm_format_mask_alloca(&mask);
        snd_pcm_hw_params_get_format_mask(hwParams, mask);
        const bool isSupported = snd_pcm_format_mask_test(mask, requestedFormat);
        QStringList supportedFormats;

        for(int format = 0; format <= SND_PCM_FORMAT_LAST; ++format) {
            if(snd_pcm_format_mask_test(mask, snd_pcm_format_t(format))) {
                supportedFormats.emplace_back(QString::fromLatin1(snd_pcm_format_name(snd_pcm_format_t(format))));
            }
        }

        if(!isSupported) {
            checkError(-1, QStringLiteral("[ALSA] Format not supported: %1")
                               .arg(QString::fromLatin1(snd_pcm_format_name(requestedFormat))));
            qInfo() << "[ALSA] Supported formats: " << supportedFormats.join(QStringLiteral(", "));
        }

        return isSupported;
    }

    void getHardwareDevices(Fooyin::OutputDevices& devices)
    {
        snd_pcm_stream_name(SND_PCM_STREAM_PLAYBACK);

        int card{-1};
        snd_ctl_card_info_t* cardinfo{nullptr};
        snd_ctl_card_info_alloca(&cardinfo);

        while(true) {
            int err = snd_card_next(&card);
            if(checkError(err, QStringLiteral("Unable to get soundcard"))) {
                break;
            }

            if(card < 0) {
                break;
            }

            char str[32];
            snprintf(str, sizeof(str) - 1, "hw:%d", card);

            snd_ctl_t* rawHandle;
            err = snd_ctl_open(&rawHandle, str, 0);
            if(checkError(err, QStringLiteral("Unable to open soundcard (%1)").arg(card))) {
                continue;
            }
            const CtlHandleUPtr handle = {rawHandle, CtlHandleDeleter()};

            err = snd_ctl_card_info(handle.get(), cardinfo);
            if(checkError(err, QStringLiteral("Control failure for soundcard (%1)").arg(card))) {
                continue;
            }

            int dev{-1};
            snd_pcm_info_t* pcminfo{nullptr};
            snd_pcm_info_alloca(&pcminfo);
            while(true) {
                err = snd_ctl_pcm_next_device(handle.get(), &dev);
                if(checkError(err, QStringLiteral("Failed to get device for soundcard (%1)").arg(card))) {
                    continue;
                }
                if(dev < 0) {
                    break;
                }

                snd_pcm_info_set_device(pcminfo, dev);
                snd_pcm_info_set_subdevice(pcminfo, 0);
                snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_PLAYBACK);

                err = snd_ctl_pcm_info(handle.get(), pcminfo);
                if(checkError(err, QStringLiteral("Failed to get control info for soundcard (%1)").arg(card))) {
                    continue;
                }

                Fooyin::OutputDevice device;
                device.name = QStringLiteral("hw:%1,%2").arg(card).arg(dev);
                device.desc = QStringLiteral("%1 - %2 %3")
                                  .arg(device.name, QString::fromLatin1(snd_ctl_card_info_get_name(cardinfo)),
                                       QString::fromLatin1(snd_pcm_info_get_name(pcminfo)));
                devices.emplace_back(device);
            }
        }
    }

    bool initAlsa()
    {
        int err{-1};
        {
            snd_pcm_t* rawHandle;
            err = snd_pcm_open(&rawHandle, m_device.toLocal8Bit().constData(), SND_PCM_STREAM_PLAYBACK,
                               SND_PCM_NONBLOCK);
            if(checkError(err, QStringLiteral("Failed to open device"))) {
                return false;
            }
            m_pcmHandle = {rawHandle, PcmHandleDeleter()};
        }
        snd_pcm_t* handle = m_pcmHandle.get();

        snd_pcm_hw_params_t* hwParams;
        snd_pcm_hw_params_alloca(&hwParams);

        err = snd_pcm_hw_params_any(handle, hwParams);
        if(checkError(err, QStringLiteral("Failed to initialise hardware parameters"))) {
            return false;
        }

        m_pausable = snd_pcm_hw_params_can_pause(hwParams);

        err = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
        if(checkError(err, QStringLiteral("Failed to set access mode"))) {
            return false;
        }

        const snd_pcm_format_t alsaFormat = findAlsaFormat(m_format.sampleFormat());
        if(checkError(alsaFormat, QStringLiteral("Format not supported"))) {
            return false;
        }

        // TODO: Handle resampling
        if(!formatSupported(alsaFormat, hwParams)) {
            return false;
        }

        err = snd_pcm_hw_params_set_format(handle, hwParams, alsaFormat);
        if(checkError(err, QStringLiteral("Failed to set audio format"))) {
            return false;
        }

        const auto sampleRate = static_cast<uint32_t>(m_format.sampleRate());

        err = snd_pcm_hw_params_set_rate(handle, hwParams, sampleRate, 0);
        if(checkError(err, QStringLiteral("Failed to set sample rate"))) {
            return false;
        }

        uint32_t channelCount = m_format.channelCount();

        err = snd_pcm_hw_params_set_channels_near(handle, hwParams, &channelCount);
        if(checkError(err, QStringLiteral("Failed to set channel count"))) {
            return false;
        }

        snd_pcm_uframes_t maxBufferSize;
        err = snd_pcm_hw_params_get_buffer_size_max(hwParams, &maxBufferSize);
        if(checkError(err, QStringLiteral("Unable to get max buffer size"))) {
            return false;
        }

        m_bufferSize = std::min(m_bufferSize, maxBufferSize);
        err          = snd_pcm_hw_params_set_buffer_size_near(handle, hwParams, &m_bufferSize);
        if(checkError(err, QStringLiteral("Unable to set buffer size"))) {
            return false;
        }

        err = snd_pcm_hw_params_set_period_size_near(handle, hwParams, &m_periodSize, nullptr);
        if(checkError(err, QStringLiteral("Failed to set period size"))) {
            return false;
        }

        err = snd_pcm_hw_params(handle, hwParams);
        if(checkError(err, QStringLiteral("Failed to apply hardware parameters"))) {
            return false;
        }

        snd_pcm_sw_params_t* swParams;
        snd_pcm_sw_params_alloca(&swParams);

        err = snd_pcm_sw_params_current(handle, swParams);
        if(checkError(err, QStringLiteral("Unable to get sw-parameters"))) {
            return false;
        }

        snd_pcm_uframes_t boundary;
        err = snd_pcm_sw_params_get_boundary(swParams, &boundary);
        if(checkError(err, QStringLiteral("Unable to get boundary"))) {
            return false;
        }

        // Play silence when underrun
        err = snd_pcm_sw_params_set_silence_size(handle, swParams, boundary);
        if(checkError(err, QStringLiteral("Unable to set silence size"))) {
            return false;
        }

        err = snd_pcm_sw_params_set_silence_threshold(handle, swParams, 0);
        if(checkError(err, QStringLiteral("Unable to set silence threshold"))) {
            return false;
        }

        err = snd_pcm_sw_params_set_start_threshold(handle, swParams, INT_MAX);
        if(checkError(err, QStringLiteral("Unable to set start threshold"))) {
            return false;
        }

        err = snd_pcm_sw_params_set_stop_threshold(handle, swParams, INT_MAX);
        if(checkError(err, QStringLiteral("Unable to set stop threshold"))) {
            return false;
        }

        err = snd_pcm_sw_params(handle, swParams);
        if(checkError(err, QStringLiteral("Failed to apply software parameters"))) {
            return false;
        }

        return !checkError(snd_pcm_prepare(m_pcmHandle.get()), QStringLiteral("Prepare error"));
    }

    bool attemptRecovery(snd_pcm_status_t* status)
    {
        if(!status) {
            return false;
        }

        bool autoRecoverAttempted{false};
        snd_pcm_state_t pcmst{SND_PCM_STATE_DISCONNECTED};

        // Give ALSA a number of chances to recover
        for(int n = 0; n < 5; ++n) {
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
                if(!m_started) {
                    return true;
                }
                snd_pcm_start(m_pcmHandle.get());
                continue;
            }

            switch(pcmst) {
                // Underrun
                case(SND_PCM_STATE_DRAINING):
                case(SND_PCM_STATE_XRUN):
                    checkError(snd_pcm_prepare(m_pcmHandle.get()), QStringLiteral("ALSA prepare error"));
                    continue;
                // Hardware suspend
                case(SND_PCM_STATE_SUSPENDED):
                    printError(QStringLiteral("Suspended. Attempting to resume.."));
                    err = snd_pcm_resume(m_pcmHandle.get());
                    if(err == -EAGAIN) {
                        printError(QStringLiteral("Resume failed. Retrying..."));
                        continue;
                    }
                    if(err == -ENOSYS) {
                        printError(QStringLiteral("Resume not supported. Trying prepare..."));
                        err = snd_pcm_prepare(m_pcmHandle.get());
                    }
                    checkError(err, QStringLiteral("Could not be resumed"));
                    continue;
                // Device lost
                case(SND_PCM_STATE_DISCONNECTED):
                case(SND_PCM_STATE_OPEN):
                default:
                    printError(QStringLiteral("Device lost. Stopping playback."));
                    QMetaObject::invokeMethod(m_self, [this]() { emit m_self->stateChanged(State::Disconnected); });
                    return false;
            }
        }
        return false;
    }

    bool recoverState(OutputState* state = nullptr)
    {
        if(!m_pcmHandle) {
            return false;
        }

        snd_pcm_status_t* status;
        snd_pcm_status_alloca(&status);

        const bool recovered = attemptRecovery(status);

        if(!recovered) {
            printError(QStringLiteral("Could not recover"));
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
};

AlsaOutput::AlsaOutput()
    : p{std::make_unique<Private>(this)}
{ }

AlsaOutput::~AlsaOutput()
{
    p->reset();
}

bool AlsaOutput::init(const AudioFormat& format)
{
    p->m_format = format;

    if(!p->initAlsa()) {
        uninit();
        return false;
    }

    p->m_initialised = true;
    return true;
}

void AlsaOutput::uninit()
{
    p->reset();
    p->m_initialised = false;
}

void AlsaOutput::reset()
{
    p->checkError(snd_pcm_drop(p->m_pcmHandle.get()), QStringLiteral("ALSA drop error"));
    p->checkError(snd_pcm_prepare(p->m_pcmHandle.get()), QStringLiteral("ALSA prepare error"));

    p->m_started = false;
    p->recoverState();
}

void AlsaOutput::start()
{
    p->m_started = true;
    snd_pcm_start(p->m_pcmHandle.get());
}

void AlsaOutput::drain()
{
    snd_pcm_drain(p->m_pcmHandle.get());
}

bool AlsaOutput::initialised() const
{
    return p->m_initialised;
}

QString AlsaOutput::device() const
{
    return p->m_device;
}

int AlsaOutput::bufferSize() const
{
    return static_cast<int>(p->m_bufferSize);
}

OutputState AlsaOutput::currentState()
{
    OutputState state;

    p->recoverState(&state);

    return state;
}

OutputDevices AlsaOutput::getAllDevices() const
{
    OutputDevices devices;

    getPcmDevices(devices);
    p->getHardwareDevices(devices);

    return devices;
}

int AlsaOutput::write(const AudioBuffer& buffer)
{
    if(!p->m_pcmHandle || !p->recoverState()) {
        return 0;
    }

    const int frameCount = buffer.frameCount();

    AudioBuffer adjustedBuff{buffer};
    adjustedBuff.scale(p->m_volume);

    snd_pcm_sframes_t err{0};
    err = snd_pcm_writei(p->m_pcmHandle.get(), adjustedBuff.constData().data(), frameCount);
    if(p->checkError(static_cast<int>(err), QStringLiteral("Write error"))) {
        return 0;
    }
    if(err != frameCount) {
        printError(
            QStringLiteral("Unexpected partial write (%1 of %2 frames)").arg(static_cast<int>(err)).arg(frameCount));
    }
    return static_cast<int>(err);
}

void AlsaOutput::setPaused(bool pause)
{
    if(!p->m_pausable) {
        return;
    }

    if(!p->recoverState()) {
        return;
    }

    const auto state = snd_pcm_state(p->m_pcmHandle.get());
    if(state == SND_PCM_STATE_RUNNING && pause) {
        p->checkError(snd_pcm_pause(p->m_pcmHandle.get(), 1), QStringLiteral("Couldn't pause device"));
    }
    else if(state == SND_PCM_STATE_PAUSED && !pause) {
        p->checkError(snd_pcm_pause(p->m_pcmHandle.get(), 0), QStringLiteral("Couldn't unpause device"));
    }
}

void AlsaOutput::setVolume(double volume)
{
    p->m_volume = volume;
}

void AlsaOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        p->m_device = device;
    }
}

QString AlsaOutput::error() const
{
    return p->m_error;
}
} // namespace Fooyin::Alsa
