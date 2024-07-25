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
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QMenuBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWindow>

constexpr auto MainWindowGeometry  = "Interface/Geometry";
constexpr auto MainWindowPrevState = "Interface/PrevState";

namespace Fooyin {
MainWindow::MainWindow(ActionManager* actionManager, MainMenuBar* menubar, SettingsManager* settings, QWidget* parent)
    : QMainWindow{parent}
    , m_mainMenu{menubar}
    , m_settings{settings}
    , m_prevState{Normal}
    , m_state{Normal}
    , m_isHiding{false}
    , m_hasQuit{false}
{
    actionManager->setMainWindow(this);
    setMenuBar(m_mainMenu->menuBar());
    m_settings->createSettingsDialog(this);

    if(auto prevState = m_settings->fileValue(QString::fromLatin1(MainWindowPrevState)).toString();
       !prevState.isEmpty()) {
        if(auto state = Utils::Enum::fromString<WindowState>(prevState)) {
            m_prevState = state.value();
        }
    }

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
    exit();
}

void MainWindow::open()
{
    switch(m_settings->value<Settings::Gui::StartupBehaviour>()) {
        case(StartMaximised):
            showMaximized();
            break;
        case(StartPrev):
            restoreState(m_prevState);
            break;
        case(StartHidden):
            m_state = Hidden;
            break;
        case(StartNormal):
        default:
            show();
            break;
    }
}

void MainWindow::raiseWindow()
{
    raise();
    show();
    activateWindow();

    m_state = currentState();
}

void MainWindow::toggleVisibility()
{
    if(m_state == Hidden) {
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

void MainWindow::setTitle(const QString& title)
{
    QString windowTitle{title};
    if(m_settings->value<Settings::Gui::LayoutEditing>()) {
        windowTitle.append(tr(" - Layout Editing Mode"));
    }
    setWindowTitle(windowTitle);
}

void MainWindow::resetTitle()
{
    setTitle(QStringLiteral("fooyin"));
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    m_settings->set<Settings::Gui::MainWindowPixelRatio>(devicePixelRatioF());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if(!m_isHiding) {
        if(event->spontaneous()) {
            const bool canHide = m_settings->value<Settings::Gui::Internal::ShowTrayIcon>()
                              && m_settings->value<Settings::Gui::Internal::TrayOnClose>();
            if(!isHidden() && QSystemTrayIcon::isSystemTrayAvailable() && canHide) {
                hideToTray(true);
            }
            else {
                exit();
            }
        }
        else {
            exit();
        }
    }

    event->accept();
    QMainWindow::closeEvent(event);
}

MainWindow::WindowState MainWindow::currentState()
{
    if(isHidden()) {
        return Hidden;
    }
    if(isMaximized()) {
        return Maximised;
    }

    return Normal;
}

void MainWindow::restoreState(WindowState state)
{
    switch(state) {
        case(Normal):
            restoreGeometry(m_settings->fileValue(QString::fromLatin1(MainWindowGeometry)).toByteArray());
            show();
            break;
        case(Maximised):
            showMaximized();
            break;
        case(Hidden):
            m_state = Hidden;
            break;
    }
}

void MainWindow::hideToTray(bool hide)
{
    if(hide) {
        m_isHiding  = true;
        m_prevState = std::exchange(m_state, Hidden);
        close();
        m_isHiding = false;
    }
    else {
        if(m_prevState == Maximised) {
            m_state = m_prevState;
            showMaximized();
        }
        else {
            m_state = Normal;
            show();
        }
    }
}

void MainWindow::exit()
{
    if(!m_hasQuit) {
        m_hasQuit = true;

        m_settings->fileSet(QString::fromLatin1(MainWindowGeometry), saveGeometry());
        m_settings->fileSet(QString::fromLatin1(MainWindowPrevState), Utils::Enum::toString(currentState()));

        Application::quit();
    }
}
} // namespace Fooyin

#include "moc_mainwindow.cpp"
