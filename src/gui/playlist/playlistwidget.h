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

#include <core/typedefs.h>

#include <QItemSelection>

class QHBoxLayout;
class QPushButton;

namespace Core {
class SettingsManager;

namespace Player {
class PlayerManager;
} // namespace Player

namespace Library {
class LibraryManager;
class MusicLibrary;
} // namespace Library
} // namespace Core

namespace Gui {

namespace Settings {
class SettingsDialog;
}
namespace Widgets {
class OverlayWidget;
class PlaylistModel;
class PlaylistView;

class PlaylistWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(Core::Library::LibraryManager* libraryManager, Core::Library::MusicLibrary* library,
                            Core::Player::PlayerManager* playerManager, Settings::SettingsDialog* settingsDialog,
                            Core::SettingsManager* settings, QWidget* parent = nullptr);
    ~PlaylistWidget() override = default;

    void setup();
    void reset();

    void setupConnections();

    void setAltRowColours(bool altColours);

    bool isHeaderHidden();
    void setHeaderHidden(bool showHeader);

    bool isScrollbarHidden();
    void setScrollbarHidden(bool showScrollBar);

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(Core::ActionContainer* menu) override;

signals:
    void clickedTrack(int idx, bool createNewPlaylist);

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
    Core::Library::LibraryManager* m_libraryManager;
    Core::Library::MusicLibrary* m_library;
    Core::Player::PlayerManager* m_playerManager;
    Settings::SettingsDialog* m_settingsDialog;
    Core::SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    PlaylistModel* m_model;
    PlaylistView* m_playlist;
    bool m_altRowColours;
    OverlayWidget* m_noLibrary;
};
} // namespace Widgets
} // namespace Gui
