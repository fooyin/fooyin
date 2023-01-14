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

#include "mainwindow.h"

#include "actions/actioncontainer.h"
#include "actions/actionmanager.h"
#include "core/constants.h"
#include "core/coresettings.h"
#include "core/gui/editablelayout.h"
#include "core/gui/quicksetupdialog.h"
#include "core/gui/settings/settingsdialog.h"
#include "core/library/musiclibrary.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDir>
#include <QMenuBar>
#include <QTextEdit>
#include <QTimer>
#include <pluginsystem/pluginmanager.h>

namespace Core {
struct MainWindow::Private
{
    Widgets::EditableLayout* mainLayout;
    Widgets::SettingsDialog* settingsDialog;
    Library::MusicLibrary* library;

    QAction* openSettings;
    QAction* layoutEditing;
    QAction* openQuickSetup;
    QAction* rescan;
    QAction* quitAction;

    SettingsManager* settings;

    QuickSeupDialog* quickSetupDialog;

    ActionManager* actionManager;

    Private(ActionManager* actionManager, SettingsManager* settings, Widgets::SettingsDialog* settingsDialog,
            Library::MusicLibrary* library)
        : settingsDialog(settingsDialog)
        , library(library)
        , settings(settings)
        , actionManager(actionManager)
    { }
};

MainWindow::MainWindow(ActionManager* actionManager, SettingsManager* settings, Widgets::SettingsDialog* settingsDialog,
                       Library::MusicLibrary* library, QWidget* parent)
    : QMainWindow(parent)
    , p(std::make_unique<Private>(actionManager, settings, settingsDialog, library))
{ }

MainWindow::~MainWindow()
{
    p->settings->set(Setting::Geometry, QString::fromUtf8(saveGeometry().toBase64()));
    p->mainLayout->saveLayout();
}

void MainWindow::setupUi()
{
    if(objectName().isEmpty()) {
        setObjectName(QString::fromUtf8("MainWindow"));
    }

    resize(1280, 720);
    setMinimumSize(410, 320);
    setWindowIcon(QIcon(Core::Constants::Icons::Fooyin));

    QByteArray geometryArray = p->settings->value(Setting::Geometry).toString().toUtf8();
    QByteArray geometry      = QByteArray::fromBase64(geometryArray);
    restoreGeometry(geometry);

    p->mainLayout       = new Widgets::EditableLayout(this);
    p->quickSetupDialog = new QuickSeupDialog(this);

    setCentralWidget(p->mainLayout);

    ActionContainer* menubar = p->actionManager->createMenuBar(Core::Constants::MenuBar);
    setMenuBar(menubar->menuBar());

    menubar->appendGroup(Core::Constants::Groups::File);
    menubar->appendGroup(Core::Constants::Groups::Edit);
    menubar->appendGroup(Core::Constants::Groups::View);
    menubar->appendGroup(Core::Constants::Groups::Playback);
    menubar->appendGroup(Core::Constants::Groups::Library);
    menubar->appendGroup(Core::Constants::Groups::Help);

    ActionContainer* fileMenu = p->actionManager->createMenu(Core::Constants::Menus::File);
    menubar->addMenu(fileMenu, Core::Constants::Groups::File);
    fileMenu->menu()->setTitle(tr("&File"));
    fileMenu->appendGroup(Core::Constants::Groups::Three);

    ActionContainer* editMenu = p->actionManager->createMenu(Core::Constants::Menus::Edit);
    menubar->addMenu(editMenu, Core::Constants::Groups::Edit);
    editMenu->menu()->setTitle(tr("&Edit"));

    ActionContainer* viewMenu = p->actionManager->createMenu(Core::Constants::Menus::View);
    menubar->addMenu(viewMenu, Core::Constants::Groups::View);
    viewMenu->menu()->setTitle(tr("&View"));
    viewMenu->appendGroup(Core::Constants::Groups::Three);

    ActionContainer* playbackMenu = p->actionManager->createMenu(Core::Constants::Menus::Playback);
    menubar->addMenu(playbackMenu, Core::Constants::Groups::Playback);
    playbackMenu->menu()->setTitle(tr("&Playback"));

    ActionContainer* libraryMenu = p->actionManager->createMenu(Core::Constants::Menus::Library);
    menubar->addMenu(libraryMenu, Core::Constants::Groups::Library);
    libraryMenu->menu()->setTitle(tr("&Library"));
    libraryMenu->appendGroup(Core::Constants::Groups::Two);
    libraryMenu->appendGroup(Core::Constants::Groups::Three);

    ActionContainer* helpMenu = p->actionManager->createMenu(Core::Constants::Menus::Help);
    menubar->addMenu(helpMenu, Core::Constants::Groups::Help);
    helpMenu->menu()->setTitle(tr("&Help"));

    QIcon quitIcon = QIcon(Core::Constants::Icons::Quit);
    p->quitAction  = new QAction(quitIcon, tr("E&xit"), this);
    p->actionManager->registerAction(p->quitAction, Core::Constants::Actions::Exit);
    fileMenu->addAction(p->quitAction, Core::Constants::Groups::Three);
    connect(p->quitAction, &QAction::triggered, this, &MainWindow::close);

    QIcon layoutEditingIcon = QIcon(Core::Constants::Icons::LayoutEditing);
    p->layoutEditing        = new QAction(layoutEditingIcon, tr("Layout &Editing Mode"), this);
    p->actionManager->registerAction(p->layoutEditing, Core::Constants::Actions::LayoutEditing);
    viewMenu->addAction(p->layoutEditing, Core::Constants::Groups::Three);
    connect(p->layoutEditing, &QAction::triggered, this, &MainWindow::enableLayoutEditing);
    // connect(p->settings, &Settings::layoutEditingChanged, p->layoutEditing, &QAction::setChecked);
    p->layoutEditing->setCheckable(true);
    p->layoutEditing->setChecked(p->settings->value(Setting::LayoutEditing).toBool());

    QIcon quickSetupIcon = QIcon(Core::Constants::Icons::QuickSetup);
    p->openQuickSetup    = new QAction(quickSetupIcon, tr("&Quick Setup"), this);
    p->actionManager->registerAction(p->openQuickSetup, Core::Constants::Actions::LayoutEditing);
    viewMenu->addAction(p->openQuickSetup, Core::Constants::Groups::Three);
    connect(p->openQuickSetup, &QAction::triggered, p->quickSetupDialog, &QuickSeupDialog::show);
    connect(p->quickSetupDialog, &QuickSeupDialog::layoutChanged, p->mainLayout,
            &Widgets::EditableLayout::changeLayout);

    QIcon rescanIcon = QIcon(Core::Constants::Icons::RescanLibrary);
    p->rescan        = new QAction(rescanIcon, tr("&Rescan Library"), this);
    p->actionManager->registerAction(p->rescan, Core::Constants::Actions::Rescan);
    libraryMenu->addAction(p->rescan, Core::Constants::Groups::Two);
    connect(p->rescan, &QAction::triggered, p->library, &Library::MusicLibrary::reloadAll);

    QIcon settingsIcon = QIcon(Core::Constants::Icons::Settings);
    p->openSettings    = new QAction(settingsIcon, tr("&Settings"), this);
    p->actionManager->registerAction(p->openSettings, Core::Constants::Actions::Settings);
    libraryMenu->addAction(p->openSettings, Core::Constants::Groups::Three);
    connect(p->openSettings, &QAction::triggered, p->settingsDialog, &Widgets::SettingsDialog::show);

    if(p->settings->value(Setting::FirstRun).toBool()) {
        // Delay showing until size of parent widget (this) is set.
        QTimer::singleShot(1000, p->quickSetupDialog, &QuickSeupDialog::show);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    emit closing();
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
}

void MainWindow::contextMenuEvent(QContextMenuEvent* event)
{
    QMainWindow::contextMenuEvent(event);
}

void MainWindow::enableLayoutEditing(bool enable)
{
    p->settings->set(Setting::LayoutEditing, enable);
}
}; // namespace Core
