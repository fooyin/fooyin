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

#include <QMainWindow>

namespace Core::Library {
class MusicLibrary;
}

namespace Core {
class ActionManager;
class SettingsManager;

namespace Widgets {
class SettingsDialog;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ActionManager* actionManager, SettingsManager* settings,
                        Widgets::SettingsDialog* settingsDialog, Library::MusicLibrary* library,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    void setupUi();

signals:
    void closing();

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    void enableLayoutEditing(bool enable);

private:
    struct Private;
    std::unique_ptr<MainWindow::Private> p;
};
} // namespace Core
