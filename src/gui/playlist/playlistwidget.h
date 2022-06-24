/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "gui/playlist/playlistmodel.h"
#include "gui/playlist/playlistview.h"
#include "gui/widgets/widget.h"
#include "utils/typedefs.h"

#include <QItemSelection>

class PlayerManager;
class QHBoxLayout;
class Settings;

namespace Library {
class LibraryManager;

class PlaylistWidget : public Widget
{
    Q_OBJECT

public:
    PlaylistWidget(PlayerManager* playerManager, LibraryManager* library, QWidget* parent = nullptr);
    ~PlaylistWidget() override;

    void reset();

    void setupConnections();

    bool isHeaderHidden();
    void setHeaderHidden(bool b);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool b);

    void layoutEditingMenu(QMenu* menu) override;

signals:
    void clickedTrack(int idx, bool createNewPlaylist);

protected:
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void keyPressEvent(QKeyEvent* event) override;
    void customHeaderMenuRequested(QPoint pos);
    void changeOrder(QAction* action);
    void switchOrder();
    void changeState(Player::PlayState state);
    void playTrack(const QModelIndex& index);
    void nextTrack();
    void findCurrent();

private:
    QHBoxLayout* m_layout;
    LibraryManager* m_libraryManager;
    MusicLibrary* m_library;
    PlayerManager* m_playerManager;
    PlaylistModel m_model;
    PlaylistView m_playlist;
    Settings* m_settings;
    bool m_altRowColours;
};
}; // namespace Library
