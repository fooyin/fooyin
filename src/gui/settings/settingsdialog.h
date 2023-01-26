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

#include <QDialog>

class QListWidgetItem;
class QListWidget;
class QStackedWidget;

namespace Core {
class SettingsManager;

namespace Library {
class LibraryManager;
}
} // namespace Core

namespace Gui::Settings {
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Order of pages in settings
    // Do not change!
    enum class Page
    {
        General = 0,
        Library,
        Playlist
    };

    explicit SettingsDialog(Core::Library::LibraryManager* libraryManager, Core::SettingsManager* settings,
                            QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void setupUi();

    void createIcons() const;

    void changePage(QListWidgetItem* current, QListWidgetItem* previous);
    void openPage(Page page);

private:
    Core::Library::LibraryManager* m_libraryManager;
    Core::SettingsManager* m_settings;

    QListWidget* m_contentsWidget;
    QStackedWidget* m_pagesWidget;
};
} // namespace Gui::Settings
