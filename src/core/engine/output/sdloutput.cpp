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
#include <SDL2/SDL_audio.h>

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

struct SdlOutput::Private
{
    int bufferSize{0};
    SDL_AudioSpec desiredSpec;
    SDL_AudioSpec audioSpec;
    SDL_AudioDeviceID audioDeviceId;
};

SdlOutput::SdlOutput(QObject* parent)
    : AudioOutput{parent}
    , p{std::make_unique<Private>()}
{
    SDL_Init(SDL_INIT_AUDIO);
}

SdlOutput::~SdlOutput()
{
    SDL_CloseAudioDevice(p->audioDeviceId);
    SDL_Quit();
}

void SdlOutput::init(OutputContext* of)
{
    SDL_ClearQueuedAudio(p->audioDeviceId);
    SDL_CloseAudioDevice(p->audioDeviceId);

    p->desiredSpec.freq     = of->sampleRate;
    p->desiredSpec.format   = findFormat(of->format);
    p->desiredSpec.channels = of->channelLayout.nb_channels;
    p->desiredSpec.samples  = 2048;
    p->desiredSpec.callback = nullptr;

    p->audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &p->desiredSpec, &p->audioSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
}

void SdlOutput::start()
{
    if(SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(p->audioDeviceId, 0);
    }
}

int SdlOutput::write(const char* data, int size)
{
    int buf = SDL_GetQueuedAudioSize(p->audioDeviceId);
    if(buf >= p->bufferSize) {
        return 0;
    }
    const int success = SDL_QueueAudio(p->audioDeviceId, data, size);
    if(success == 0) {
        return size;
    }
    return success;
}

void SdlOutput::setPaused(bool pause)
{
    SDL_PauseAudioDevice(p->audioDeviceId, pause);
}

int SdlOutput::bufferSize() const
{
    return p->bufferSize;
}

void SdlOutput::setBufferSize(int size)
{
    p->bufferSize = size * 10;
}

void SdlOutput::clearBuffer()
{
    SDL_ClearQueuedAudio(p->audioDeviceId);
}
} // namespace Fy::Core::Engine
