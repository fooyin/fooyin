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

#include <utils/settings/settingspage.h>

#include <QTableWidget>

namespace Utils {
class SettingsDialogController;
}

namespace Core {
class SettingsManager;

namespace Library {
class LibraryManager;
class LibraryInfo;
} // namespace Library
} // namespace Core

namespace Gui::Settings {
class LibraryGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibraryGeneralPageWidget(Core::Library::LibraryManager* libraryManager, Core::SettingsManager* settings);
    ~LibraryGeneralPageWidget() override = default;

    void apply() override;

private:
    void addLibraryRow(const Core::Library::LibraryInfo& info);
    void addLibrary();
    void removeLibrary();

    Core::Library::LibraryManager* m_libraryManager;
    Core::SettingsManager* m_settings;
    QTableWidget m_libraryList;
};

class LibraryGeneralPage : public Utils::SettingsPage
{
public:
    LibraryGeneralPage(Utils::SettingsDialogController* controller, Core::Library::LibraryManager* libraryManager,
                       Core::SettingsManager* settings);
};
} // namespace Gui::Settings
