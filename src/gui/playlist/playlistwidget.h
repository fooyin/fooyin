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
#include <core/playlist/libraryplaylistinterface.h>

class QHBoxLayout;

namespace Fy {

namespace Utils {
class OverlayWidget;
class SettingsDialogController;
class SettingsManager;
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
class PlaylistManager;
} // namespace Playlist
} // namespace Core

namespace Gui::Widgets {
class PlaylistModel;
class PlaylistView;

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(Core::Library::LibraryManager* libraryManager,
                            Core::Playlist::PlaylistManager* playlistHandler,
                            Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                            QWidget* parent = nullptr);

    void setup();
    void reset();

    void setupConnections();

    void setAltRowColours(bool altColours);

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(Utils::ActionContainer* menu) override;

signals:
    void clickedTrack(int idx);
    void selectionWasChanged(const Core::TrackPtrList& tracks);

protected:
    void selectionChanged();
    void keyPressEvent(QKeyEvent* event) override;
    void customHeaderMenuRequested(QPoint pos);
    void changeOrder(QAction* action);
    void switchOrder();
    void changeState(Core::Player::PlayState state);
    void playTrack(const QModelIndex& index);
    void nextTrack();
    void findCurrent();

private:
    void prepareTracks(int idx);
    void expandPlaylist(const QModelIndex& parent, int first, int last);

    Core::Library::LibraryManager* m_libraryManager;
    Core::Library::MusicLibrary* m_library;
    Core::Player::PlayerManager* m_playerManager;
    Utils::SettingsManager* m_settings;
    Utils::SettingsDialogController* m_settingsDialog;

    Core::Playlist::PlaylistManager* m_playlistHandler;
    std::unique_ptr<Core::Playlist::LibraryPlaylistInterface> m_libraryPlaylistManager;

    Core::TrackPtrList m_selectedTracks;

    QHBoxLayout* m_layout;
    PlaylistModel* m_model;
    PlaylistView* m_playlist;
    bool m_altRowColours;
    bool m_changingSelection;
    Utils::OverlayWidget* m_noLibrary;
};
} // namespace Gui::Widgets
} // namespace Fy
