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

#include "sdloutput.h"

#include <SDL2/SDL.h>

#include <QDebug>

using namespace Qt::Literals::StringLiterals;

constexpr auto BufferSize = 1024;

namespace {
SDL_AudioFormat findFormat(Fooyin::AudioFormat::SampleFormat format)
{
    switch(format) {
        case(Fooyin::AudioFormat::UInt8):
            return AUDIO_U8;
        case(Fooyin::AudioFormat::Int16):
            return AUDIO_S16SYS;
        case(Fooyin::AudioFormat::Int32):
            return AUDIO_S32SYS;
        case(Fooyin::AudioFormat::Float):
            return AUDIO_F32SYS;
        case(Fooyin::AudioFormat::Double):
        default:
            return AUDIO_S16;
    }
}

void audioCallback(void* userData, uint8_t* stream, int len)
{
    auto* outputContext = static_cast<Fooyin::OutputContext*>(userData);

    const int sstride = outputContext->format.bytesPerFrame();

    if(len % sstride) {
        qWarning() << "SDL audio callback not sample aligned";
    }

    const int samples = len / sstride;

    outputContext->writeAudioToBuffer(stream, samples);
}
} // namespace

namespace Fooyin::Sdl {
struct SdlOutput::Private
{
    OutputContext outputContext;

    bool initialised{false};

    SDL_AudioSpec desiredSpec;
    SDL_AudioSpec obtainedSpec;
    SDL_AudioDeviceID audioDeviceId;

    QString device{u"default"_s};
};

SdlOutput::SdlOutput()
    : p{std::make_unique<Private>()}
{ }

SdlOutput::~SdlOutput() = default;

bool SdlOutput::init(const OutputContext& oc)
{
    if(SDL_WasInit(SDL_INIT_AUDIO)) {
        return false;
    }

    p->outputContext = oc;

    SDL_Init(SDL_INIT_AUDIO);

    p->desiredSpec.freq     = oc.format.sampleRate();
    p->desiredSpec.format   = findFormat(oc.format.sampleFormat());
    p->desiredSpec.channels = oc.channelLayout.nb_channels;
    p->desiredSpec.samples  = BufferSize;
    p->desiredSpec.callback = audioCallback;
    p->desiredSpec.userdata = &p->outputContext;

    if(p->device == "default"_L1) {
        p->audioDeviceId
            = SDL_OpenAudioDevice(nullptr, 0, &p->desiredSpec, &p->obtainedSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    }
    else {
        p->audioDeviceId = SDL_OpenAudioDevice(p->device.toLocal8Bit(), 0, &p->desiredSpec, &p->obtainedSpec,
                                               SDL_AUDIO_ALLOW_ANY_CHANGE);
    }

    if(p->audioDeviceId == 0) {
        qDebug() << "SDL Error opening audio device: " << SDL_GetError();
        return false;
    }

    p->initialised = true;
    return true;
}

void SdlOutput::uninit()
{
    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        return;
    }
    SDL_CloseAudioDevice(p->audioDeviceId);
    SDL_Quit();

    p->initialised = false;
}

void SdlOutput::reset()
{
    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        return;
    }
    SDL_PauseAudioDevice(p->audioDeviceId, 1);
    SDL_ClearQueuedAudio(p->audioDeviceId);
}

void SdlOutput::start()
{
    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        return;
    }
    if(SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(p->audioDeviceId, 0);
    }
}

bool SdlOutput::initialised() const
{
    return p->initialised;
}

QString SdlOutput::device() const
{
    return p->device;
}

bool SdlOutput::canHandleVolume() const
{
    return false;
}

OutputDevices SdlOutput::getAllDevices() const
{
    OutputDevices devices;
    bool tempInit{false};

    if(!SDL_WasInit(SDL_INIT_AUDIO)) {
        tempInit = true;
        SDL_Init(SDL_INIT_AUDIO);
    }

    devices.emplace_back("default", "Default");

    const int num = SDL_GetNumAudioDevices(0);
    for(int i = 0; i < num; ++i) {
        const QString devName = SDL_GetAudioDeviceName(i, 0);
        if(!devName.isNull()) {
            devices.emplace_back(devName, devName);
        }
    }

    if(tempInit) {
        SDL_Quit();
    }

    return devices;
}

void SdlOutput::setPaused(bool pause)
{
    SDL_PauseAudioDevice(p->audioDeviceId, pause);
}

void SdlOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        p->device = device;
    }
}
} // namespace Fooyin::Sdl
