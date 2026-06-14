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

#include <QObject>

#include <cstdint>
#include <vector>

class QLabel;
class QLineEdit;
class QHeaderView;
class QWidget;

namespace Fooyin {
class PlaylistController;
class PlaylistModel;
class PlaylistView;
class SettingsManager;
class ToolButton;

class PlaylistSearchController : public QObject
{
    Q_OBJECT

public:
    enum class Mode : uint8_t
    {
        MatchWordBeginnings = 0,
        MatchAnywhere,
    };

    PlaylistSearchController(PlaylistController* playlistController, PlaylistModel* model, PlaylistView* view,
                             QHeaderView* header, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QWidget* widget() const;
    [[nodiscard]] bool isOpen() const;

    bool open();
    void close();
    void navigate(int delta);

    void refreshMatches();
    void refreshMatchesIfOpen();
    void updateEnabledState();
    void markResultsOutOfDate();

    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void playCurrentRequested();
    void queueCurrentRequested();

private:
    [[nodiscard]] bool hasCurrentMatch() const;
    void selectMatch(int matchIndex);
    void updateMatchLabel() const;

    PlaylistController* m_playlistController;
    PlaylistModel* m_model;
    PlaylistView* m_view;
    QHeaderView* m_header;
    SettingsManager* m_settings;

    QWidget* m_widget;
    QLineEdit* m_search;
    ToolButton* m_previous;
    ToolButton* m_next;
    QLabel* m_matchesLabel;
    ToolButton* m_close;

    std::vector<int> m_matches;
    int m_matchIndex;
    bool m_resultsOutOfDate;
    uint64_t m_searchRequestToken;
};
} // namespace Fooyin
