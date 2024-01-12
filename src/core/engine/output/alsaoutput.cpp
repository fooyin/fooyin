/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

extern "C"
{
#include <libavcodec/avcodec.h>
}

#include <QDebug>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
void checkError(int error, const QString& message)
{
    if(error < 0) {
        qWarning() << "[ " + QString{snd_strerror(error)} + "]: " + message;
    }
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
            supportedFormats.emplace_back(snd_pcm_format_name(snd_pcm_format_t(format)));
        }
    }

    if(!isSupported) {
        qInfo() << "Format not supported: " << snd_pcm_format_name(requestedFormat);
        qInfo() << "Supported Formats: " << supportedFormats.join(", "_L1);
    }

    return isSupported;
}

snd_pcm_format_t findAlsaFormat(AVSampleFormat format)
{
    switch(format) {
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_U8P):
            return SND_PCM_FORMAT_U8;
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S16P):
            return SND_PCM_FORMAT_S16;
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_S32P):
            return SND_PCM_FORMAT_S32;
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_FLTP):
            return SND_PCM_FORMAT_FLOAT;
        case(AV_SAMPLE_FMT_DBL):
        case(AV_SAMPLE_FMT_DBLP):
            return SND_PCM_FORMAT_FLOAT64;
        case(AV_SAMPLE_FMT_NONE):
        case(AV_SAMPLE_FMT_S64):
        case(AV_SAMPLE_FMT_S64P):
        case(AV_SAMPLE_FMT_NB):
        default:
            return SND_PCM_FORMAT_UNKNOWN;
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

struct AlsaOutput::Private
{
    OutputContext outputContext;

    bool initialised{false};

    PcmHandleUPtr pcmHandle{nullptr};
    snd_pcm_uframes_t bufferSize{2048};
    uint32_t bufferTime{100000};
    uint32_t periods{4};
    snd_pcm_uframes_t periodSize{0};
    bool pausable{true};
    int dir{0};
    QString device{u"default"_s};
    bool deviceLost;
    bool started{false};

    void reset()
    {
        if(pcmHandle) {
            snd_pcm_drop(pcmHandle.get());
            pcmHandle.reset();
        }
        dir     = 0;
        started = false;
    }

    bool recoverState(OutputState* state = nullptr)
    {
        if(!pcmHandle) {
            return false;
        }

        snd_pcm_status_t* st;
        snd_pcm_status_alloca(&st);

        bool recovered        = false;
        snd_pcm_state_t pcmst = SND_PCM_STATE_DISCONNECTED;

        // Give ALSA a number of chances to recover
        for(int n = 0; n < 5; ++n) {
            int err = snd_pcm_status(pcmHandle.get(), st);
            if(err == -EPIPE) {
                pcmst = SND_PCM_STATE_XRUN;
            }
            else {
                pcmst = snd_pcm_status_get_state(st);
            }

            if(pcmst == SND_PCM_STATE_RUNNING || pcmst == SND_PCM_STATE_PAUSED) {
                recovered = true;
                break;
            }

            if(pcmst == SND_PCM_STATE_PREPARED) {
                if(!started) {
                    recovered = true;
                    break;
                }
                snd_pcm_start(pcmHandle.get());
                continue;
            }

            qDebug() << QString{u"(%1) Attempting to recover from state '%2'..."_s}.arg(n + 1).arg(
                snd_pcm_state_name(pcmst));

            switch(pcmst) {
                // Underrun
                case SND_PCM_STATE_DRAINING:
                case SND_PCM_STATE_XRUN:
                    err = snd_pcm_prepare(pcmHandle.get());
                    checkError(err, u"ALSA prepare error"_s);
                    continue;
                // Hardware suspend
                case SND_PCM_STATE_SUSPENDED:
                    qWarning() << "ALSA in suspend mode. Attempting to resume...";
                    err = snd_pcm_resume(pcmHandle.get());
                    if(err == -EAGAIN) {
                        qWarning() << "ALSA resume failed. Retrying...";
                        continue;
                    }
                    if(err == -ENOSYS) {
                        qWarning() << "ALSA resume not supported. Trying prepare...";
                        err = snd_pcm_prepare(pcmHandle.get());
                    }
                    checkError(err, "ALSA could not be resumed: " + QString{snd_strerror(err)});
                    continue;
                // Device lost
                case SND_PCM_STATE_DISCONNECTED:
                case SND_PCM_STATE_OPEN:
                case SND_PCM_STATE_SETUP:
                default:
                    if(!deviceLost) {
                        qWarning() << "ALSA device lost. Attempting to recover...";
                        // TODO: Request audio output reload
                        deviceLost = true;
                    }
            }
        }

        if(!recovered) {
            qWarning() << "ALSA could not recover";
        }

        if(state) {
            auto delay   = snd_pcm_status_get_delay(st);
            state->delay = static_cast<double>(std::max(delay, 0L)) / static_cast<double>(outputContext.sampleRate);
            state->freeSamples = static_cast<int>(snd_pcm_status_get_avail(st));
            state->freeSamples = std::clamp(state->freeSamples, 0, static_cast<int>(bufferSize));
            // Align to period size
            state->freeSamples   = static_cast<int>(state->freeSamples / periodSize * periodSize);
            state->queuedSamples = static_cast<int>(bufferSize) - state->freeSamples;
        }

        return recovered;
    }
};

AlsaOutput::AlsaOutput()
    : p{std::make_unique<Private>()}
{ }

AlsaOutput::~AlsaOutput()
{
    p->reset();
}

bool AlsaOutput::init(const OutputContext& oc)
{
    if(p->initialised) {
        return false;
    }

    p->outputContext = oc;

    int err{-1};
    {
        snd_pcm_t* rawHandle;
        err = snd_pcm_open(&rawHandle, p->device.toLocal8Bit(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
        if(err < 0) {
            qDebug() << "Failed to open ALSA device: " << snd_strerror(err);
            return false;
        }
        p->pcmHandle = {rawHandle, PcmHandleDeleter()};
    }
    snd_pcm_t* handle = p->pcmHandle.get();

    snd_pcm_hw_params_t* hwParams;
    snd_pcm_hw_params_alloca(&hwParams);

    const auto handleInitError = [this]() {
        p->reset();
        return !p->pcmHandle;
    };

    err = snd_pcm_hw_params_any(handle, hwParams);
    if(err < 0) {
        qDebug() << "Failed to initialize ALSA hardware parameters: " << snd_strerror(err);
        return handleInitError();
    }

    p->pausable = snd_pcm_hw_params_can_pause(hwParams);

    err = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(err < 0) {
        qDebug() << "Failed to set ALSA access mode: " << snd_strerror(err);
        return handleInitError();
    }

    const snd_pcm_format_t format = findAlsaFormat(oc.format);
    if(format < 0) {
        qWarning() << "Format not supported by ALSA";
        return handleInitError();
    }

    // TODO: Handle resampling
    if(!formatSupported(format, hwParams)) {
        return handleInitError();
    }

    err = snd_pcm_hw_params_set_format(handle, hwParams, format);
    if(err < 0) {
        qDebug() << "Failed to set ALSA audio format: " << snd_strerror(err);
        return handleInitError();
    }

    uint32_t sampleRate = oc.sampleRate;

    err = snd_pcm_hw_params_set_rate_near(handle, hwParams, &sampleRate, &p->dir);
    if(err < 0) {
        qDebug() << "Failed to set ALSA sample rate: " << snd_strerror(err);
        return handleInitError();
    }

    uint32_t channelCount = oc.channelLayout.nb_channels;

    err = snd_pcm_hw_params_set_channels_near(handle, hwParams, &channelCount);
    if(err < 0) {
        qDebug() << "Failed to set ALSA channel count: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_hw_params_set_buffer_time_near(handle, hwParams, &p->bufferTime, nullptr);
    if(err < 0) {
        qDebug() << "Failed to set ALSA buffer time: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_hw_params_set_periods_near(handle, hwParams, &p->periods, nullptr);
    if(err < 0) {
        qDebug() << "Failed to set ALSA periods: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_hw_params(handle, hwParams);
    if(err < 0) {
        qDebug() << "Failed to apply ALSA hardware parameters: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_hw_params_get_buffer_size(hwParams, &p->bufferSize);
    if(err < 0) {
        qDebug() << "Unable to get buffer size: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_hw_params_get_period_size(hwParams, &p->periodSize, nullptr);
    if(err < 0) {
        qDebug() << "Unable to get period size: " << snd_strerror(err);
        return handleInitError();
    }

    snd_pcm_sw_params_t* swParams;
    snd_pcm_sw_params_alloca(&swParams);

    err = snd_pcm_sw_params_current(handle, swParams);
    if(err < 0) {
        qDebug() << "Unable to get sw-parameters: " << snd_strerror(err);
        return handleInitError();
    }

    snd_pcm_uframes_t boundary;
    err = snd_pcm_sw_params_get_boundary(swParams, &boundary);
    if(err < 0) {
        qDebug() << "Unable to get boundary: " << snd_strerror(err);
        return handleInitError();
    }

    // Play silence when underrun
    err = snd_pcm_sw_params_set_silence_size(handle, swParams, boundary);
    if(err < 0) {
        qDebug() << "Unable to set silence size: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_sw_params_set_silence_threshold(handle, swParams, 0);
    if(err < 0) {
        qDebug() << "Unable to set silence threshold: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_sw_params_set_start_threshold(handle, swParams, INT_MAX);
    if(err < 0) {
        qDebug() << "Unable to set start threshold: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_sw_params_set_stop_threshold(handle, swParams, INT_MAX);
    if(err < 0) {
        qDebug() << "Unable to set stop threshold: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_sw_params(handle, swParams);
    if(err < 0) {
        qDebug() << "Failed to apply ALSA software parameters: " << snd_strerror(err);
        return handleInitError();
    }

    err = snd_pcm_prepare(p->pcmHandle.get());
    if(err < 0) {
        qDebug() << "Alsa prepare error: " << snd_strerror(err);
        return handleInitError();
    }

    p->initialised = true;
    return true;
}

void AlsaOutput::uninit()
{
    p->reset();
    p->initialised = false;
}

void AlsaOutput::reset()
{
    if(!p->pcmHandle || !p->initialised) {
        return;
    }

    int err = snd_pcm_drop(p->pcmHandle.get());
    checkError(err, u"ALSA drop error"_s);

    err = snd_pcm_prepare(p->pcmHandle.get());
    checkError(err, u"ALSA prepare error"_s);

    p->recoverState();
}

void AlsaOutput::start()
{
    if(!p->pcmHandle || !p->initialised) {
        return;
    }

    p->started = true;
    snd_pcm_start(p->pcmHandle.get());
}

bool AlsaOutput::initialised() const
{
    return p->initialised;
}

QString AlsaOutput::device() const
{
    return p->device;
}

bool AlsaOutput::canHandleVolume() const
{
    return false;
}

int AlsaOutput::bufferSize() const
{
    return static_cast<int>(p->bufferSize);
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

    void** hints;
    const int err = snd_device_name_hint(-1, "pcm", &hints);
    if(err < 0) {
        return {};
    }

    for(int n = 0; hints[n]; ++n) {
        char* devName = snd_device_name_get_hint(hints[n], "NAME");
        char* desc    = snd_device_name_get_hint(hints[n], "DESC");
        char* io      = snd_device_name_get_hint(hints[n], "IOID");

        if(devName && desc) {
            if(!io || strcmp(io, "Output") == 0) {
                if(strcmp(devName, "default") == 0) {
                    devices.insert(devices.begin(), {devName, desc});
                }
                else {
                    devices.emplace_back(devName, desc);
                }
            }
        }

        if(devName) {
            std::free(devName);
        }
        if(desc) {
            std::free(desc);
        }
        if(io) {
            std::free(io);
        }
    }
    snd_device_name_free_hint(hints);

    return devices;
}

int AlsaOutput::write(const uint8_t* data, int samples)
{
    if(!p->pcmHandle || !p->recoverState()) {
        return 0;
    }

    snd_pcm_sframes_t err{0};
    err = snd_pcm_writei(p->pcmHandle.get(), data, samples);
    if(err < 0) {
        qWarning() << "ALSA write error";
        return 0;
    }
    if(err != samples) {
        qWarning() << QString{u"Unexpected partial write (%1 of %2 frames)"_s}.arg(static_cast<int>(err)).arg(samples);
    }
    return static_cast<int>(err);
}

void AlsaOutput::setPaused(bool pause)
{
    if(!p->pausable || !initialised()) {
        return;
    }

    p->recoverState();

    int err;
    const auto state = snd_pcm_state(p->pcmHandle.get());
    if(state == SND_PCM_STATE_RUNNING && pause) {
        err = snd_pcm_pause(p->pcmHandle.get(), 1);
        checkError(err, u"Couldn't pause ALSA device"_s);
    }
    else if(state == SND_PCM_STATE_PAUSED && !pause) {
        err = snd_pcm_pause(p->pcmHandle.get(), 0);
        checkError(err, u"Couldn't unpause ALSA device"_s);
    }
}

void AlsaOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        p->device = device;
    }
}
} // namespace Fooyin
