/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <QDialog>

#include <memory>

class QTabWidget;

namespace Fooyin {
class ActionManager;
class NetworkAccessManager;
class PlayerController;
class PlaylistLoader;
class SettingsManager;
class TrackSelectionController;

namespace RadioBrowser {
class RadioBrowserConnectionManager;
class RadioSearch;
class RadioBrowserController;
class RadioBrowserWidget;
class RadioStationStore;

class RadioBrowserDialog : public QDialog
{
    Q_OBJECT

public:
    RadioBrowserDialog(std::shared_ptr<NetworkAccessManager> network, std::shared_ptr<PlaylistLoader> playlistLoader,
                       SettingsManager* settings, PlayerController* playerController, RadioStationStore* store,
                       ActionManager* actionManager, TrackSelectionController* trackSelection,
                       QWidget* parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

    void done(int value) override;

private:
    void saveState();
    void loadState();

    SettingsManager* m_settings;
    QTabWidget* m_tabs;
    RadioBrowserConnectionManager* m_connectionManager;
    RadioSearch* m_searchFilterBar;
    RadioBrowserController* m_searchController;
    RadioBrowserController* m_savedStationsController;
    RadioBrowserWidget* m_searchWidget;
    RadioBrowserWidget* m_savedStationsWidget;
};
} // namespace RadioBrowser
} // namespace Fooyin
