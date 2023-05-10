/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Core::Engine {
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
    PcmHandleUPtr pcmHandle{nullptr};
    snd_pcm_uframes_t bufferSize{2048};
    bool pausable;
    int dir{0};

    void reset()
    {
        if(pcmHandle) {
            snd_pcm_drain(pcmHandle.get());
            pcmHandle.reset();
        }
        dir = 0;
    }
};

AlsaOutput::AlsaOutput(QObject* parent)
    : AudioOutput{parent}
    , p{std::make_unique<Private>()}
{ }

AlsaOutput::~AlsaOutput()
{
    p->reset();
}

void AlsaOutput::init(OutputContext* of)
{
    p->reset();

    {
        snd_pcm_t* rawHandle;
        int err = snd_pcm_open(&rawHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        if(err < 0) {
            qDebug() << "Failed to open ALSA device: " << snd_strerror(err);
            return;
        }
        p->pcmHandle = {rawHandle, PcmHandleDeleter()};
    }
    snd_pcm_t* handle = p->pcmHandle.get();

    snd_pcm_hw_params_t* hwParams;
    snd_pcm_hw_params_alloca(&hwParams);

    int err = snd_pcm_hw_params_any(handle, hwParams);
    if(err < 0) {
        qDebug() << "Failed to initialize ALSA hardware parameters: " << snd_strerror(err);
        return;
    }

    p->pausable = snd_pcm_hw_params_can_pause(hwParams);

    const auto streamFormat = of->format;

    err = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(err < 0) {
        qDebug() << "Failed to set ALSA access mode: " << snd_strerror(err);
        return;
    }

    snd_pcm_format_t sampleFormat{SND_PCM_FORMAT_S16};
    switch(streamFormat) {
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_U8P):
            sampleFormat = SND_PCM_FORMAT_U8;
            break;
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S16P):
            sampleFormat = SND_PCM_FORMAT_S16;
            break;
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_S32P):
            sampleFormat = SND_PCM_FORMAT_S32;
            break;
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_FLTP):
            sampleFormat = SND_PCM_FORMAT_FLOAT;
            break;
        case(AV_SAMPLE_FMT_DBL):
        case(AV_SAMPLE_FMT_DBLP):
            sampleFormat = SND_PCM_FORMAT_FLOAT64;
            break;
        default:
            qDebug() << QString{"Sampleformat %d is not supported"}.arg(streamFormat);
    }

    err = snd_pcm_hw_params_set_format(handle, hwParams, sampleFormat);
    if(err < 0) {
        qDebug() << "Failed to set ALSA audio format: " << snd_strerror(err);
        return;
    }

    uint32_t sampleRate = of->sampleRate;

    err = snd_pcm_hw_params_set_rate_near(handle, hwParams, &sampleRate, &p->dir);
    if(err < 0) {
        qDebug() << "Failed to set ALSA sample rate: " << snd_strerror(err);
        return;
    }

    err = snd_pcm_hw_params_set_channels(handle, hwParams, of->channelLayout.nb_channels);
    if(err < 0) {
        qDebug() << "Failed to set ALSA channel count: " << snd_strerror(err);
        return;
    }

    err = snd_pcm_hw_params_get_buffer_size(hwParams, &p->bufferSize);
    if(err < 0) {
        qDebug() << "Unable to get buffersize: " << snd_strerror(err);
        return;
    }

    err = snd_pcm_hw_params(handle, hwParams);
    if(err < 0) {
        qDebug() << "Failed to apply ALSA hardware parameters: " << snd_strerror(err);
        return;
    }

    snd_pcm_sw_params_t* swParams;
    snd_pcm_sw_params_alloca(&swParams);

    err = snd_pcm_sw_params_current(handle, swParams);
    if(err < 0) {
        qDebug() << "Unable to get sw-parameters: " << snd_strerror(err);
        return;
    }

    snd_pcm_uframes_t boundary;
    err = snd_pcm_sw_params_get_boundary(swParams, &boundary);
    if(err < 0) {
        qDebug() << "Unable to get boundary: " << snd_strerror(err);
        return;
    }

    // Play silence when underrun
    err = snd_pcm_sw_params_set_silence_size(handle, swParams, boundary);
    if(err < 0) {
        qDebug() << "Unable to set silence size: " << snd_strerror(err);
        return;
    }

    //    err = snd_pcm_sw_params_set_start_threshold(handle, swParams, INT_MAX);
    //    if(err < 0) {
    //        qDebug() << "Unable to set start threshold: " << snd_strerror(err);
    //        return;
    //    }

    err = snd_pcm_sw_params(handle, swParams);
    if(err < 0) {
        qDebug() << "Failed to apply ALSA software parameters: " << snd_strerror(err);
        return;
    }
}

void AlsaOutput::start()
{
    int err{0};
    err = snd_pcm_start(p->pcmHandle.get());
    if(err < 0) {
        qDebug() << "pcm start error";
    }
}

size_t AlsaOutput::bytesFree() const
{
    int frames = snd_pcm_avail_update(p->pcmHandle.get());
    if(frames == -EPIPE) {
        // Try and handle buffer underrun
        int err = snd_pcm_recover(p->pcmHandle.get(), frames, 0);
        if(err < 0) {
            qWarning() << "Couldn't recover from buffer underun";
            return 0;
        }
        else {
            frames = snd_pcm_avail_update(p->pcmHandle.get());
        }
    }
    else if(frames < 0) {
        return 0;
    }
    return snd_pcm_frames_to_bytes(p->pcmHandle.get(), frames);
}

int AlsaOutput::write(const char* data, int size)
{
    int space = bytesFree();

    if(!space) {
        return 0;
    }

    if(size < space) {
        space = size;
    }

    const int frames = snd_pcm_bytes_to_frames(p->pcmHandle.get(), space);
    const int result = snd_pcm_writei(p->pcmHandle.get(), data, frames);

    return snd_pcm_frames_to_bytes(p->pcmHandle.get(), result);
}

void AlsaOutput::setPaused(bool pause)
{
    if(!p->pausable) {
        return;
    }
    const auto state = snd_pcm_state(p->pcmHandle.get());
    if(state == SND_PCM_STATE_RUNNING && pause) {
        if(snd_pcm_pause(p->pcmHandle.get(), 1) < 0) {
            qWarning() << "Couldn't pause ALSA device";
        }
    }
    else if(state == SND_PCM_STATE_PAUSED && !pause) {
        if(snd_pcm_pause(p->pcmHandle.get(), 0) < 0) {
            qWarning() << "Couldn't resume ALSA device";
        }
    }
}

int AlsaOutput::bufferSize() const
{
    return p->bufferSize;
}

void AlsaOutput::setBufferSize(int size)
{
    p->bufferSize = size;
}

void AlsaOutput::clearBuffer()
{
    //    snd_pcm_drop(p->pcmHandle.get());
    //    snd_pcm_prepare(p->pcmHandle.get());
}
} // namespace Fy::Core::Engine
