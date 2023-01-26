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

#include <QComboBox>
#include <QTableWidget>
#include <QWidget>

namespace Core {
class SettingsManager;

namespace Library {
class LibraryManager;
class LibraryInfo;
} // namespace Library
} // namespace Core

namespace Gui::Settings {
class GeneralPage : public QWidget
{
public:
    explicit GeneralPage(Core::SettingsManager* settings, QWidget* parent = nullptr);
    ~GeneralPage() override;

private:
    Core::SettingsManager* m_settings;
    QComboBox m_deviceList;
};

class LibraryPage : public QWidget
{
public:
    explicit LibraryPage(Core::Library::LibraryManager* libraryManager, Core::SettingsManager* settings,
                         QWidget* parent = nullptr);
    ~LibraryPage() override;

private:
    void addLibraryRow(const Core::Library::LibraryInfo& info);
    void addLibrary();
    void removeLibrary();

    Core::Library::LibraryManager* m_libraryManager;
    Core::SettingsManager* m_settings;
    QTableWidget m_libraryList;
};

class PlaylistPage : public QWidget
{
public:
    explicit PlaylistPage(Core::SettingsManager* settings, QWidget* parent = nullptr);
    ~PlaylistPage() override;

private:
    Core::SettingsManager* m_settings;
};
} // namespace Gui::Settings
