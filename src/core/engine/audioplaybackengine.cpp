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

#include "audioplaybackengine.h"

#include "audioclock.h"
#include "audiorenderer.h"
#include "engine/ffmpeg/ffmpegdecoder.h"
#include "internalcoresettings.h"

#include <core/coresettings.h>
#include <core/engine/audiobuffer.h>
#include <core/engine/audiodecoder.h>
#include <core/track.h>
#include <utils/settings/settingsmanager.h>

#include <QBasicTimer>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto BufferInterval = 5ms;
#else
constexpr auto BufferInterval = 5;
#endif

constexpr auto MaxDecodeLength = 1000;

namespace Fooyin {
struct AudioPlaybackEngine::Private
{
    AudioPlaybackEngine* self;

    SettingsManager* settings;

    AudioClock clock;
    QTimer* positionUpdateTimer{nullptr};

    TrackStatus status{TrackStatus::NoTrack};
    PlaybackState state{PlaybackState::Stopped};
    AudioOutput::State outputState{AudioOutput::State::None};

    uint64_t startPosition{0};
    uint64_t endPosition{0};
    uint64_t lastPosition{0};

    uint64_t totalBufferTime{0};
    uint64_t bufferLength{0};

    uint64_t duration{0};
    double volume{1.0};
    bool ending{false};
    bool pausing{false};

    Track currentTrack;
    AudioFormat format;

    std::unique_ptr<AudioDecoder> decoder;
    AudioRenderer* renderer;

    QBasicTimer bufferTimer;
    QBasicTimer pauseTimer;

    FadingIntervals fadeIntervals;

    explicit Private(AudioPlaybackEngine* self_, SettingsManager* settings_)
        : self{self_}
        , settings{settings_}
        , bufferLength{static_cast<uint64_t>(settings->value<Settings::Core::BufferLength>())}
        , decoder{std::make_unique<FFmpegDecoder>()}
        , renderer{new AudioRenderer(self)}
        , fadeIntervals{settings->value<Settings::Core::Internal::FadingIntervals>().value<FadingIntervals>()}
    {
        settings->subscribe<Settings::Core::BufferLength>(self, [this](int length) { bufferLength = length; });
        settings->subscribe<Settings::Core::Internal::FadingIntervals>(
            self, [this](const QVariant& fading) { fadeIntervals = fading.value<FadingIntervals>(); });

        QObject::connect(renderer, &AudioRenderer::bufferProcessed, self,
                         [this](const AudioBuffer& buffer) { totalBufferTime -= buffer.duration(); });
        QObject::connect(renderer, &AudioRenderer::finished, self, [this]() { onRendererFinished(); });
        QObject::connect(renderer, &AudioRenderer::outputStateChanged, self,
                         [this](AudioOutput::State outState) { handleOutputState(outState); });
    }

    QTimer* positionTimer()
    {
        if(!positionUpdateTimer) {
            positionUpdateTimer = new QTimer(self);
            positionUpdateTimer->setInterval(50ms);
            positionUpdateTimer->setTimerType(Qt::PreciseTimer);
            QObject::connect(positionUpdateTimer, &QTimer::timeout, self, [this]() { updatePosition(); });
        }
        return positionUpdateTimer;
    }

    void readNextBuffer()
    {
        if(totalBufferTime >= bufferLength) {
            return;
        }

        const auto bytesToEnd = static_cast<size_t>(format.bytesForDuration(endPosition - lastPosition));
        const auto bytesLeft
            = std::min(bytesToEnd, static_cast<size_t>(format.bytesForDuration(bufferLength - totalBufferTime)));
        const auto maxBytes = std::min(bytesLeft, static_cast<size_t>(format.bytesForDuration(MaxDecodeLength)));

        const auto buffer = decoder->readBuffer(maxBytes);
        if(buffer.isValid()) {
            totalBufferTime += buffer.duration();
            renderer->queueBuffer(buffer);
        }

        if(!buffer.isValid() || buffer.endTime() >= endPosition) {
            bufferTimer.stop();
            renderer->queueBuffer({});
            ending = true;
            QMetaObject::invokeMethod(self, &AudioEngine::trackAboutToFinish);
        }
    }

    void handleOutputState(AudioOutput::State outState)
    {
        outputState = outState;
        if(outputState == AudioOutput::State::Disconnected) {
            self->pause();
        }
    }

    void updateState(PlaybackState newState)
    {
        if(std::exchange(state, newState) != state) {
            emit self->stateChanged(newState);
        }
        clock.setPaused(newState != PlaybackState::Playing);
    }

    void stop()
    {
        auto delayedStop = [this]() {
            stopWorkers(true);
        };

        const bool canFade
            = settings->value<Settings::Core::Internal::EngineFading>() && fadeIntervals.outPauseStop > 0;
        if(canFade) {
            QObject::connect(renderer, &AudioRenderer::paused, self, delayedStop, Qt::SingleShotConnection);
            renderer->pause(true, fadeIntervals.outPauseStop);
        }
        else {
            stopWorkers(true);
        }

        updateState(PlaybackState::Stopped);
        positionTimer()->stop();
        lastPosition = 0;
        emit self->positionChanged(0);
    }

    void pause()
    {
        auto delayedPause = [this]() {
            pauseOutput(true);
            updateState(PlaybackState::Paused);
            clock.setPaused(true);
        };

        QObject::connect(renderer, &AudioRenderer::paused, self, delayedPause, Qt::SingleShotConnection);

        const int fadeInterval
            = settings->value<Settings::Core::Internal::EngineFading>() ? fadeIntervals.outPauseStop : 0;
        renderer->pause(true, fadeInterval);
    }

    void play()
    {
        if(outputState == AudioOutput::State::Disconnected) {
            if(renderer->init(format)) {
                outputState = AudioOutput::State::None;
            }
            else {
                updateState(PlaybackState::Error);
                return;
            }
        }

        const auto prevState = state;

        startPlayback();
        if(state == PlaybackState::Stopped && currentTrack.offset() > 0) {
            decoder->seek(currentTrack.offset());
        }

        updateState(PlaybackState::Playing);

        const bool canFade = settings->value<Settings::Core::Internal::EngineFading>()
                          && (prevState == PlaybackState::Paused || renderer->isFading());
        const int fadeInterval = canFade ? fadeIntervals.inPauseStop : 0;

        renderer->pause(false, fadeInterval);
        changeTrackStatus(TrackStatus::BufferedTrack);
        positionTimer()->start();
    }

    void enterErrorState()
    {
        updateState(PlaybackState::Error);
    }

    TrackStatus changeTrackStatus(TrackStatus newStatus)
    {
        auto prevStatus = std::exchange(status, newStatus);
        if(prevStatus != status) {
            emit self->trackStatusChanged(status);
        }
        return prevStatus;
    }

    void startBufferTimer()
    {
        bufferTimer.start(BufferInterval, self);
    }

    void updatePosition()
    {
        const auto currentPosition = startPosition + clock.currentPosition();
        if(std::exchange(lastPosition, currentPosition) != lastPosition) {
            emit self->positionChanged(lastPosition - startPosition);
        }

        if(currentTrack.hasCue() && lastPosition >= endPosition) {
            clock.setPaused(true);
            clock.sync(duration);
            changeTrackStatus(TrackStatus::EndOfTrack);
        }
    }

    bool updateFormat(const AudioFormat& nextFormat)
    {
        const auto prevFormat = std::exchange(format, nextFormat);

        if(settings->value<Settings::Core::GaplessPlayback>() && prevFormat == format
           && state != PlaybackState::Paused) {
            return true;
        }

        if(!renderer->init(format)) {
            format = {};
            return false;
        }

        return true;
    }

    void startPlayback()
    {
        decoder->start();
        startBufferTimer();
        renderer->start();
    }

    void onRendererFinished()
    {
        if(currentTrack.hasCue()) {
            return;
        }

        clock.setPaused(true);
        clock.sync(duration);

        changeTrackStatus(TrackStatus::EndOfTrack);
    }

    void pauseOutput(bool pause)
    {
        if(pause) {
            bufferTimer.stop();
        }
        else {
            startBufferTimer();
        }
    }

    void resetWorkers()
    {
        bufferTimer.stop();
        clock.setPaused(true);
        renderer->reset();
        totalBufferTime = 0;
    }

    void stopWorkers(bool full = false)
    {
        bufferTimer.stop();
        clock.setPaused(true);
        clock.sync();
        renderer->stop();
        if(full) {
            renderer->closeOutput();
            outputState = AudioOutput::State::Disconnected;
        }
        decoder->stop();
        totalBufferTime = 0;
    }
};

AudioPlaybackEngine::AudioPlaybackEngine(SettingsManager* settings, QObject* parent)
    : AudioEngine{parent}
    , p{std::make_unique<Private>(this, settings)}
{ }

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    AudioPlaybackEngine::stop();

    p->stopWorkers();

    if(p->positionUpdateTimer) {
        p->positionUpdateTimer->deleteLater();
    }
}

void AudioPlaybackEngine::seek(uint64_t pos)
{
    if(!p->decoder->isSeekable()) {
        return;
    }

    p->resetWorkers();

    p->decoder->seek(pos + p->startPosition);
    p->clock.sync(pos);

    if(p->state == PlaybackState::Playing) {
        p->clock.setPaused(false);
        p->startBufferTimer();
        p->renderer->start();
    }
    else {
        p->updatePosition();
    }
}

void AudioPlaybackEngine::changeTrack(const Track& track)
{
    const Track prevTrack = std::exchange(p->currentTrack, track);

    auto setupDuration = [this, &track]() {
        p->duration      = track.duration();
        p->startPosition = track.offset();
        p->endPosition   = p->startPosition + p->duration;
        p->lastPosition  = p->startPosition;
    };

    if(p->ending && track.filepath() == prevTrack.filepath() && p->endPosition == track.offset()) {
        emit positionChanged(0);
        p->ending = false;
        p->clock.sync(0);
        setupDuration();
        p->changeTrackStatus(TrackStatus::BufferedTrack);
        if(p->state == PlaybackState::Playing) {
            p->play();
        }
        return;
    }

    p->stopWorkers();

    emit positionChanged(0);

    p->ending = false;
    p->clock.setPaused(true);
    p->clock.sync();

    if(!track.isValid()) {
        p->changeTrackStatus(TrackStatus::InvalidTrack);
        return;
    }

    p->changeTrackStatus(TrackStatus::LoadingTrack);

    if(!p->decoder->init(track.filepath())) {
        p->changeTrackStatus(TrackStatus::InvalidTrack);
        return;
    }

    if(!p->updateFormat(p->decoder->format())) {
        p->enterErrorState();
        p->changeTrackStatus(TrackStatus::NoTrack);
        return;
    }

    p->changeTrackStatus(TrackStatus::LoadedTrack);

    setupDuration();

    if(track.offset() > 0) {
        p->decoder->seek(track.offset());
    }

    if(p->state == PlaybackState::Playing) {
        p->play();
    }
}

void AudioPlaybackEngine::play()
{
    if(p->status == TrackStatus::NoTrack || p->status == TrackStatus::InvalidTrack) {
        return;
    }

    if(p->status == TrackStatus::EndOfTrack && p->state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }

    p->play();
}

void AudioPlaybackEngine::pause()
{
    if(p->status == TrackStatus::NoTrack || p->status == TrackStatus::InvalidTrack) {
        return;
    }

    if(p->status == TrackStatus::EndOfTrack && p->state == PlaybackState::Stopped) {
        seek(0);
        emit positionChanged(0);
    }
    else {
        p->pause();
    }
}

void AudioPlaybackEngine::stop()
{
    if(p->state != PlaybackState::Stopped) {
        p->stop();
    }
}

void AudioPlaybackEngine::setVolume(double volume)
{
    p->volume = volume;
    p->renderer->updateVolume(volume);
}

void AudioPlaybackEngine::setAudioOutput(const OutputCreator& output, const QString& device)
{
    const bool playing = p->state == PlaybackState::Playing;

    if(playing) {
        p->clock.setPaused(true);
        p->renderer->pause(true);
    }

    p->renderer->updateOutput(output, device);

    if(playing) {
        if(!p->renderer->init(p->format)) {
            p->changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        p->clock.setPaused(false);
        p->renderer->start();
        p->renderer->pause(false);
    }
}

void AudioPlaybackEngine::setOutputDevice(const QString& device)
{
    if(device.isEmpty()) {
        return;
    }

    const bool playing = p->state == PlaybackState::Playing;

    if(playing) {
        p->clock.setPaused(true);
        p->renderer->pause(true);
    }

    p->renderer->updateDevice(device);

    if(playing) {
        if(!p->renderer->init(p->format)) {
            p->changeTrackStatus(TrackStatus::NoTrack);
            return;
        }

        p->clock.setPaused(false);
        p->startPlayback();
        p->renderer->pause(false);
    }
}

void AudioPlaybackEngine::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->bufferTimer.timerId()) {
        p->readNextBuffer();
    }

    QObject::timerEvent(event);
}
} // namespace Fooyin

#include "moc_audioplaybackengine.cpp"
