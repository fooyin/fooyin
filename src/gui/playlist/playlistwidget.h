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

#pragma once

#include "gui/fywidget.h"

#include <core/models/trackfwd.h>

class QHBoxLayout;

namespace Fy {

namespace Utils {
class OverlayWidget;
class SettingsDialogController;
class SettingsManager;
class HeaderView;
} // namespace Utils

namespace Core {
namespace Player {
class PlayerManager;
enum PlayState : uint8_t;
} // namespace Player

namespace Library {
class LibraryManager;
class MusicLibrary;
} // namespace Library

namespace Playlist {
class PlaylistHandler;
} // namespace Playlist
} // namespace Core

namespace Gui::Widgets {
class PlaylistModel;
class PlaylistView;

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
                            Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                            QWidget* parent = nullptr);

    void setup();
    void reset();

    void setupConnections();

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    [[nodiscard]] QString name() const override;

signals:
    void selectionWasChanged(const Core::TrackList& tracks);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void selectionChanged();
    void customHeaderMenuRequested(QPoint pos);
    void changeState(Core::Player::PlayState state);
    void playTrack(const QModelIndex& index);
    void findCurrent();
    void prepareTracks(int idx);
    void expandPlaylist(const QModelIndex& parent, int first, int last);
    void clearSelection(bool clearView = false);
    void switchContextMenu(int section, QPoint pos);

    Core::Library::MusicLibrary* m_library;
    Core::Player::PlayerManager* m_playerManager;
    Utils::SettingsManager* m_settings;
    Utils::SettingsDialogController* m_settingsDialog;

    Core::Playlist::PlaylistHandler* m_playlistHandler;

    Core::TrackList m_selectedTracks;

    QHBoxLayout* m_layout;
    PlaylistModel* m_model;
    PlaylistView* m_playlist;
    Utils::HeaderView* m_header;
    bool m_changingSelection;
};
} // namespace Gui::Widgets
} // namespace Fy
