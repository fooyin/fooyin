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

#include <gui/fywidget.h>

class QLineEdit;

namespace Fooyin {
class MusicLibrary;
class PlaylistController;
class SettingsManager;
class SearchController;

class SearchWidget : public FyWidget
{
    Q_OBJECT

public:
    enum class SearchMode : uint8_t
    {
        Library = 0,
        Playlist, // Not implemented
        PlaylistInline,
        AllPlaylists, // Not implemented
    };

    SearchWidget(SearchController* controller, PlaylistController* playlistController, MusicLibrary* library,
                 SettingsManager* settings, QWidget* parent = nullptr);
    ~SearchWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    void layoutEditingMenu(QMenu* menu) override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateConnectedState();
    void searchChanged();
    void changePlaceholderText();
    void showOptionsMenu();

    SearchController* m_searchController;
    PlaylistController* m_playlistController;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    QLineEdit* m_searchBox;
    QString m_defaultPlaceholder;
    SearchMode m_mode;
    bool m_unconnected;
    bool m_autoSearch;
};
} // namespace Fooyin
