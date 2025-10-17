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

#pragma once

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/plugins/guiplugin.h>

class QDBusObjectPath;

namespace Fooyin {
struct PlaylistTrack;

namespace Mpris {
class MprisPlugin : public QObject,
                    public Plugin,
                    public CorePlugin,
                    public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "mpris.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

    // Root
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool Fullscreen READ fullscreen WRITE setFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool CanSetFullscreen READ canSetFullscreen CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

    // Player
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious CONSTANT)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanPlay READ canPlay CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek CONSTANT)
    Q_PROPERTY(double Volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus CONSTANT)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus NOTIFY loopStatusChanged)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(QVariantMap Metadata READ metadata CONSTANT)
    Q_PROPERTY(int64_t Position READ position CONSTANT)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)

public:
    MprisPlugin();

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

    void shutdown() override;

    // Root
    QString identity() const;
    QString desktopEntry() const;
    bool canRaise() const;
    bool canQuit() const;
    bool canSetFullscreen() const;
    bool fullscreen() const;
    void setFullscreen(bool fullscreen);
    QStringList supportedUriSchemes() const;
    QStringList supportedMimeTypes() const;

    // Player
    bool canControl() const;
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPause() const;
    bool canPlay() const;
    bool canSeek() const;
    double volume() const;
    void setVolume(double volume);
    QString playbackStatus() const;
    QString loopStatus() const;
    void setLoopStatus(const QString& status);
    bool shuffle() const;
    void setShuffle(bool value);
    QVariantMap metadata() const;
    int64_t position() const;
    double minimumRate() const;
    double maximumRate() const;

    // Root
    void Raise();
    void Quit();

    // Player
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(int64_t offset);
    void SetPosition(const QDBusObjectPath& path, int64_t position);

signals:
    void fullscreenChanged(bool fullscreen);
    void volumeChanged(double volume);
    void loopStatusChanged(const QString& status);
    void shuffleChanged(bool shuffle);

    void reloadMetadata();

private:
    QString currentCoverPath() const;
    void notify(const QString& name, const QVariant& value);
    void trackChanged(const PlaylistTrack& playlistTrack);
    void loadMetaData(const PlaylistTrack& playlistTrack);

    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    std::shared_ptr<AudioLoader> m_audioLoader;
    WindowController* m_windowController;
    SettingsManager* m_settings;

    bool m_registered;
    QString m_currCoverKey;
    QVariantMap m_currentMetaData;
    CoverProvider* m_coverProvider;
};
} // namespace Mpris
} // namespace Fooyin
