/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "mprisplugin.h"

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/settings/settingsmanager.h>

#include "core/playlist/playlisthandler.h"
#include "mprisplayer.h"
#include "mprisroot.h"

#include <QApplication>
#include <QDBusObjectPath>

constexpr auto MprisObjectPath = "/org/mpris/MediaPlayer2";
constexpr auto ServiceName     = "org.mpris.MediaPlayer2.fooyin";
constexpr auto PlayerEntity    = "org.mpris.MediaPlayer2.Player";
constexpr auto DbusPath        = "org.freedesktop.DBus.Properties";

namespace {
QDBusObjectPath formatTrackId(int index)
{
    const QString trackId = QString{QStringLiteral("/org/fooyin/fooyin/track/%1").arg(index)};
    return QDBusObjectPath{trackId};
}

QString formatDateTime(uint64_t time)
{
    if(time <= 0) {
        return {};
    }

    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(time)).toString(Qt::ISODate);
}
} // namespace

namespace Fooyin::Mpris {
MprisPlugin::MprisPlugin()
    : m_registered{false}
{ }

void MprisPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_playlistHandler  = context.playlistHandler;
    m_settings         = context.settingsManager;

    QObject::connect(m_playerController, &PlayerController::playModeChanged, this, [this]() {
        notify(QStringLiteral("LoopStatus"), loopStatus());
        notify(QStringLiteral("Shuffle"), shuffle());
    });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, [this]() {
        notify(QStringLiteral("PlaybackStatus"), playbackStatus());
        notify(QStringLiteral("CanPause"), canPause());
        notify(QStringLiteral("CanPlay"), canPlay());
        notify(QStringLiteral("CanGoNext"), canPlay());
        notify(QStringLiteral("CanGoPrevious"), canGoPrevious());
        notify(QStringLiteral("CanSeek"), canSeek());
    });
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, [this]() {
        notify(QStringLiteral("Metadata"), metadata());
        notify(QStringLiteral("CanSeek"), canSeek());
    });
    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     [this](uint64_t ms) { emit Seeked(static_cast<int64_t>(ms) * 1000); });
}

void MprisPlugin::initialise(const GuiPluginContext& /*context*/)
{
    new MprisRoot(this);
    new MprisPlayer(this);

    if(!QDBusConnection::sessionBus().isConnected()) {
        qWarning() << "Cannot connect to the dbus session bus";
        return;
    }

    if(!QDBusConnection::sessionBus().registerService(QString::fromLatin1(ServiceName))) {
        qWarning() << "Cannot register with the session dbus";
        return;
    }

    if(!QDBusConnection::sessionBus().registerObject(QString::fromLatin1(MprisObjectPath), this)) {
        qWarning() << "Cannot register object to the dbus";
        return;
    }

    m_registered = true;
}

void MprisPlugin::shutdown()
{
    if(!m_registered) {
        return;
    }

    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(ServiceName));
}

QString MprisPlugin::identity() const
{
    return qApp->applicationDisplayName();
}

QString MprisPlugin::desktopEntry() const
{
    return qApp->desktopFileName();
}

bool MprisPlugin::canRaise() const
{
    return true;
}

bool MprisPlugin::canControl() const
{
    return true;
}

bool MprisPlugin::canGoNext() const
{
    return m_playlistHandler->activePlaylist();
}

bool MprisPlugin::canGoPrevious() const
{
    return m_playlistHandler->activePlaylist();
}

bool MprisPlugin::canPause() const
{
    return m_playerController->currentTrack().isValid();
}

bool MprisPlugin::canPlay() const
{
    return m_playlistHandler->activePlaylist() || !m_playerController->playbackQueue().empty();
}

bool MprisPlugin::canSeek() const
{
    // TODO: Use engine state to detemrine if track is seekable
    return m_playerController->currentTrack().isValid();
}

double MprisPlugin::volume() const
{
    return m_settings->value<Settings::Core::OutputVolume>();
}

void MprisPlugin::setVolume(double volume)
{
    volume = std::clamp(volume, 0.0, 1.0);

    m_settings->set<Settings::Core::OutputVolume>(volume);
}

void MprisPlugin::Raise() { }

void MprisPlugin::Quit()
{
    QCoreApplication::quit();
}

void MprisPlugin::Next()
{
    m_playerController->next();
}

void MprisPlugin::Previous()
{
    m_playerController->previous();
}

void MprisPlugin::Pause()
{
    m_playerController->pause();
}

void MprisPlugin::PlayPause()
{
    m_playerController->playPause();
}

void MprisPlugin::Stop()
{
    m_playerController->stop();
}

void MprisPlugin::Play()
{
    m_playerController->play();
}

void MprisPlugin::Seek(int64_t offset)
{
    const int64_t newPosition = position() + offset;
    SetPosition({}, newPosition);
}

void MprisPlugin::SetPosition(const QDBusObjectPath& /*path*/, int64_t position)
{
    m_playerController->changePosition(position / 1000);
}

void MprisPlugin::notify(const QString& name, const QVariant& value)
{
    QDBusMessage msg = QDBusMessage::createSignal(QString::fromLatin1(MprisObjectPath), QString::fromLatin1(DbusPath),
                                                  QStringLiteral("PropertiesChanged"));
    QVariantMap map;
    map.insert(name, value);
    msg.setArguments({QString::fromLatin1(PlayerEntity), map, QStringList{}});
    QDBusConnection::sessionBus().send(msg);
}

QString MprisPlugin::playbackStatus() const
{
    switch(m_playerController->playState()) {
        case(PlayState::Playing):
            return QStringLiteral("Playing");
        case(PlayState::Paused):
            return QStringLiteral("Paused");
        case(PlayState::Stopped):
            return QStringLiteral("Stopped");
        default:
            return {};
    }
}

QString MprisPlugin::loopStatus() const
{
    const auto mode = m_playerController->playMode();

    if(mode & Playlist::RepeatPlaylist) {
        return QStringLiteral("Playlist");
    }

    if(mode & Playlist::RepeatTrack) {
        return QStringLiteral("Track");
    }

    return QStringLiteral("None");
}

void MprisPlugin::setLoopStatus(const QString& status)
{
    auto mode = m_playerController->playMode();

    if(status == u"Playlist") {
        mode |= Playlist::RepeatPlaylist;
        mode &= ~Playlist::RepeatAlbum;
        mode &= ~Playlist::RepeatTrack;
    }
    else if(status == u"Track") {
        mode &= ~Playlist::RepeatPlaylist;
        mode &= ~Playlist::RepeatAlbum;
        mode |= Playlist::RepeatTrack;
    }
    else if(status == u"None") {
        mode &= ~Playlist::RepeatPlaylist;
        mode &= ~Playlist::RepeatAlbum;
        mode &= ~Playlist::RepeatTrack;
    }

    m_playerController->setPlayMode(mode);
}

bool MprisPlugin::shuffle() const
{
    const auto mode = m_playerController->playMode();
    return mode & Playlist::ShuffleAlbums || mode & Playlist::ShuffleTracks;
}

void MprisPlugin::setShuffle(bool value)
{
    auto mode = m_playerController->playMode();

    if(value) {
        mode |= Playlist::ShuffleTracks;
    }
    else {
        mode &= ~Playlist::ShuffleAlbums;
        mode &= ~Playlist::ShuffleTracks;
    }

    m_playerController->setPlayMode(mode);
}

QVariantMap MprisPlugin::metadata() const
{
    const auto [track, _, index] = m_playerController->currentPlaylistTrack();

    if(!track.isValid()) {
        return {};
    }

    QVariantMap metadata;

    metadata[QStringLiteral("mpris:trackid")]     = formatTrackId(index);
    metadata[QStringLiteral("mpris:length")]      = static_cast<quint64>(track.duration() * 1000);
    metadata[QStringLiteral("xesam:url")]         = track.filepath();
    metadata[QStringLiteral("xesam:title")]       = track.title();
    metadata[QStringLiteral("xesam:trackNumber")] = track.trackNumber();
    metadata[QStringLiteral("xesam:album")]       = track.album();
    metadata[QStringLiteral("xesam:albumArtist")] = track.albumArtist();
    metadata[QStringLiteral("xesam:artist")]      = track.artists();
    metadata[QStringLiteral("xesam:genre")]       = track.genres();
    metadata[QStringLiteral("xesam:discNumber")]  = track.discNumber();
    metadata[QStringLiteral("xesam:comment")]     = track.comment();
    metadata[QStringLiteral("xesam:composer")]    = track.composer();
    metadata[QStringLiteral("xesam:firstUsed")]   = formatDateTime(track.firstPlayed());
    metadata[QStringLiteral("xesam:lastUsed")]    = formatDateTime(track.lastPlayed());
    metadata[QStringLiteral("xesam:useCount")]    = track.playCount();

    // TODO: Support embedded covers - will need to read and save to temp location
    if(track.hasCover() && !track.hasEmbeddedCover()) {
        metadata[QStringLiteral("xesam:artUrl")] = track.coverPath();
    }

    return metadata;
}

int64_t MprisPlugin::position() const
{
    return static_cast<int64_t>(m_playerController->currentPosition()) * 1000;
}
} // namespace Fooyin::Mpris

#include "moc_mprisplugin.cpp"
