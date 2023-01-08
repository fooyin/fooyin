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

#include "core/gui/fywidget.h"
#include "core/typedefs.h"

#include <QItemSelection>

class QHBoxLayout;
class QPushButton;

namespace Core {
class Settings;

namespace Player {
class PlayerManager;
}; // namespace Player

namespace Library {
class LibraryManager;
class MusicLibrary;
}; // namespace Library

namespace Widgets {
class OverlayWidget;
class PlaylistModel;
class PlaylistView;

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(QWidget* parent = nullptr);
    ~PlaylistWidget() override;

    void setup();
    void reset();

    void setupConnections();

    void setAltRowColours(bool altColours);

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(ActionContainer* menu) override;

signals:
    void clickedTrack(int idx, bool createNewPlaylist);

protected:
    void selectionChanged();
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
    Library::LibraryManager* m_libraryManager;
    Library::MusicLibrary* m_library;
    Player::PlayerManager* m_playerManager;
    PlaylistModel* m_model;
    PlaylistView* m_playlist;
    Settings* m_settings;
    bool m_altRowColours;
    OverlayWidget* m_noLibrary;
};
}; // namespace Widgets
}; // namespace Core
