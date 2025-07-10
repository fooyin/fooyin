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

#include "mediacontrolplugin.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/windowcontroller.h>

#include <QBuffer>
#include <QLoggingCategory>
#include <QPixmap>
#include <QMainWindow>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace winrt::Windows::Media;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

Q_LOGGING_CATEGORY(MEDIA_CONTROL, "fy.mediacontrol")

namespace Fooyin::MediaControl {

MediaControlPlugin::MediaControlPlugin() = default;

MediaControlPlugin::~MediaControlPlugin() = default;

void MediaControlPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_playlistHandler  = context.playlistHandler;
    m_audioLoader      = context.audioLoader;
    m_settings         = context.settingsManager;

    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this,
                     &MediaControlPlugin::trackChanged);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     &MediaControlPlugin::playStateChanged);
}

void MediaControlPlugin::initialise(const GuiPluginContext& context)
{
    m_coverProvider = new CoverProvider(m_audioLoader, m_settings, this);
    m_coverProvider->setUsePlaceholder(false);

    HWND hWnd = reinterpret_cast<HWND>(context.windowController->mainWindow()->winId());

    auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
    winrt::check_hresult(interop->GetForWindow(hWnd, winrt::guid_of<ISystemMediaTransportControls>(), winrt::put_abi(m_smtc)));
    m_smtc.IsPlayEnabled(true);
    m_smtc.IsPauseEnabled(true);
    m_smtc.IsNextEnabled(true);
    m_smtc.IsPreviousEnabled(true);
    m_smtc.IsStopEnabled(true);
    m_smtc.IsRewindEnabled(true);

    m_buttonPressedToken = m_smtc.ButtonPressed({this, &MediaControlPlugin::buttonPressed});
}

void MediaControlPlugin::shutdown()
{
    if(m_smtc) {
        m_smtc.ButtonPressed(m_buttonPressedToken);
        m_smtc.DisplayUpdater().ClearAll();
        m_smtc = nullptr;
    }
}

void MediaControlPlugin::trackChanged(const PlaylistTrack& /*playlistTrack*/)
{
    if(!m_smtc) {
        return;
    }

    m_smtc.IsNextEnabled(m_playlistHandler->nextTrack().isValid());
    m_smtc.IsPreviousEnabled(m_playlistHandler->previousTrack().isValid());

    updateDisplay();
}

void MediaControlPlugin::playStateChanged()
{
    if(!m_smtc) {
        return;
    }

    switch(m_playerController->playState()) {
    case Player::PlayState::Playing:
        m_smtc.PlaybackStatus(MediaPlaybackStatus::Playing);
        break;
    case Player::PlayState::Paused:
        m_smtc.PlaybackStatus(MediaPlaybackStatus::Paused);
        break;
    case Player::PlayState::Stopped:
        m_smtc.PlaybackStatus(MediaPlaybackStatus::Stopped);
        break;
    }
}

void MediaControlPlugin::updateDisplay()
{
    if(!m_smtc) {
        return;
    }

    auto updater = m_smtc.DisplayUpdater();
    const auto playlistTrack = m_playerController->currentPlaylistTrack();

    if(!playlistTrack.isValid()) {
        updater.ClearAll();
        return;
    }

    const auto& track = playlistTrack.track;

    updater.Type(MediaPlaybackType::Music);
    updater.MusicProperties().Title(track.title().toStdWString());
    updater.MusicProperties().Artist(track.artist().toStdWString());
    updater.MusicProperties().AlbumArtist(track.albumArtist().toStdWString());
    updater.MusicProperties().AlbumTitle(track.album().toStdWString());
    updater.MusicProperties().TrackNumber(track.trackNumber().toInt());
    auto genres = updater.MusicProperties().Genres();
    genres.Clear();
    for(const auto& genre : track.genres()) {
        genres.Append(genre.toStdWString());
    }

    const QPixmap cover = m_coverProvider->trackCover(track, Track::Cover::Front);
    if(!cover.isNull()) {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        cover.save(&buffer, "PNG");

        InMemoryRandomAccessStream stream;
        DataWriter writer(stream.GetOutputStreamAt(0));
        writer.WriteBytes({reinterpret_cast<uint8_t*>(ba.data()), static_cast<uint32_t>(ba.size())});
        writer.StoreAsync().get();

        updater.Thumbnail(RandomAccessStreamReference::CreateFromStream(stream));
    } else {
        updater.Thumbnail(nullptr);
    }

    updater.Update();
}

void MediaControlPlugin::buttonPressed(const ISystemMediaTransportControls& /*sender*/,
                                       const SystemMediaTransportControlsButtonPressedEventArgs& args)
{
    switch(args.Button()) {
    case SystemMediaTransportControlsButton::Play:
        m_playerController->play();
        break;
    case SystemMediaTransportControlsButton::Pause:
        m_playerController->pause();
        break;
    case SystemMediaTransportControlsButton::Stop:
        m_playerController->stop();
        break;
    case SystemMediaTransportControlsButton::Next:
        m_playerController->next();
        break;
    case SystemMediaTransportControlsButton::Previous:
        m_playerController->previous();
        break;
    case SystemMediaTransportControlsButton::Rewind:
        m_playerController->setCurrentPosition(0);
        break;
    default:
        break;
    }
}

} // namespace Fooyin::MediaControl

#include "moc_mediacontrolplugin.cpp"
