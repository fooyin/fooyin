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

#include <QDialog>
#include <QModelIndexList>

class QLineEdit;

namespace Fooyin {
class ActionManager;
class Application;
class CoverProvider;
class PlaylistInteractor;
class PlaylistWidget;
class SettingsManager;

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    SearchDialog(ActionManager* actionManager, PlaylistInteractor* playlistInteractor, CoverProvider* coverProvider,
                 Application* core, QWidget* parent = nullptr);
    ~SearchDialog() override;

    [[nodiscard]] QSize sizeHint() const override;

private:
    void search();
    void updateTitle();
    void showOptionsMenu();
    void selectInPlaylist(const QModelIndexList& indexes);

    void saveState();
    void loadState();

    PlaylistInteractor* m_playlistInteractor;
    SettingsManager* m_settings;
    QLineEdit* m_searchBar;
    PlaylistWidget* m_view;
    bool m_autoSelect;
};
} // namespace Fooyin
