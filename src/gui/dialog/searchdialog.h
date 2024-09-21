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

#include "playlist/playlistwidget.h"

#include <QDialog>
#include <QModelIndexList>

class QLineEdit;
class QItemSelection;

namespace Fooyin {
class ActionManager;
class Application;
class CoverProvider;
class PlaylistInteractor;
class SettingsManager;

class SearchDialog : public QDialog
{
    Q_OBJECT

public:
    SearchDialog(ActionManager* actionManager, PlaylistInteractor* playlistInteractor, CoverProvider* coverProvider,
                 Application* core, PlaylistWidget::Mode mode, QWidget* parent = nullptr);
    ~SearchDialog() override;

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void search();
    void updateTitle();
    void showOptionsMenu();
    void selectInPlaylist();

    void saveState();
    void loadState();

    PlaylistWidget::Mode m_mode;
    PlaylistInteractor* m_playlistInteractor;
    SettingsManager* m_settings;

    QLineEdit* m_searchBar;
    PlaylistWidget* m_view;
    bool m_autoSelect;
};
} // namespace Fooyin
