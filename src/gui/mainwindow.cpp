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

#include "mainwindow.h"

#include "editablelayout.h"
#include "guiconstants.h"
#include "guisettings.h"
#include "mainmenubar.h"

#include <core/coresettings.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenuBar>
#include <QTextEdit>
#include <QTimer>

namespace Fy::Gui {
MainWindow::MainWindow(Utils::ActionManager* actionManager, Utils::SettingsManager* settings,
                       Widgets::EditableLayout* editableLayout, QWidget* parent)
    : QMainWindow{parent}
    , m_actionManager{actionManager}
    , m_settings{settings}
    , m_editableLayout{editableLayout}
{
    actionManager->setMainWindow(this);

    setupMenu();
}

MainWindow::~MainWindow()
{
    m_settings->set<Settings::Geometry>(saveGeometry().toBase64());
    m_settings->set<Settings::SettingsGeometry>(m_settings->settingsDialog()->geometry().toBase64());
}

void MainWindow::setupUi()
{
    if(objectName().isEmpty()) {
        setObjectName(QString::fromUtf8("MainWindow"));
    }

    resize(1280, 720);
    setMinimumSize(410, 320);
    setWindowIcon(QIcon::fromTheme(Constants::Icons::Fooyin));

    const QByteArray mainGeometry = QByteArray::fromBase64(m_settings->value<Settings::Geometry>());
    restoreGeometry(mainGeometry);

    const QByteArray settingsGeometry = QByteArray::fromBase64(m_settings->value<Settings::SettingsGeometry>());
    m_settings->settingsDialog()->updateGeometry(settingsGeometry);

    setCentralWidget(m_editableLayout);

    if(m_settings->value<Core::Settings::FirstRun>()) {
        // Delay showing until size of parent widget (this) is set.
        QTimer::singleShot(1000, m_editableLayout, &Widgets::EditableLayout::showQuickSetup);
    }
}

void MainWindow::setupMenu()
{
    m_mainMenu = new MainMenuBar(m_actionManager, this);
    setMenuBar(m_mainMenu->menuBar());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    emit closing();
    QMainWindow::closeEvent(event);
}
} // namespace Fy::Gui
