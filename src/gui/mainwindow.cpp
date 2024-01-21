/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include "mainwindow.h"

#include "mainmenubar.h"

#include <core/constants.h>
#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QContextMenuEvent>
#include <QMenuBar>
#include <QTimer>

constexpr auto MainWindowGeometry = "Interface/Geometry";

namespace Fooyin {
MainWindow::MainWindow(ActionManager* actionManager, MainMenuBar* menubar, SettingsManager* settings, QWidget* parent)
    : QMainWindow{parent}
    , m_mainMenu{menubar}
    , m_settings{settings}
{
    actionManager->setMainWindow(this);
    setMenuBar(m_mainMenu->menuBar());

    setWindowTitle(Constants::AppName);

    resize(1280, 720);
    setWindowIcon(QIcon::fromTheme(Constants::Icons::Fooyin));
}

MainWindow::~MainWindow()
{
    m_settings->fileSet(MainWindowGeometry, saveGeometry());
}

void MainWindow::open()
{
    switch(m_settings->value<Settings::Gui::StartupBehaviour>()) {
        case(Maximised): {
            showMaximized();
            break;
        }
        case(RememberLast): {
            restoreGeometry(m_settings->fileValue(MainWindowGeometry).toByteArray());
            show();
            break;
        }
        case(Normal):
        default: {
            show();
            break;
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    emit closing();
    QMainWindow::closeEvent(event);
}
} // namespace Fooyin

#include "moc_mainwindow.cpp"
