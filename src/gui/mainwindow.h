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

#include <core/scripting/scriptparser.h>

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
        Normal,
        Maximised,
        RememberLast
    };
    Q_ENUM(StartupBehaviour)

    explicit MainWindow(ActionManager* actionManager, MainMenuBar* menubar, SettingsManager* settings,
                        QWidget* parent = nullptr);

    ~MainWindow() override;

    void open();
    void updateTitle(const Track& track);

signals:
    void closing();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    MainMenuBar* m_mainMenu;
    SettingsManager* m_settings;
    ScriptParser m_parser;
};
} // namespace Fooyin
