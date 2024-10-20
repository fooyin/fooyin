/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/playlist/playlist.h>
#include <core/track.h>
#include <gui/fywidget.h>

#include <QBasicTimer>

class QLineEdit;

namespace Fooyin {
class MusicLibrary;
class Playlist;
class PlaylistController;
class PlaylistHandler;
class SettingsManager;
class SearchController;

class SearchWidget : public FyWidget
{
    Q_OBJECT

public:
    enum class SearchMode : uint8_t
    {
        Library = 0,
        Playlist,
        PlaylistFilter,
        AllPlaylists,
    };

    struct SearchColours
    {
        std::optional<QColor> failBg;
        std::optional<QColor> failFg;
    };

    SearchWidget(SearchController* controller, PlaylistController* playlistController, MusicLibrary* library,
                 SettingsManager* settings, QWidget* parent = nullptr);
    ~SearchWidget() override;

    static QString defaultPlaylistName();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void layoutEditingMenu(QMenu* menu) override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    [[nodiscard]] bool isQuickSearch() const;
    Playlist* findOrAddPlaylist(const TrackList& tracks);
    [[nodiscard]] PlaylistTrackList getTracksToSearch(SearchMode mode) const;
    void deleteWord();

    bool handleFilteredTracks(SearchMode mode, const PlaylistTrackList& playlistTracks);
    void handleSearchFailed();

    void loadColours();
    void resetColours();

    void updateConnectedState();
    void searchChanged(bool enterKey = false);
    void changePlaceholderText();
    void showOptionsMenu();

    SearchController* m_searchController;
    PlaylistController* m_playlistController;
    PlaylistHandler* m_playlistHandler;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    QBasicTimer m_searchTimer;
    QLineEdit* m_searchBox;
    QString m_defaultPlaceholder;
    SearchMode m_mode;
    std::optional<SearchMode> m_forceMode;
    bool m_forceNewPlaylist;
    bool m_unconnected;
    bool m_exclusivePlaylist;
    bool m_autoSearch;
    SearchColours m_colours;
};
} // namespace Fooyin
