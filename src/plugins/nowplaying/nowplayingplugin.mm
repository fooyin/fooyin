/*
 * Fooyin
 * Copyright © 2025, Carter Li <zhangsongcui@live.cn>
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

#include "nowplayingplugin.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>
#include <utils/enum.h>

#include <QLoggingCategory>
#include <QPixmap>

#import <AppKit/AppKit.h>
#import <MediaPlayer/MediaPlayer.h>

Q_LOGGING_CATEGORY(NOWPLAYING, "fy.nowplaying")

using namespace Qt::StringLiterals;

#define M_NOW_PLAYING_INFO reinterpret_cast<NSMutableDictionary*>(m_nowPlayingInfo)
#define M_REMOTE_TARGET reinterpret_cast<NowPlayingRemoteTarget*>(m_remoteTarget)

// Objective-C class to act as a target for remote commands
@interface NowPlayingRemoteTarget : NSObject
@property (nonatomic, assign) Fooyin::NowPlaying::NowPlayingPlugin* plugin;
@end

@implementation NowPlayingRemoteTarget
- (MPRemoteCommandHandlerStatus)play { self.plugin->playerController()->play(); return MPRemoteCommandHandlerStatusSuccess; }
- (MPRemoteCommandHandlerStatus)pause { self.plugin->playerController()->pause(); return MPRemoteCommandHandlerStatusSuccess; }
- (MPRemoteCommandHandlerStatus)togglePlayPause { self.plugin->playerController()->playPause(); return MPRemoteCommandHandlerStatusSuccess; }
- (MPRemoteCommandHandlerStatus)nextTrack { self.plugin->playerController()->next(); return MPRemoteCommandHandlerStatusSuccess; }
- (MPRemoteCommandHandlerStatus)previousTrack { self.plugin->playerController()->previous(); return MPRemoteCommandHandlerStatusSuccess; }
- (MPRemoteCommandHandlerStatus)changePlaybackPosition:(MPChangePlaybackPositionCommandEvent *)event {
    self.plugin->playerController()->seek(static_cast<uint64_t>(event.positionTime * 1000));
    return MPRemoteCommandHandlerStatusSuccess;
}
- (MPRemoteCommandHandlerStatus)changeRepeatModeCommand:(MPChangeRepeatModeCommandEvent *)event {
    auto currentMode = self.plugin->playerController()->playMode();
    currentMode &= ~(Fooyin::Playlist::RepeatPlaylist | Fooyin::Playlist::RepeatAlbum | Fooyin::Playlist::RepeatTrack);

    switch (event.repeatType) {
    case MPRepeatTypeOff:
        break;
    case MPRepeatTypeOne:
        currentMode |= Fooyin::Playlist::RepeatTrack;
        break;
    case MPRepeatTypeAll:
        currentMode |= Fooyin::Playlist::RepeatPlaylist;
        break;
    }
    self.plugin->playerController()->setPlayMode(currentMode);
    return MPRemoteCommandHandlerStatusSuccess;
}
- (MPRemoteCommandHandlerStatus)changeShuffleModeCommand:(MPChangeShuffleModeCommandEvent *)event {
    auto currentMode = self.plugin->playerController()->playMode();
    currentMode &= ~(Fooyin::Playlist::ShuffleAlbums | Fooyin::Playlist::ShuffleTracks);

    switch (event.shuffleType) {
    case MPShuffleTypeOff:
        break;
    case MPShuffleTypeItems:
        currentMode |= Fooyin::Playlist::ShuffleTracks;
        break;
    case MPShuffleTypeCollections:
        currentMode |= Fooyin::Playlist::ShuffleAlbums;
        break;
    }
    self.plugin->playerController()->setPlayMode(currentMode);
    return MPRemoteCommandHandlerStatusSuccess;
}
- (MPRemoteCommandHandlerStatus)seekForwardCommand {
    self.plugin->playerController()->seekForward(self.plugin->settings()->value<Fooyin::Settings::Gui::SeekStepSmall>());
    return MPRemoteCommandHandlerStatusSuccess;
}
- (MPRemoteCommandHandlerStatus)seekBackwardCommand {
    self.plugin->playerController()->seekBackward(self.plugin->settings()->value<Fooyin::Settings::Gui::SeekStepSmall>());
    return MPRemoteCommandHandlerStatusSuccess;
}
- (MPRemoteCommandHandlerStatus)ratingCommand:(MPRatingCommandEvent *)event {
    self.plugin->playerController()->currentTrack().setRating(event.rating);
    return MPRemoteCommandHandlerStatusSuccess;
}
@end


namespace Fooyin::NowPlaying {

template <typename Fn>
struct on_scope_exit {
    on_scope_exit(Fn &&fn): _fn(std::move(fn)) {}
    ~on_scope_exit() { this->_fn(); }

private:
    Fn _fn;
};

NowPlayingPlugin::NowPlayingPlugin()
    : m_nowPlayingInfo([NSMutableDictionary new])
    , m_remoteTarget([NowPlayingRemoteTarget new])
{
    M_REMOTE_TARGET.plugin = this;
}

NowPlayingPlugin::~NowPlayingPlugin()
{
    if(m_nowPlayingInfo) {
        [M_NOW_PLAYING_INFO release];
    }
    if(m_remoteTarget) {
        [M_REMOTE_TARGET release];
    }
}

void NowPlayingPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_playlistHandler  = context.playlistHandler;
    m_audioLoader      = context.audioLoader;
    m_settings         = context.settingsManager;

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &NowPlayingPlugin::playStateChanged);
    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this, &NowPlayingPlugin::trackChanged);
    QObject::connect(m_playerController, &PlayerController::positionMoved, this, &NowPlayingPlugin::positionMoved);
}

void NowPlayingPlugin::initialise(const GuiPluginContext& context)
{
    Q_UNUSED(context);
    m_coverProvider = new CoverProvider(m_audioLoader, m_settings, this);
    m_coverProvider->setUsePlaceholder(false);

    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
        if(m_playerController->currentTrack().id() == track.id()) {
            updateNowPlayingInfo();
        }
    });

    setupRemoteCommands();
}

void NowPlayingPlugin::shutdown()
{
    [M_NOW_PLAYING_INFO removeAllObjects];
    MPNowPlayingInfoCenter.defaultCenter.nowPlayingInfo = nil;
    MPNowPlayingInfoCenter.defaultCenter.playbackState = MPNowPlayingPlaybackStateStopped;

    MPRemoteCommandCenter* commandCenter = MPRemoteCommandCenter.sharedCommandCenter;
    id target = M_REMOTE_TARGET;
    [commandCenter.playCommand removeTarget:target];
    [commandCenter.pauseCommand removeTarget:target];
    [commandCenter.togglePlayPauseCommand removeTarget:target];
    [commandCenter.nextTrackCommand removeTarget:target];
    [commandCenter.previousTrackCommand removeTarget:target];
    [commandCenter.changePlaybackPositionCommand removeTarget:target];
    [commandCenter.changeRepeatModeCommand removeTarget:target];
    [commandCenter.changeShuffleModeCommand removeTarget:target];
    [commandCenter.seekForwardCommand removeTarget:target];
    [commandCenter.seekBackwardCommand removeTarget:target];
    [commandCenter.ratingCommand removeTarget:target];
}

void NowPlayingPlugin::setupRemoteCommands()
{
    MPRemoteCommandCenter* commandCenter = MPRemoteCommandCenter.sharedCommandCenter;
    id target = M_REMOTE_TARGET;

    [commandCenter.playCommand addTarget:target action:@selector(play)];
    commandCenter.playCommand.enabled = YES;

    [commandCenter.pauseCommand addTarget:target action:@selector(pause)];
    commandCenter.pauseCommand.enabled = YES;

    [commandCenter.togglePlayPauseCommand addTarget:target action:@selector(togglePlayPause)];
    commandCenter.togglePlayPauseCommand.enabled = YES;

    [commandCenter.nextTrackCommand addTarget:target action:@selector(nextTrack)];
    commandCenter.nextTrackCommand.enabled = YES;

    [commandCenter.previousTrackCommand addTarget:target action:@selector(previousTrack)];
    commandCenter.previousTrackCommand.enabled = YES;

    [commandCenter.changePlaybackPositionCommand addTarget:target action:@selector(changePlaybackPosition:)];
    commandCenter.changePlaybackPositionCommand.enabled = YES;

    [commandCenter.changeRepeatModeCommand addTarget:target action:@selector(changeRepeatModeCommand:)];
    commandCenter.changeRepeatModeCommand.enabled = YES;

    [commandCenter.changeShuffleModeCommand addTarget:target action:@selector(changeShuffleModeCommand:)];
    commandCenter.changeShuffleModeCommand.enabled = YES;

    [commandCenter.seekForwardCommand addTarget:target action:@selector(seekForwardCommand)];
    commandCenter.seekForwardCommand.enabled = YES;

    [commandCenter.seekBackwardCommand addTarget:target action:@selector(seekBackwardCommand)];
    commandCenter.seekBackwardCommand.enabled = YES;

    [commandCenter.ratingCommand addTarget:target action:@selector(ratingCommand:)];
    commandCenter.ratingCommand.enabled = YES;
}

void NowPlayingPlugin::updateNowPlayingInfo()
{
    const auto& playlistTrack = m_playerController->currentPlaylistTrack();
    if(!playlistTrack.isValid()) {
        [M_NOW_PLAYING_INFO removeAllObjects];
        MPNowPlayingInfoCenter.defaultCenter.nowPlayingInfo = nil;
        return;
    }

    const auto& track = playlistTrack.track;

    M_NOW_PLAYING_INFO[MPMediaItemPropertyTitle] = track.title().toNSString();
    M_NOW_PLAYING_INFO[MPMediaItemPropertyArtist] = track.artists().join(u", "_s).toNSString();
    M_NOW_PLAYING_INFO[MPMediaItemPropertyAlbumTitle] = track.album().toNSString();
    M_NOW_PLAYING_INFO[MPMediaItemPropertyPlaybackDuration] = @(track.duration() / 1000.0);
    M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(m_playerController->currentPosition() / 1000.0);
    M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyPlaybackRate] = @(m_playerController->playState() == Player::PlayState::Playing ? 1.0 : 0.0);
    M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyMediaType] = @(MPNowPlayingInfoMediaTypeAudio);
    M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyPlaybackProgress] = @(m_playerController->currentPosition() / static_cast<double>(track.duration()));
    M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyPlaybackQueueIndex] = @(m_playlistHandler->activePlaylist()->currentTrackIndex());
    M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyPlaybackQueueCount] = @(m_playlistHandler->activePlaylist()->trackCount());

    const QPixmap cover = m_coverProvider->trackCover(track, Track::Cover::Front);
    if(!cover.isNull()) {
        QImage image = cover.toImage().convertToFormat(QImage::Format_ARGB32);
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        on_scope_exit releaseColorSpace([&] { CGColorSpaceRelease(colorSpace); });

        CGContextRef context = CGBitmapContextCreate(
            image.bits(),
            image.width(),
            image.height(),
            8,
            image.bytesPerLine(),
            colorSpace,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
        on_scope_exit releaseContext([&] { CGContextRelease(context); });

        if(context) {
            CGImageRef cgImage = CGBitmapContextCreateImage(context);
            if(cgImage) {
                M_NOW_PLAYING_INFO[MPMediaItemPropertyArtwork] = [MPMediaItemArtwork.alloc initWithBoundsSize:image.size().toCGSize()
                                                                                           requestHandler:^(CGSize size) {
                    auto result = [[NSImage.alloc initWithCGImage:cgImage size:size] autorelease];
                    CGImageRelease(cgImage);
                    return result;
                }];
            }
        }
    }

    MPNowPlayingInfoCenter.defaultCenter.nowPlayingInfo = M_NOW_PLAYING_INFO;
}

void NowPlayingPlugin::trackChanged(const PlaylistTrack& playlistTrack)
{
    if(playlistTrack.isValid()) {
        updateNowPlayingInfo();
    } else {
        [M_NOW_PLAYING_INFO removeAllObjects];
        MPNowPlayingInfoCenter.defaultCenter.nowPlayingInfo = nil;
        MPNowPlayingInfoCenter.defaultCenter.playbackState = MPNowPlayingPlaybackStateStopped;
    }
}

void NowPlayingPlugin::playStateChanged()
{
    MPNowPlayingInfoCenter* center = MPNowPlayingInfoCenter.defaultCenter;
    switch(m_playerController->playState()) {
    case Player::PlayState::Playing:
        center.playbackState = MPNowPlayingPlaybackStatePlaying;
        M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyPlaybackRate] = @(1.0);
        break;
    case Player::PlayState::Paused:
        center.playbackState = MPNowPlayingPlaybackStatePaused;
        M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyPlaybackRate] = @(0.0);
        break;
    case Player::PlayState::Stopped:
        center.playbackState = MPNowPlayingPlaybackStateStopped;
        M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyPlaybackRate] = @(0.0);
        break;
    }
    center.nowPlayingInfo = M_NOW_PLAYING_INFO;
}

void NowPlayingPlugin::positionMoved(uint64_t ms)
{
    M_NOW_PLAYING_INFO[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(ms / 1000.0);
    MPNowPlayingInfoCenter.defaultCenter.nowPlayingInfo = M_NOW_PLAYING_INFO;
}

} // namespace Fooyin::NowPlaying

#include "moc_nowplayingplugin.cpp"
