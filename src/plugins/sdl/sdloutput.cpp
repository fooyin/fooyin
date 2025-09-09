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

#include "sdloutput.h"

#include <SDL2/SDL.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QTimerEvent>

Q_LOGGING_CATEGORY(SDL, "fy.sdl")

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto EventInterval = 200ms;
#else
constexpr auto EventInterval = 200;
#endif

namespace {
SDL_AudioFormat findFormat(Fooyin::SampleFormat format)
{
    switch(format) {
        case(Fooyin::SampleFormat::U8):
            return AUDIO_U8;
        case(Fooyin::SampleFormat::S16):
            return AUDIO_S16SYS;
        case(Fooyin::SampleFormat::S24):
        case(Fooyin::SampleFormat::S32):
            return AUDIO_S32SYS;
        case(Fooyin::SampleFormat::F32):
        case(Fooyin::SampleFormat::F64):
            return AUDIO_F32SYS;
        case(Fooyin::SampleFormat::Unknown):
        default:
            return AUDIO_S16;
    }
}

Fooyin::SampleFormat findSampleFormat(SDL_AudioFormat format)
{
    switch(format) {
        case(AUDIO_U8):
            return Fooyin::SampleFormat::U8;
        case(AUDIO_S16SYS):
            return Fooyin::SampleFormat::S16;
        case(AUDIO_S32SYS):
            return Fooyin::SampleFormat::S32;
        case(AUDIO_F32SYS):
            return Fooyin::SampleFormat::F32;
        default:
            return Fooyin::SampleFormat::Unknown;
    }
}
} // namespace

namespace Fooyin::Sdl {
SdlOutput::SdlOutput()
    : m_bufferSize{8192}
    , m_initialised{false}
    , m_device{u"default"_s}
    , m_volume{1.0}
{ }

bool SdlOutput::init(const AudioFormat& format)
{
    m_format = format;

    SDL_Init(SDL_INIT_AUDIO);

    m_desiredSpec.freq     = format.sampleRate();
    m_desiredSpec.format   = findFormat(format.sampleFormat());
    m_desiredSpec.channels = format.channelCount();
    m_desiredSpec.samples  = m_bufferSize;
    m_desiredSpec.callback = nullptr;

    if(m_device == u"default"_s) {
        m_audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &m_desiredSpec, &m_obtainedSpec, SDL_AUDIO_ALLOW_ANY_CHANGE);
    }
    else {
        m_audioDeviceId = SDL_OpenAudioDevice(m_device.toLocal8Bit().constData(), 0, &m_desiredSpec, &m_obtainedSpec,
                                              SDL_AUDIO_ALLOW_ANY_CHANGE);
    }

    if(m_audioDeviceId == 0) {
        qCWarning(SDL) << "Error opening audio device:" << SDL_GetError();
        return false;
    }

    if(format.sampleFormat() == SampleFormat::F64 || m_obtainedSpec.format != m_desiredSpec.format) {
        qCDebug(SDL) << "Format not supported:" << m_format.prettyFormat();
        m_format.setSampleFormat(findSampleFormat(m_obtainedSpec.format));
        qCDebug(SDL) << "Using compatible format:" << m_format.prettyFormat();
    }
    if(m_obtainedSpec.freq != m_desiredSpec.freq) {
        qCDebug(SDL) << "Sample rate not supported:" << m_desiredSpec.freq << "Hz";
        qCDebug(SDL) << "Using sample rate:" << m_obtainedSpec.freq << "Hz";
        m_format.setSampleRate(m_obtainedSpec.freq);
    }
    if(m_obtainedSpec.channels != m_desiredSpec.channels) {
        qCDebug(SDL) << "Using channels:" << m_obtainedSpec.channels;
        m_format.setChannelCount(m_obtainedSpec.channels);
    }

    m_initialised = true;
    return true;
}

void SdlOutput::uninit()
{
    m_eventTimer.stop();
    SDL_CloseAudioDevice(m_audioDeviceId);
    SDL_Quit();

    m_initialised = false;
}

void SdlOutput::reset()
{
    SDL_PauseAudioDevice(m_audioDeviceId, 1);
    SDL_ClearQueuedAudio(m_audioDeviceId);
}

void SdlOutput::start()
{
    if(SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(m_audioDeviceId, 0);
        m_eventTimer.start(EventInterval, this);
    }
}

void SdlOutput::drain()
{
    while(SDL_GetQueuedAudioSize(m_audioDeviceId) > 0) {
        checkEvents();
        SDL_Delay(1000);
    }
}

bool SdlOutput::initialised() const
{
    return m_initialised;
}

QString SdlOutput::device() const
{
    return m_device;
}

int SdlOutput::bufferSize() const
{
    return m_bufferSize;
}

OutputState SdlOutput::currentState()
{
    OutputState state;

    state.queuedSamples = static_cast<int>(SDL_GetQueuedAudioSize(m_audioDeviceId) / m_format.bytesPerFrame());
    state.freeSamples   = m_bufferSize - state.queuedSamples;

    return state;
}

OutputDevices SdlOutput::getAllDevices(bool isCurrentOutput)
{
    OutputDevices devices;

    if(!isCurrentOutput) {
        SDL_Init(SDL_INIT_AUDIO);
    }

    devices.emplace_back(u"default"_s, u"Default"_s);

    const int num = SDL_GetNumAudioDevices(0);
    for(int i = 0; i < num; ++i) {
        const QString devName = QString::fromUtf8(SDL_GetAudioDeviceName(i, 0));
        if(!devName.isNull()) {
            devices.emplace_back(devName, devName);
        }
    }

    if(!isCurrentOutput) {
        SDL_Quit();
    }

    return devices;
}

int SdlOutput::write(const AudioBuffer& buffer)
{
    AudioBuffer adjustedBuffer{buffer};
    adjustedBuffer.scale(m_volume);

    if(SDL_QueueAudio(m_audioDeviceId, adjustedBuffer.constData().data(), buffer.byteCount()) == 0) {
        return buffer.sampleCount();
    }

    return 0;
}

void SdlOutput::setPaused(bool pause)
{
    SDL_PauseAudioDevice(m_audioDeviceId, pause);
}

void SdlOutput::setVolume(double volume)
{
    m_volume = volume;
}

void SdlOutput::setDevice(const QString& device)
{
    if(!device.isEmpty()) {
        m_device = device;
    }
}

QString SdlOutput::error() const
{
    // TODO
    return {};
}

AudioFormat SdlOutput::format() const
{
    return m_format;
}

void SdlOutput::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_eventTimer.timerId()) {
        checkEvents();
    }

    AudioOutput::timerEvent(event);
}

void SdlOutput::checkEvents()
{
    while(SDL_PollEvent(&m_event)) {
        switch(m_event.type) {
            case(SDL_AUDIODEVICEREMOVED):
                emit stateChanged(AudioOutput::State::Disconnected);
            default:
                break;
        }
    }
}
} // namespace Fooyin::Sdl
