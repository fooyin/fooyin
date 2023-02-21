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
#include "gui/quicksetup/quicksetupdialog.h"
#include "guiconstants.h"
#include "guisettings.h"
#include "layoutprovider.h"
#include "mainmenubar.h"

#include <core/coresettings.h>

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenuBar>
#include <QTextEdit>
#include <QTimer>

namespace Gui {
struct MainWindow::Private
{
    Utils::ActionManager* actionManager;
    Core::SettingsManager* settings;
    Widgets::EditableLayout* editableLayout;

    MainMenuBar* mainMenu;

    QAction* openSettings;
    QAction* layoutEditing;
    QAction* openQuickSetup;
    QAction* rescan;
    QAction* quitAction;

    LayoutProvider* layoutProvider;
    QuickSetupDialog* quickSetupDialog;

    Private(Utils::ActionManager* actionManager, Core::SettingsManager* settings, LayoutProvider* layoutProvider,
            Widgets::EditableLayout* editableLayout)
        : actionManager{actionManager}
        , settings{settings}
        , editableLayout{editableLayout}
        , layoutProvider{layoutProvider}
    {
        registerLayouts();
    }

    void registerLayouts() const
    {
        layoutProvider->registerLayout("Empty", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[],\"State\":\"AAAA/"
                                                "wAAAAEAAAABAAACLwD/////AQAAAAIA\"}}]}");

        layoutProvider->registerLayout(
            "Simple", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[\"Status\",\"Playlist\",\"Controls\"],"
                      "\"State\":\"AAAA/wAAAAEAAAAEAAAAGQAAA94AAAAUAAAAAAD/////AQAAAAIA\"}}]}");

        layoutProvider->registerLayout(
            "Stone", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[\"Status\",\"Search\",{\"SplitterHorizontal\":{"
                     "\"Children\":[\"FilterAlbumArtist\",\"Playlist\"],\"State\":\"AAAA/wAAAAEAAAADAAAA/wAABlEAAAAAAP/"
                     "///8BAAAAAQA=\"}},\"Controls\"],\"State\":\"AAAA/wAAAAEAAAAFAAAAGQAAAB4AAAO8AAAAFAAAAAAA/////"
                     "wEAAAACAA==\"}}]}");

        layoutProvider->registerLayout(
            "Vision", "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[\"Status\",{\"SplitterHorizontal\":{"
                      "\"Children\":[\"Controls\",\"Search\"],\"State\":\"AAAA/wAAAAEAAAADAAAD1wAAA3kAAAAAAP////"
                      "8BAAAAAQA=\"}},{\"SplitterHorizontal\":{\"Children\":[\"Artwork\",\"Playlist\"],\"State\":"
                      "\"AAAA/wAAAAEAAAADAAAD2AAAA3gAAAAAAP////8BAAAAAQA=\"}}],\"State\":\"AAAA/"
                      "wAAAAEAAAAEAAAAGQAAAB4AAAPUAAAAFAD/////AQAAAAIA\"}}]}");

        layoutProvider->registerLayout(
            "Ember",
            "{\"Layout\":[{\"SplitterVertical\":{\"Children\":[{\"SplitterHorizontal\":{\"Children\":[\"FilterGenre\","
            "\"FilterAlbumArtist\",\"FilterArtist\",\"FilterAlbum\"],\"State\":\"AAAA/"
            "wAAAAEAAAAFAAABAAAAAQAAAAEAAAABAAAAALUA/////"
            "wEAAAABAA==\"}},{\"SplitterHorizontal\":{\"Children\":[\"Controls\",\"Search\"],\"State\":\"AAAA/"
            "wAAAAEAAAADAAAFfgAAAdIAAAAAAP////"
            "8BAAAAAQA=\"}},{\"SplitterHorizontal\":{\"Children\":[{\"SplitterVertical\":{\"Children\":[\"Artwork\","
            "\"Info\"],\"State\":\"AAAA/wAAAAEAAAADAAABzAAAAbcAAAAAAP////8BAAAAAgA=\"}},\"Playlist\"],\"State\":\"AAAA/"
            "wAAAAEAAAADAAABdQAABdsAAAAAAP////8BAAAAAQA=\"}},\"Status\"],\"State\":\"AAAA/"
            "wAAAAEAAAAFAAAA+gAAAB4AAALWAAAAGQAAAAAA/////wEAAAACAA==\"}}]}");
    }
};

MainWindow::MainWindow(Utils::ActionManager* actionManager, Core::SettingsManager* settings,
                       LayoutProvider* layoutProvider, Widgets::EditableLayout* editableLayout, QWidget* parent)
    : QMainWindow{parent}
    , p{std::make_unique<Private>(actionManager, settings, layoutProvider, editableLayout)}
{ }

MainWindow::~MainWindow()
{
    p->settings->set<Settings::Geometry>(saveGeometry().toBase64());
    p->editableLayout->saveLayout();
}

void MainWindow::setupUi()
{
    if(objectName().isEmpty()) {
        setObjectName(QString::fromUtf8("MainWindow"));
    }

    resize(1280, 720);
    setMinimumSize(410, 320);
    setWindowIcon(QIcon(Constants::Icons::Fooyin));

    const QByteArray geometryArray = p->settings->value<Settings::Geometry>();
    const QByteArray geometry      = QByteArray::fromBase64(geometryArray);
    restoreGeometry(geometry);

    p->editableLayout->initialise();
    setCentralWidget(p->editableLayout);

    if(p->settings->value<Core::Settings::FirstRun>()) {
        // Delay showing until size of parent widget (this) is set.
        QTimer::singleShot(1000, p->quickSetupDialog, &QuickSetupDialog::show);
    }
}

void MainWindow::setupMenu()
{
    p->mainMenu = new MainMenuBar(p->actionManager, this);
    setMenuBar(p->mainMenu->menuBar());

    p->quickSetupDialog = new QuickSetupDialog(p->layoutProvider, this);

    Utils::ActionContainer* viewMenu = p->actionManager->actionContainer(Constants::Menus::View);

    if(viewMenu) {
        const QIcon layoutEditingIcon = QIcon(Constants::Icons::LayoutEditing);
        p->layoutEditing              = new QAction(layoutEditingIcon, tr("Layout &Editing Mode"), this);
        p->actionManager->registerAction(p->layoutEditing, Constants::Actions::LayoutEditing);
        viewMenu->addAction(p->layoutEditing, Constants::Groups::Three);
        connect(p->layoutEditing, &QAction::triggered, this, &MainWindow::enableLayoutEditing);
        p->settings->subscribe<Settings::LayoutEditing>(p->layoutEditing, &QAction::setChecked);
        p->layoutEditing->setCheckable(true);
        p->layoutEditing->setChecked(p->settings->value<Settings::LayoutEditing>());

        const QIcon quickSetupIcon = QIcon(Constants::Icons::QuickSetup);
        p->openQuickSetup          = new QAction(quickSetupIcon, tr("&Quick Setup"), this);
        p->actionManager->registerAction(p->openQuickSetup, Constants::Actions::LayoutEditing);
        viewMenu->addAction(p->openQuickSetup, Constants::Groups::Three);
        connect(p->openQuickSetup, &QAction::triggered, p->quickSetupDialog, &QuickSetupDialog::show);
        connect(p->quickSetupDialog, &QuickSetupDialog::layoutChanged, p->editableLayout,
                &Widgets::EditableLayout::changeLayout);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    emit closing();
    QMainWindow::closeEvent(event);
}

void MainWindow::enableLayoutEditing(bool enable)
{
    p->settings->set<Settings::LayoutEditing>(enable);
}
} // namespace Gui
