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

#include <QMainWindow>

namespace Fy {

namespace Utils {
class ActionManager;
class SettingsManager;
} // namespace Utils

namespace Gui {
class MainMenuBar;

namespace Widgets {
class EditableLayout;
} // namespace Widgets

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum StartupBehaviour
    {
        Normal,
        Maximised,
        RememberLast
    };
    Q_ENUM(StartupBehaviour)

    explicit MainWindow(Utils::ActionManager* actionManager, Utils::SettingsManager* settings,
                        Widgets::EditableLayout* editableLayout, QWidget* parent = nullptr);

    ~MainWindow() override;

    void open();

signals:
    void closing();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    Utils::ActionManager* m_actionManager;
    Utils::SettingsManager* m_settings;

    MainMenuBar* m_mainMenu;

    Widgets::EditableLayout* m_editableLayout;
};
} // namespace Gui
} // namespace Fy
