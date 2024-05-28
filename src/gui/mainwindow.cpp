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

#include "mainwindow.h"

#include "internalguisettings.h"
#include "menubar/mainmenubar.h"

#include <core/application.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QContextMenuEvent>
#include <QMenuBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWindow>

constexpr auto MainWindowGeometry = "Interface/Geometry";

namespace Fooyin {
MainWindow::MainWindow(ActionManager* actionManager, MainMenuBar* menubar, SettingsManager* settings, QWidget* parent)
    : QMainWindow{parent}
    , m_mainMenu{menubar}
    , m_settings{settings}
    , m_isHidden{false}
    , m_isMaximised{false}
    , m_isMinimised{false}
    , m_hidingToTray{false}
{
    actionManager->setMainWindow(this);
    setMenuBar(m_mainMenu->menuBar());
    m_settings->createSettingsDialog(this);

    resetTitle();

    resize(1280, 720);
    setWindowIcon(Utils::iconFromTheme(Constants::Icons::Fooyin));

    if(windowHandle()) {
        QObject::connect(windowHandle(), &QWindow::screenChanged, this,
                         [this]() { m_settings->set<Settings::Gui::MainWindowPixelRatio>(devicePixelRatioF()); });
    }
}

MainWindow::~MainWindow()
{
    m_settings->fileSet(QString::fromLatin1(MainWindowGeometry), saveGeometry());
}

void MainWindow::open()
{
    switch(m_settings->value<Settings::Gui::StartupBehaviour>()) {
        case(Maximised):
            showMaximized();
            break;
        case(RememberLast):
            restoreGeometry(m_settings->fileValue(QString::fromLatin1(MainWindowGeometry)).toByteArray());
            show();
            break;
        case(Normal):
        default:
            show();
            break;
    }
}

void MainWindow::toggleVisibility()
{
    if(m_isHidden) {
        hideToTray(false);
    }
    else if(isActiveWindow()) {
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        hideToTray(true);
    }
    else if(isMinimized()) {
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        hideToTray(false);
    }
    else if(!isVisible()) {
        show();
        activateWindow();
    }
    else {
        activateWindow();
        raise();
    }
}

void MainWindow::prependTitle(const QString& title)
{
    if(title.isEmpty()) {
        resetTitle();
    }
    else {
        setWindowTitle(title + QStringLiteral(" 𑁋 fooyin"));
    }
}

void MainWindow::resetTitle()
{
    setWindowTitle(QStringLiteral("fooyin"));
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    m_settings->set<Settings::Gui::MainWindowPixelRatio>(devicePixelRatioF());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    emit closing();

    if(m_hidingToTray) {
        m_hidingToTray = false;
        QMainWindow::closeEvent(event);
        return;
    }

    const bool canHide = m_settings->value<Settings::Gui::Internal::ShowTrayIcon>()
                      && m_settings->value<Settings::Gui::Internal::TrayOnClose>();

    if(!m_isHidden && QSystemTrayIcon::isSystemTrayAvailable() && canHide) {
        hideToTray(true);
    }
    else {
        Application::quit();
    }
}

void MainWindow::hideToTray(bool hide)
{
    m_isHidden = hide;

    if(hide) {
        m_isMaximised  = isMaximized();
        m_isMinimised  = isMinimized();
        m_hidingToTray = true;
        close();
    }
    else {
        if(m_isMinimised) {
            showMinimized();
        }
        else if(m_isMaximised) {
            showMaximized();
        }
        else {
            show();
        }
    }
}
} // namespace Fooyin

#include "moc_mainwindow.cpp"
