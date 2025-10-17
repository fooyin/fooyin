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

#include "mprisplayer.h"
#include "mprisroot.h"

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guipaths.h>
#include <gui/windowcontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/crypto.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QDBusObjectPath>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(MPRIS, "fy.mpris")

using namespace Qt::StringLiterals;

constexpr auto MprisObjectPath = "/org/mpris/MediaPlayer2";
constexpr auto ServiceName     = "org.mpris.MediaPlayer2.fooyin";
constexpr auto PlayerEntity    = "org.mpris.MediaPlayer2.Player";
constexpr auto DbusPath        = "org.freedesktop.DBus.Properties";

namespace {
QDBusObjectPath formatTrackId(int index)
{
    const QString trackId = u"/org/fooyin/fooyin/track/%1"_s.arg(index);
    return QDBusObjectPath{trackId};
}

QString formatDateTime(uint64_t time)
{
    if(time <= 0) {
        return {};
    }

    const auto dateTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(time));
    return dateTime.toString(Qt::ISODate);
}
} // namespace

namespace Fooyin::Mpris {
MprisPlugin::MprisPlugin()
    : m_registered{false}
    , m_coverProvider{nullptr}
{ }

void MprisPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_playlistHandler  = context.playlistHandler;
    m_audioLoader      = context.audioLoader;
    m_settings         = context.settingsManager;

    QObject::connect(m_playerController, &PlayerController::playModeChanged, this, [this]() {
        notify(u"LoopStatus"_s, loopStatus());
        notify(u"Shuffle"_s, shuffle());
        notify(u"CanGoNext"_s, canGoNext());
        notify(u"CanGoPrevious"_s, canGoPrevious());
    });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, [this]() {
        notify(u"PlaybackStatus"_s, playbackStatus());
        notify(u"CanPause"_s, canPause());
        notify(u"CanPlay"_s, canPlay());
        notify(u"CanGoNext"_s, canGoNext());
        notify(u"CanGoPrevious"_s, canGoPrevious());
        notify(u"CanSeek"_s, canSeek());
    });
    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, this, &MprisPlugin::trackChanged);
    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     [this](uint64_t ms) {
        QDBusMessage msg = QDBusMessage::createSignal(QString::fromLatin1(MprisObjectPath), QString::fromLatin1(PlayerEntity),
                                                    u"Seeked"_s);
        msg.setArguments({QVariant::fromValue(static_cast<qint64>(ms) * 1000)});
        QDBusConnection::sessionBus().send(msg);
    });

    QObject::connect(this, &MprisPlugin::reloadMetadata, this, [this]() { notify(u"Metadata"_s, metadata()); });
}

void MprisPlugin::initialise(const GuiPluginContext& context)
{
    m_windowController = context.windowController;
    m_coverProvider    = new CoverProvider(m_audioLoader, m_settings, this);

    m_coverProvider->setUsePlaceholder(false);

    QObject::connect(m_windowController, &WindowController::isFullScreenChanged, this,
                     [this]() { notify(u"Fullscreen"_s, fullscreen()); });

    new MprisRoot(this);
    new MprisPlayer(this);

    if(!QDBusConnection::sessionBus().isConnected()) {
        qCWarning(MPRIS) << "Cannot connect to the dbus session bus";
        return;
    }

    if(!QDBusConnection::sessionBus().registerService(QString::fromLatin1(ServiceName))) {
        qCWarning(MPRIS) << "Cannot register with the session dbus";
        return;
    }

    if(!QDBusConnection::sessionBus().registerObject(QString::fromLatin1(MprisObjectPath), this)) {
        qCWarning(MPRIS) << "Cannot register object to the dbus";
        return;
    }

    m_registered = true;
}

void MprisPlugin::shutdown()
{
    QFile::remove(currentCoverPath());

    if(!m_registered) {
        return;
    }

    QDBusConnection::sessionBus().unregisterService(QString::fromLatin1(ServiceName));
}

QString MprisPlugin::identity() const
{
    return QApplication::applicationDisplayName();
}

QString MprisPlugin::desktopEntry() const
{
    return QApplication::desktopFileName();
}

bool MprisPlugin::canRaise() const
{
    return true;
}

bool MprisPlugin::canQuit() const
{
    return true;
}

bool MprisPlugin::canSetFullscreen() const
{
    return true;
}

bool MprisPlugin::fullscreen() const
{
    return m_windowController->isFullScreen();
}

void MprisPlugin::setFullscreen(bool fullscreen)
{
    m_windowController->setFullScreen(fullscreen);
}

QStringList MprisPlugin::supportedUriSchemes() const
{
    return {u"file"_s};
}

QStringList MprisPlugin::supportedMimeTypes() const
{
    return Track::supportedMimeTypes();
}

bool MprisPlugin::canControl() const
{
    return true;
}

bool MprisPlugin::canGoNext() const
{
    return m_playlistHandler->activePlaylist() && m_playlistHandler->nextTrack().isValid();
}

bool MprisPlugin::canGoPrevious() const
{
    return m_playlistHandler->activePlaylist() && m_playlistHandler->previousTrack().isValid();
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

QString MprisPlugin::playbackStatus() const
{
    return Utils::Enum::toString(m_playerController->playState());
}

QString MprisPlugin::loopStatus() const
{
    const auto mode = m_playerController->playMode();

    if(mode & Playlist::RepeatPlaylist) {
        return u"Playlist"_s;
    }

    if(mode & Playlist::RepeatTrack) {
        return u"Track"_s;
    }

    return u"None"_s;
}

void MprisPlugin::setLoopStatus(const QString& status)
{
    auto mode = m_playerController->playMode();

    if(status == "Playlist"_L1) {
        mode |= Playlist::RepeatPlaylist;
        mode &= ~Playlist::RepeatAlbum;
        mode &= ~Playlist::RepeatTrack;
    }
    else if(status == "Track"_L1) {
        mode &= ~Playlist::RepeatPlaylist;
        mode &= ~Playlist::RepeatAlbum;
        mode |= Playlist::RepeatTrack;
    }
    else if(status == "None"_L1) {
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
    return m_currentMetaData;
}

int64_t MprisPlugin::position() const
{
    return static_cast<int64_t>(m_playerController->currentPosition()) * 1000;
}

double MprisPlugin::minimumRate() const
{
    return 1.0;
}

double MprisPlugin::maximumRate() const
{
    return 1.0;
}

void MprisPlugin::Raise()
{
    m_windowController->raise();
}

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
    m_playerController->seek(position / 1000);
}

QString MprisPlugin::currentCoverPath() const
{
    return Fooyin::Gui::coverPath() + m_currCoverKey + u".jpg"_s;
}

void MprisPlugin::notify(const QString& name, const QVariant& value)
{
    QDBusMessage msg = QDBusMessage::createSignal(QString::fromLatin1(MprisObjectPath), QString::fromLatin1(DbusPath),
                                                  u"PropertiesChanged"_s);
    QVariantMap map;
    map.insert(name, value);
    msg.setArguments({QString::fromLatin1(PlayerEntity), map, QStringList{}});
    QDBusConnection::sessionBus().send(msg);
}

void MprisPlugin::trackChanged(const PlaylistTrack& playlistTrack)
{
    QFile::remove(currentCoverPath());
    m_currentMetaData.clear();

    if(playlistTrack.isValid()) {
        loadMetaData(playlistTrack);
        notify(u"CanSeek"_s, canSeek());
        notify(u"CanGoNext"_s, canGoNext());
        notify(u"CanGoPrevious"_s, canGoPrevious());
    }
}

void MprisPlugin::loadMetaData(const PlaylistTrack& playlistTrack)
{
    const auto& [track, _, index] = playlistTrack;

    if(m_currentMetaData.empty()) {
        m_currentMetaData[u"mpris:trackid"_s]     = formatTrackId(std::max(0, index));
        m_currentMetaData[u"mpris:length"_s]      = static_cast<quint64>(track.duration() * 1000);
        m_currentMetaData[u"xesam:url"_s]         = track.filepath();
        m_currentMetaData[u"xesam:title"_s]       = track.title();
        m_currentMetaData[u"xesam:trackNumber"_s] = track.trackNumber();
        m_currentMetaData[u"xesam:album"_s]       = track.album();
        m_currentMetaData[u"xesam:albumArtist"_s] = track.albumArtist();
        m_currentMetaData[u"xesam:artist"_s]      = track.artists();
        m_currentMetaData[u"xesam:genre"_s]       = track.genres();
        m_currentMetaData[u"xesam:discNumber"_s]  = track.discNumber();
        m_currentMetaData[u"xesam:comment"_s]     = track.comment();
        m_currentMetaData[u"xesam:composer"_s]    = track.composer();
        m_currentMetaData[u"xesam:firstUsed"_s]   = formatDateTime(track.firstPlayed());
        m_currentMetaData[u"xesam:lastUsed"_s]    = formatDateTime(track.lastPlayed());
        m_currentMetaData[u"xesam:useCount"_s]    = track.playCount();
    }

    if(!m_coverProvider) {
        return;
    }

    const auto loadArtAndReload = [this]() {
        m_currentMetaData[u"mpris:artUrl"_s] = QUrl::fromLocalFile(currentCoverPath()).toString();
        qCDebug(MPRIS) << "Sending MPRIS data:" << m_currentMetaData;
        emit reloadMetadata();
    };

    const QString coverKey = Utils::generateHash(u"MPRIS"_s, track.hash());
    if(m_currCoverKey != coverKey) {
        QFile::remove(currentCoverPath());

        m_coverProvider->trackCoverFull(track, Track::Cover::Front)
            .then([this, coverKey, track, loadArtAndReload](const QPixmap& cover) {
                if(!cover.isNull()) {
                    m_currCoverKey = coverKey;
                    QFile file{currentCoverPath()};
                    if(file.open(QIODevice::WriteOnly)) {
                        if(!cover.save(&file, "JPG", 85)) {
                            qCInfo(MPRIS) << "Failed to save MPRIS cover for track:" << track.filepath();
                        }
                    }
                }
                loadArtAndReload();
            });
    }
    else {
        loadArtAndReload();
    }
}
} // namespace Fooyin::Mpris

#include "moc_mprisplugin.cpp"
