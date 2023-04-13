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

#include <QActionGroup>
#include <QObject>

namespace Fy {

namespace Utils {
class ActionManager;
class ActionContainer;
class SettingsManager;
} // namespace Utils

namespace Core::Library {
class MusicLibrary;
} // namespace Core::Library

namespace Gui {
class LibraryMenu : public QObject
{
    Q_OBJECT

public:
    LibraryMenu(Utils::ActionManager* actionManager, Core::Library::MusicLibrary* library,
                Utils::SettingsManager* settings, QObject* parent = nullptr);

private:
    void setupSwitchMenu();

    Utils::ActionManager* m_actionManager;
    Core::Library::MusicLibrary* m_library;
    Utils::SettingsManager* m_settings;

    QAction* m_openSettings;
    QAction* m_rescanLibrary;
};
} // namespace Gui
} // namespace Fy
