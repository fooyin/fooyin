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

#include <QObject>

namespace Fooyin {
class ActionManager;
class SettingsManager;

class FileMenu : public QObject
{
    Q_OBJECT

public:
    explicit FileMenu(ActionManager* actionManager, SettingsManager* settings, QObject* parent = nullptr);

signals:
    void requestExit();
    void requestAddFiles();
    void requestAddFolders();
    void requestNewPlaylist();
    void requestNewAutoPlaylist();
    void requestLoadPlaylist();
    void requestSavePlaylist();
    void requestSaveAllPlaylists();

private:
    ActionManager* m_actionManager;
    SettingsManager* m_settings;
};
} // namespace Fooyin
