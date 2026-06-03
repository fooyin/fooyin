/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <QLoggingCategory>
#include <QMetaObject>

#include <algorithm>
#include <bit>
#include <limits>

Q_LOGGING_CATEGORY(SDL, "fy.sdl")

using namespace Qt::StringLiterals;

constexpr auto SdlPeriodMs       = 40;
constexpr auto SdlTargetBufferMs = 200;

namespace {
SDL_AudioFormat findFormat(Fooyin::SampleFormat format)
{
    switch(format) {
        case(Fooyin::SampleFormat::U8):
            return AUDIO_U8;
        case(Fooyin::SampleFormat::S16):
            return AUDIO_S16SYS;
        case(Fooyin::SampleFormat::S24In32):
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

int requestedDeviceBufferFrames(const Fooyin::AudioFormat& format)
{
    const int frames     = std::max(1, format.framesForDuration(SdlPeriodMs));
    const auto po2Frames = std::bit_ceil(static_cast<uint32_t>(frames));
    return static_cast<int>(std::min<uint32_t>(po2Frames, std::numeric_limits<Uint16>::max()));
}

int targetQueueFrames(const Fooyin::AudioFormat& format, const SDL_AudioSpec& obtainedSpec)
{
    const int obtainedFrames = std::max(1, static_cast<int>(obtainedSpec.samples));
    const int durationFrames = std::max(1, format.framesForDuration(SdlTargetBufferMs));
    return std::max(durationFrames, obtainedFrames * 4);
}
} // namespace

namespace Fooyin::Sdl {
SdlOutput::SdlOutput()
    : m_deviceBufferFrames{0}
    , m_targetBufferFrames{0}
    , m_initialised{false}
    , m_device{u"default"_s}
    , m_desiredSpec{}
    , m_obtainedSpec{}
    , m_audioDeviceId{0}
    , m_event{}
{
#ifdef Q_OS_WIN32
    SDL_setenv("SDL_AUDIODRIVER", "directsound", true); // WASAPI driver (default) is broken
#endif
}

bool SdlOutput::init(const AudioFormat& format)
{
    m_format = format;

    if(SDL_Init(SDL_INIT_AUDIO) != 0) {
        qCWarning(SDL) << "Error initialising SDL audio:" << SDL_GetError();
        return false;
    }

    m_desiredSpec.freq     = format.sampleRate();
    m_desiredSpec.format   = findFormat(format.sampleFormat());
    m_desiredSpec.channels = format.channelCount();
    m_desiredSpec.samples  = static_cast<Uint16>(requestedDeviceBufferFrames(format));
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

    m_deviceBufferFrames = std::max(1, static_cast<int>(m_obtainedSpec.samples));

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

    m_targetBufferFrames = targetQueueFrames(m_format, m_obtainedSpec);
    qCDebug(SDL) << "SDL backend initialised with device period:" << m_deviceBufferFrames
                 << "frames and queue target:" << m_targetBufferFrames << "frames (" << SdlTargetBufferMs << "ms)";

    m_initialised = true;
    return true;
}

void SdlOutput::uninit()
{
    if(m_audioDeviceId != 0) {
        SDL_CloseAudioDevice(m_audioDeviceId);
        m_audioDeviceId = 0;
    }

    SDL_Quit();

    m_deviceBufferFrames = 0;
    m_targetBufferFrames = 0;
    m_initialised        = false;
}

void SdlOutput::reset()
{
    SDL_PauseAudioDevice(m_audioDeviceId, 1);
    SDL_ClearQueuedAudio(m_audioDeviceId);
}

void SdlOutput::start()
{
    if(SDL_GetAudioDeviceStatus(m_audioDeviceId) != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(m_audioDeviceId, 0);
    }
    checkEvents();
}

void SdlOutput::drain()
{
    while(SDL_GetQueuedAudioSize(m_audioDeviceId) > 0) {
        checkEvents();
        SDL_Delay(20);
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
    return std::max(1, m_targetBufferFrames);
}

OutputState SdlOutput::currentState()
{
    OutputState state;
    checkEvents();

    state.queuedFrames = queuedFrames();
    state.freeFrames   = std::max(0, bufferSize() - state.queuedFrames);
    if(m_format.sampleRate() > 0) {
        state.delay = static_cast<double>(state.queuedFrames) / static_cast<double>(m_format.sampleRate());
    }

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

int SdlOutput::write(const std::span<const std::byte> data, const int frameCount)
{
    checkEvents();
    if(frameCount <= 0 || m_audioDeviceId == 0) {
        return 0;
    }

    const int bytesPerFrame = m_format.bytesPerFrame();
    if(bytesPerFrame <= 0) {
        return 0;
    }

    const size_t availableFrames = data.size() / static_cast<size_t>(bytesPerFrame);
    const int framesToWrite      = std::min(frameCount, static_cast<int>(availableFrames));
    if(framesToWrite <= 0) {
        return 0;
    }

    const int acceptedFrames = std::min(framesToWrite, std::max(0, bufferSize() - queuedFrames()));
    if(acceptedFrames <= 0) {
        return 0;
    }

    const size_t byteCount = static_cast<size_t>(acceptedFrames) * static_cast<size_t>(bytesPerFrame);
    if(SDL_QueueAudio(m_audioDeviceId, data.data(), byteCount) == 0) {
        return acceptedFrames;
    }

    return 0;
}

void SdlOutput::setPaused(bool pause)
{
    SDL_PauseAudioDevice(m_audioDeviceId, pause);
}

bool SdlOutput::supportsVolumeControl() const
{
    return false;
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

void SdlOutput::checkEvents()
{
    while(SDL_PollEvent(&m_event)) {
        switch(m_event.type) {
            case(SDL_AUDIODEVICEREMOVED):
                QMetaObject::invokeMethod(
                    this, [this]() { Q_EMIT stateChanged(AudioOutput::State::Disconnected); }, Qt::QueuedConnection);
            default:
                break;
        }
    }
}

int SdlOutput::queuedFrames() const
{
    const int bytesPerFrame = m_format.bytesPerFrame();
    if(m_audioDeviceId == 0 || bytesPerFrame <= 0) {
        return 0;
    }

    const auto queuedBytes = SDL_GetQueuedAudioSize(m_audioDeviceId);
    return static_cast<int>(queuedBytes / static_cast<Uint32>(bytesPerFrame));
}
} // namespace Fooyin::Sdl
