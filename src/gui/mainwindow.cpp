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
#include "gui/menu/editmenu.h"
#include "gui/menu/filemenu.h"
#include "gui/menu/helpmenu.h"
#include "gui/menu/librarymenu.h"
#include "gui/menu/playbackmenu.h"
#include "gui/menu/viewmenu.h"
#include "gui/quicksetup/quicksetupdialog.h"
#include "guiconstants.h"
#include "guisettings.h"
#include "layoutprovider.h"
#include "mainmenubar.h"

#include <core/coresettings.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenuBar>
#include <QTextEdit>
#include <QTimer>

namespace Fy::Gui {
MainWindow::MainWindow(Utils::ActionManager* actionManager, Core::Player::PlayerManager* playerManager,
                       Core::Library::LibraryManager* libraryManager, Utils::SettingsManager* settings,
                       LayoutProvider* layoutProvider, Widgets::WidgetFactory* widgetFactory, QWidget* parent)
    : QMainWindow{parent}
    , m_actionManager{actionManager}
    , m_playerManager{playerManager}
    , m_libraryManager{libraryManager}
    , m_settings{settings}
    , m_widgetFactory{widgetFactory}
    , m_layoutProvider{layoutProvider}
    , m_editableLayout{
          new Widgets::EditableLayout(m_settings, m_actionManager, m_widgetFactory, m_layoutProvider, this)}
{
    actionManager->setMainWindow(this);

    setupMenu();
    registerLayouts();
}

MainWindow::~MainWindow()
{
    m_settings->set<Settings::Geometry>(saveGeometry().toBase64());
    m_editableLayout->saveLayout();
}

void MainWindow::setupUi()
{
    if(objectName().isEmpty()) {
        setObjectName(QString::fromUtf8("MainWindow"));
    }

    resize(1280, 720);
    setMinimumSize(410, 320);
    setWindowIcon(QIcon(Constants::Icons::Fooyin));

    const QByteArray geometryArray = m_settings->value<Settings::Geometry>();
    const QByteArray geometry      = QByteArray::fromBase64(geometryArray);
    restoreGeometry(geometry);

    m_editableLayout->initialise();
    setCentralWidget(m_editableLayout);

    m_fileMenu     = new FileMenu(m_actionManager, m_settings, this);
    m_editMenu     = new EditMenu(m_actionManager, this);
    m_viewMenu     = new ViewMenu(m_actionManager, m_settings, this);
    m_playbackMenu = new PlaybackMenu(m_actionManager, m_playerManager, this);
    m_libraryMenu  = new LibraryMenu(m_actionManager, m_libraryManager, m_settings, this);
    m_helpMenu     = new HelpMenu(m_actionManager, this);

    connect(m_viewMenu, &ViewMenu::layoutEditingChanged, this, &MainWindow::enableLayoutEditing);
    connect(m_viewMenu, &ViewMenu::openQuickSetup, m_quickSetupDialog, &QuickSetupDialog::show);

    if(m_settings->value<Core::Settings::FirstRun>()) {
        // Delay showing until size of parent widget (this) is set.
        QTimer::singleShot(1000, m_quickSetupDialog, &QuickSetupDialog::show);
    }
}

void MainWindow::setupMenu()
{
    m_mainMenu = new MainMenuBar(m_actionManager, this);
    setMenuBar(m_mainMenu->menuBar());

    m_quickSetupDialog = new QuickSetupDialog(m_layoutProvider, this);

    connect(
        m_quickSetupDialog, &QuickSetupDialog::layoutChanged, m_editableLayout, &Widgets::EditableLayout::changeLayout);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    emit closing();
    QMainWindow::closeEvent(event);
}

void MainWindow::enableLayoutEditing(bool enable)
{
    m_settings->set<Settings::LayoutEditing>(enable);
}

void MainWindow::registerLayouts() const
{
    m_layoutProvider->registerLayout("Empty",
                                     R"({"Layout":[{"SplitterVertical":{"Children":[],
                                     "State":"AAAA/wAAAAEAAAABAAACLwD/////AQAAAAIA"}}]})");

    m_layoutProvider->registerLayout("Simple",
                                     R"({"Layout":[{"SplitterVertical":{"Children":["Status","Playlist","Controls"],
                                     "State":"AAAA/wAAAAEAAAAEAAAAGQAAA94AAAAUAAAAAAD/////AQAAAAIA"}}]})");

    m_layoutProvider->registerLayout("Vision",
                                     R"({"Layout":[{"SplitterVertical":{"Children":["Status",{"SplitterHorizontal":{
                                     "Children":["Controls","Search"],"State":"AAAA/wAAAAEAAAADAAAD1wAAA3kAAAAAAP////
                                     8BAAAAAQA="}},{"SplitterHorizontal":{"Children":["Artwork","Playlist"],"State":
                                     "AAAA/wAAAAEAAAADAAAD2AAAA3gAAAAAAP////8BAAAAAQA="}}],"State":"AAAA/
                                     wAAAAEAAAAEAAAAGQAAAB4AAAPUAAAAFAD/////AQAAAAIA"}}]})");
}
} // namespace Fy::Gui
