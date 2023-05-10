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

#include "audioplayer.h"

#include "audioengine.h"
#include "core/engine/ffmpeg/ffmpegengine.h"
#include "core/engine/output/alsaoutput.h"
#include "core/models/track.h"
#include "engine/output/sdloutput.h"

namespace Fy::Core::Engine {
struct AudioPlayer::Private
{
    AudioEngine* backend;
    PlaybackState state;
    AudioOutput* audioOutput;
    Track track;

    Private()
        : backend{nullptr}
        , state{StoppedState}
        , audioOutput{nullptr}
    { }
};

AudioPlayer::AudioPlayer(QObject* parent)
    : Utils::Worker{parent}
    , p{std::make_unique<Private>()}
{ }

AudioPlayer::~AudioPlayer()
{
    disconnect();
}

void AudioPlayer::init()
{
    p->backend     = new FFmpeg::FFmpegEngine(this);
    p->audioOutput = new SdlOutput(this);

    p->backend->setAudioOutput(p->audioOutput);
}

Track AudioPlayer::track() const
{
    return p->track;
}

PlaybackState AudioPlayer::playbackState() const
{
    return p->state;
}

TrackStatus AudioPlayer::trackStatus() const
{
    return p->backend ? p->backend->trackStatus() : NoTrack;
}

uint64_t AudioPlayer::position() const
{
    return p->backend ? p->backend->position() : 0;
}

bool AudioPlayer::isPlaying() const
{
    return p->state == PlayingState;
}

void AudioPlayer::play()
{
    if(!p->backend) {
        return;
    }
    p->backend->play();
}

void AudioPlayer::pause()
{
    if(p->backend) {
        p->backend->pause();
    }
}

void AudioPlayer::stop()
{
    if(p->backend) {
        p->backend->stop();
    }
}

void AudioPlayer::seek(uint64_t position)
{
    if(!p->backend) {
        return;
    }
    p->backend->seek(position);
}

void AudioPlayer::setVolume(float /*value*/) { }

void AudioPlayer::changeTrack(const Track& track)
{
    if(!p->backend) {
        return;
    }

    p->track = track;
    p->backend->changeTrack(p->track.filepath());

    emit trackChanged(p->track);
}

void AudioPlayer::setState(PlaybackState ps)
{
    if(p->state != ps) {
        p->state = ps;
        emit playbackStateChanged(ps);
    }
}

void AudioPlayer::setStatus(TrackStatus status)
{
    emit trackStatusChanged(status);
}
} // namespace Fy::Core::Engine
