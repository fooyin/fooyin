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

#include "widgetprovider.h"

#include "core/gui/controls/controlwidget.h"
#include "core/gui/info/infowidget.h"
#include "core/gui/library/coverwidget.h"
#include "core/gui/library/statuswidget.h"
#include "core/gui/playlist/playlistwidget.h"
#include "core/gui/settings/settingsdialog.h"
#include "core/gui/widgets/spacer.h"
#include "core/gui/widgets/splitterwidget.h"
#include "core/library/librarymanager.h"
#include "widgetfactory.h"

#include <QMenu>
#include <pluginsystem/pluginmanager.h>
#include <utils/enumhelper.h>

struct WidgetProvider::Private
{
    PlayerManager* playerManager;
    Library::LibraryManager* libraryManager;
    Library::MusicLibrary* library;
    SettingsDialog* settingsDialog;
    Widgets::WidgetFactory* widgetFactory;

    QMap<QString, QMenu*> menus;

    Private(PlayerManager* playerManager, Library::LibraryManager* libraryManager, Library::MusicLibrary* library,
            SettingsDialog* settingsDialog)
        : playerManager(playerManager)
        , libraryManager(libraryManager)
        , library(library)
        , settingsDialog(settingsDialog)
        , widgetFactory(PluginSystem::object<Widgets::WidgetFactory>())
    { }
};

WidgetProvider::WidgetProvider(PlayerManager* playerManager, Library::LibraryManager* libraryManager,
                               Library::MusicLibrary* library, SettingsDialog* settingsDialog, QObject* parent)
    : QObject(parent)
    , p(std::make_unique<Private>(playerManager, libraryManager, library, settingsDialog))
{
    registerWidgets();
}

WidgetProvider::~WidgetProvider() = default;

FyWidget* WidgetProvider::createWidget(const QString& widget, SplitterWidget* splitter)
{
    auto* createdWidget = p->widgetFactory->make(widget);
    splitter->addWidget(createdWidget);
    return createdWidget;
}

SplitterWidget* WidgetProvider::createSplitter(Qt::Orientation type, QWidget* parent)
{
    auto* splitter = new SplitterWidget(parent);
    splitter->setOrientation(type);
    return splitter;
}

void WidgetProvider::addMenuActions(QMenu* menu, SplitterWidget* splitter)
{
    p->menus.clear();
    auto widgets = p->widgetFactory->widgetNames();
    auto widgetSubMenus = p->widgetFactory->menus();
    for(const auto& widget : widgets) {
        const QStringList subMenus = widgetSubMenus.value(widget);
        auto* parentMenu = menu;
        for(const auto& subMenu : subMenus) {
            if(!p->menus.contains(subMenu)) {
                auto* childMenu = new QMenu(subMenu, parentMenu);
                p->menus.insert(subMenu, childMenu);
            }
            auto* childMenu = p->menus.value(subMenu);
            parentMenu->addMenu(childMenu);
            parentMenu = childMenu;
        }
        auto* addWidget = new QAction(widget, parentMenu);
        QAction::connect(addWidget, &QAction::triggered, this, [this, widget, splitter] {
            createWidget(widget, splitter);
        });
        parentMenu->addAction(addWidget);
    }
}

void WidgetProvider::registerWidgets()
{
    auto* factory = PluginSystem::object<Widgets::WidgetFactory>();

    factory->registerClass<ControlWidget>("Controls");
    factory->registerClass<InfoWidget>("Info");
    factory->registerClass<CoverWidget>("Artwork");
    factory->registerClass<Library::PlaylistWidget>("Playlist");
    factory->registerClass<Widgets::Spacer>("Spacer");
    factory->registerClass<VerticalSplitterWidget>("Vertical", {"Splitter"});
    factory->registerClass<HoriztonalSplitterWidget>("Horiztonal", {"Splitter"});
    factory->registerClass<StatusWidget>("Status");
}
