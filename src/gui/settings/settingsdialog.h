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

namespace Core::Library {
class LibraryManager;
}

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

    explicit SettingsDialog(Core::Library::LibraryManager* libManager, QWidget* parent = nullptr);
    ~SettingsDialog() override;

    void setupUi();

    void changePage(QListWidgetItem* current, QListWidgetItem* previous);
    void openPage(Page page);

private:
    struct Private;
    std::unique_ptr<SettingsDialog::Private> p;
};
} // namespace Gui::Settings
