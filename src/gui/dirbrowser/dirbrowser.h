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

#include <core/player/playerdefs.h>
#include <gui/fywidget.h>

namespace Fooyin {
class ActionManager;
class DirBrowserPrivate;
class PlaylistInteractor;
class Playlist;
struct PlaylistTrack;
class SettingsManager;

class DirBrowser : public FyWidget
{
    Q_OBJECT

public:
    enum class Mode : int
    {
        Tree,
        List,
    };

    DirBrowser(const QStringList& supportedExtensions, ActionManager* actionManager,
               PlaylistInteractor* playlistInteractor, SettingsManager* settings, QWidget* parent = nullptr);
    ~DirBrowser() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void updateDir(const QString& dir);

signals:
    void rootChanged();

public slots:
    void playstateChanged(Player::PlayState state);
    void activePlaylistChanged(Fooyin::Playlist* playlist);
    void playlistTrackChanged(const Fooyin::PlaylistTrack& track);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    std::unique_ptr<DirBrowserPrivate> p;
};
} // namespace Fooyin
