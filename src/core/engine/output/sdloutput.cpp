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

#include "sdloutput.h"

#include <SDL2/SDL.h>

#include <QDebug>

namespace Fy::Core::Engine {
SDL_AudioFormat findFormat(AVSampleFormat format)
{
    switch(format) {
        case(AV_SAMPLE_FMT_U8):
        case(AV_SAMPLE_FMT_U8P):
            return AUDIO_U8;
        case(AV_SAMPLE_FMT_S16):
        case(AV_SAMPLE_FMT_S16P):
            return AUDIO_S16SYS;
        case(AV_SAMPLE_FMT_S32):
        case(AV_SAMPLE_FMT_S32P):
            return AUDIO_S32SYS;
        case(AV_SAMPLE_FMT_FLT):
        case(AV_SAMPLE_FMT_FLTP):
            return AUDIO_F32SYS;
        case(AV_SAMPLE_FMT_DBL):
        case(AV_SAMPLE_FMT_DBLP):
        case(AV_SAMPLE_FMT_S64):
        case(AV_SAMPLE_FMT_S64P):
        case(AV_SAMPLE_FMT_NB):
        case(AV_SAMPLE_FMT_NONE):
        default:
            return AUDIO_S16;
    }
}

SdlOutput::SdlOutput()
    : m_bufferSize{0}
    , m_device{"default"}
{ }

SdlOutput::~SdlOutput()
{
    uninit();
}

QString SdlOutput::name() const
{
    return "SDL2";
}

QString SdlOutput::device() const
{
    return m_device;
}

void SdlOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

bool SdlOutput::init(OutputContext* oc)
{
    if(SDL_WasInit(SDL_INIT_AUDIO)) {
        return false;
    }

    SDL_Init(SDL_INIT_AUDIO);

    m_desiredSpec.freq     = oc->sampleRate;
    m_desiredSpec.format   = findFormat(oc->format);
    m_desiredSpec.channels = oc->channelLayout.nb_channels;
    m_desiredSpec.samples  = 2048;
    m_desiredSpec.callback = nullptr;

    if(m_device == "default") {
        m_audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &m_desiredSpec, &m_obtainedSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    }
    else {
        m_audioDeviceId = SDL_OpenAudioDevice(
            m_device.toLocal8Bit(), 0, &m_desiredSpec, &m_obtainedSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    }

    if(m_audioDeviceId == 0) {
        qDebug() << "SDL Error opening audio device: " << SDL_GetError();
        return false;
    }

    oc->bufferSize = 3 * m_obtainedSpec.samples;
    oc->sampleRate = m_obtainedSpec.freq;

    m_bufferSize = oc->bufferSize;

    return true;
}

void SdlOutput::uninit()
{
    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        return;
    }
    SDL_CloseAudioDevice(m_audioDeviceId);
    SDL_Quit();
}

void SdlOutput::reset()
{
    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        return;
    }
    SDL_PauseAudioDevice(m_audioDeviceId, 1);
    SDL_ClearQueuedAudio(m_audioDeviceId);
}

void SdlOutput::start()
{
    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        return;
    }
    if(SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(m_audioDeviceId, 0);
    }
}

int SdlOutput::write(OutputContext* oc, const uint8_t* data, int samples)
{
    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        return 0;
    }
    const int err = SDL_QueueAudio(m_audioDeviceId, data, samples * oc->sstride);
    if(err < 0) {
        qWarning() << "SDL write error: " << SDL_GetError();
        return 0;
    }
    return samples;
}

void SdlOutput::setPaused(bool pause)
{
    SDL_PauseAudioDevice(m_audioDeviceId, pause);
}

OutputState SdlOutput::currentState(OutputContext* oc)
{
    OutputState state;
    state.queuedSamples = SDL_GetQueuedAudioSize(m_audioDeviceId) / oc->sstride;
    state.freeSamples   = m_bufferSize - state.queuedSamples;
    return state;
}

OutputDevices SdlOutput::getAllDevices() const
{
    OutputDevices devices;
    bool tempInit{false};

    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        tempInit = true;
        SDL_Init(SDL_INIT_AUDIO);
    }

    devices.emplace("default", "Default");

    const int num = SDL_GetNumAudioDevices(0);
    for(int i = 0; i < num; ++i) {
        const QString devName = SDL_GetAudioDeviceName(i, 0);
        if(!devName.isNull()) {
            devices.emplace(devName, devName);
        }
    }

    if(tempInit) {
        SDL_Quit();
    }

    return devices;
}
} // namespace Fy::Core::Engine
