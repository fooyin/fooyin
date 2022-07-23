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

#include "gui/editablelayout.h"
#include "gui/quicksetupdialog.h"
#include "gui/settings/settingsdialog.h"
#include "library/musiclibrary.h"
#include "settings/settings.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDir>
#include <QFontDatabase>
#include <QMenuBar>
#include <QTextEdit>
#include <QTimer>

struct MainWindow::Private
{
    EditableLayout* mainLayout;
    SettingsDialog* settingsDialog;
    Library::MusicLibrary* library;

    QMenuBar* menuBar;

    QMenu* menuFile;
    QMenu* menuEdit;
    QMenu* menuView;
    QMenu* menuPlayback;
    QMenu* menuLibrary;
    QMenu* menuHelp;

    QAction* openSettings;
    QAction* layoutEditing;
    QAction* openQuickSetup;
    QAction* rescan;

    Settings* settings;

    QuickSeupDialog* quickSetupDialog;

    WidgetProvider* widgetProvider;

    Private(WidgetProvider* widgetProvider, SettingsDialog* settingsDialog, Library::MusicLibrary* library)
        : settingsDialog(settingsDialog)
        , library(library)
        , settings(Settings::instance())
        , widgetProvider(widgetProvider)
    { }
};

MainWindow::MainWindow(WidgetProvider* widgetProvider, SettingsDialog* settingsDialog, Library::MusicLibrary* library,
                       QWidget* parent)
    : QMainWindow(parent)
    , p(std::make_unique<Private>(widgetProvider, settingsDialog, library))
{
    QFontDatabase::addApplicationFont("://fonts/Guifx v2 Transports.ttf");

    setupUi();
}

MainWindow::~MainWindow()
{
    p->settings->set(Settings::Setting::Geometry, QString::fromUtf8(saveGeometry().toBase64()));
    p->mainLayout->saveLayout();
}

void MainWindow::setupUi()
{
    if(objectName().isEmpty()) {
        setObjectName(QString::fromUtf8("MainWindow"));
    }

    resize(1280, 720);
    setMinimumSize(410, 320);
    setWindowIcon(QIcon("://images/fooyin.png"));

    QByteArray geometryArray = p->settings->value(Settings::Setting::Geometry).toString().toUtf8();
    QByteArray geometry = QByteArray::fromBase64(geometryArray);
    restoreGeometry(geometry);

    p->mainLayout = new EditableLayout(p->widgetProvider, this);
    p->quickSetupDialog = new QuickSeupDialog(this);

    setCentralWidget(p->mainLayout);

    p->menuBar = new QMenuBar(this);

    p->menuFile = new QMenu(p->menuBar);
    p->menuEdit = new QMenu(p->menuBar);
    p->menuView = new QMenu(p->menuBar);
    p->menuPlayback = new QMenu(p->menuBar);
    p->menuLibrary = new QMenu(p->menuBar);
    p->menuHelp = new QMenu(p->menuBar);

    p->menuBar->setGeometry(QRect(0, 0, 1290, 20));
    setMenuBar(p->menuBar);

    p->openSettings = new QAction(this);
    p->layoutEditing = new QAction(this);
    p->openQuickSetup = new QAction(this);
    p->rescan = new QAction(this);

    p->layoutEditing->setCheckable(true);
    p->layoutEditing->setChecked(p->settings->value(Settings::Setting::LayoutEditing).toBool());

    p->menuBar->addAction(p->menuFile->menuAction());
    p->menuBar->addAction(p->menuEdit->menuAction());
    p->menuBar->addAction(p->menuView->menuAction());
    p->menuBar->addAction(p->menuPlayback->menuAction());
    p->menuBar->addAction(p->menuLibrary->menuAction());
    p->menuBar->addAction(p->menuHelp->menuAction());

    p->menuFile->setTitle("File");
    p->menuEdit->setTitle("Edit");
    p->menuView->setTitle("View");
    p->menuPlayback->setTitle("Playback");
    p->menuLibrary->setTitle("Library");
    p->menuHelp->setTitle("Help");

    p->openSettings->setText("Settings");
    p->layoutEditing->setText("Layout Editing Mode");
    p->openQuickSetup->setText("Quick Setup");
    p->rescan->setText("Rescan Library");

    p->menuLibrary->addAction(p->rescan);
    p->menuLibrary->addAction(p->openSettings);
    p->menuView->addAction(p->layoutEditing);
    p->menuView->addAction(p->openQuickSetup);

    connect(p->rescan, &QAction::triggered, p->library, &Library::MusicLibrary::reloadAll);
    connect(p->openSettings, &QAction::triggered, p->settingsDialog, &SettingsDialog::show);
    connect(p->settings, &Settings::layoutEditingChanged, this, [this](bool enabled) {
        p->layoutEditing->setChecked(enabled);
    });
    connect(p->layoutEditing, &QAction::triggered, this, [this](bool checked) {
        p->settings->set(Settings::Setting::LayoutEditing, checked);
    });
    connect(p->openQuickSetup, &QAction::triggered, p->quickSetupDialog, &QuickSeupDialog::show);
    connect(p->quickSetupDialog, &QuickSeupDialog::layoutChanged, p->mainLayout, &EditableLayout::changeLayout);

    if(p->settings->value(Settings::Setting::FirstRun).toBool()) {
        // Delay showing until size of parent widget (this) is set.
        QTimer::singleShot(1000, p->quickSetupDialog, &QuickSeupDialog::show);
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
}

void MainWindow::contextMenuEvent(QContextMenuEvent* e)
{
    QMainWindow::contextMenuEvent(e);
}
