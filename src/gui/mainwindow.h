/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
class ActionManager;
class MainMenuBar;
class SettingsManager;
class Track;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum StartupBehaviour
    {
        StartNormal,
        StartMaximised,
        StartHidden,
        StartPrev
    };
    Q_ENUM(StartupBehaviour)

    enum WindowState
    {
        Normal,
        Maximised,
        Hidden
    };
    Q_ENUM(WindowState)

    explicit MainWindow(ActionManager* actionManager, MainMenuBar* menubar, SettingsManager* settings,
                        QWidget* parent = nullptr);
    ~MainWindow() override;

    void open();
    void raiseWindow();
    void toggleVisibility();

    void setTitle(const QString& title);
    void resetTitle();

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    WindowState currentState();
    void restoreState(WindowState state);
    void hideToTray(bool hide);
    void exit();

    MainMenuBar* m_mainMenu;
    SettingsManager* m_settings;

    WindowState m_prevState;
    WindowState m_state;
    bool m_isHiding;
    bool m_hasQuit;
};
} // namespace Fooyin
