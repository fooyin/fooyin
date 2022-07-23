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

#include "gui/controls/controlwidget.h"
#include "gui/filter/filterwidget.h"
#include "gui/info/infowidget.h"
#include "gui/library/coverwidget.h"
#include "gui/library/searchwidget.h"
#include "gui/library/statuswidget.h"
#include "gui/playlist/playlistwidget.h"
#include "gui/settings/settingsdialog.h"
#include "gui/widgets/spacer.h"
#include "gui/widgets/splitterwidget.h"
#include "library/librarymanager.h"
#include "utils/enumhelper.h"
#include "widgetfactory.h"

#include <PluginManager>
#include <QMenu>

struct WidgetProvider::Private
{
    PlayerManager* playerManager;
    Library::LibraryManager* libraryManager;
    Library::MusicLibrary* library;
    SettingsDialog* settingsDialog;
    Widgets::WidgetFactory* widgetFactory;

    QMap<QString, QMenu*> menus;

    QList<Library::FilterWidget*> filters;

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

Widget* WidgetProvider::createWidget(const QString& widget, SplitterWidget* splitter)
{
    auto* createdWidget = p->widgetFactory->make(widget);
    splitter->addWidget(createdWidget);
    return createdWidget;
}

Widget* WidgetProvider::createFilter(Filters::FilterType filterType, SplitterWidget* splitter)
{
    const int index = static_cast<int>(p->filters.size());
    auto* filter = qobject_cast<Library::FilterWidget*>(createWidget(EnumHelper::toString(filterType), splitter));
    filter->setIndex(index);
    p->filters.append(filter);
    return filter;
}

SplitterWidget* WidgetProvider::createSplitter(Qt::Orientation type, QWidget* parent)
{
    auto* splitter = new SplitterWidget(parent);
    splitter->setOrientation(type);
    return splitter;
}

void WidgetProvider::addMenuActions(QMenu* menu, SplitterWidget* splitter)
{
    //    auto* addHSplitterWidget = new QAction("Horizontal Splitter", menu);
    //    QAction::connect(addHSplitterWidget, &QAction::triggered, this, [this, splitter] {
    //        createSplitter(Qt::Horizontal, splitter);
    //    });
    //    menu->addAction(addHSplitterWidget);
    //    auto* addVSplitterWidget = new QAction("Vertical Splitter", menu);
    //    QAction::connect(addVSplitterWidget, &QAction::triggered, this, [this, splitter] {
    //        createSplitter(Qt::Vertical, splitter);
    //    });
    //    menu->addAction(addVSplitterWidget);

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

    //    addWidgetMenu(menu, splitter);
}

void WidgetProvider::registerWidgets()
{
    auto* factory = PluginSystem::object<Widgets::WidgetFactory>();

    factory->registerClass<ControlWidget>("Controls");

    factory->registerClass<Library::GenreFilter>("Genre", {"Filter"});
    factory->registerClass<Library::YearFilter>("Year", {"Filter"});
    factory->registerClass<Library::AlbmArtistFilter>("AlbumArtist", {"Filter"});
    factory->registerClass<Library::ArtistFilter>("Artist", {"Filter"});
    factory->registerClass<Library::AlbumFilter>("Album", {"Filter"});

    factory->registerClass<InfoWidget>("Info");
    factory->registerClass<CoverWidget>("Artwork");
    factory->registerClass<SearchWidget>("Search");
    factory->registerClass<Library::PlaylistWidget>("Playlist");
    factory->registerClass<Widgets::Spacer>("Spacer");
    factory->registerClass<SplitterWidget>("Splitter");
    factory->registerClass<StatusWidget>("Status");
}

// void WidgetProvider::addWidgetMenu(QMenu* menu, SplitterWidget* splitter)
//{
//     auto* widgetMenu = new QMenu("Widget", menu);

//    //    addFilterMenu(widgetMenu, splitter);

//    menu->addMenu(widgetMenu);
//}

// void WidgetProvider::addFilterMenu(QMenu* menu, SplitterWidget* splitter)
//{
//     auto* filterMenu = new QMenu("Filter", menu);

//    auto* addFilterAlbumArtist = new QAction("Album Artist", menu);
//    QAction::connect(addFilterAlbumArtist, &QAction::triggered, this, [this, splitter] {
//        createFilter(Filters::FilterType::AlbumArtist, splitter);
//    });
//    filterMenu->addAction(addFilterAlbumArtist);
//    auto* addFilterAlbum = new QAction("Album", menu);
//    QAction::connect(addFilterAlbum, &QAction::triggered, this, [this, splitter] {
//        createFilter(Filters::FilterType::Album, splitter);
//    });
//    filterMenu->addAction(addFilterAlbum);
//    auto* addFilterArtist = new QAction("Artist", menu);
//    QAction::connect(addFilterArtist, &QAction::triggered, this, [this, splitter] {
//        createFilter(Filters::FilterType::Artist, splitter);
//    });
//    filterMenu->addAction(addFilterArtist);
//    auto* addFilterGenre = new QAction("Genre", menu);
//    QAction::connect(addFilterGenre, &QAction::triggered, this, [this, splitter] {
//        createFilter(Filters::FilterType::Genre, splitter);
//    });
//    filterMenu->addAction(addFilterGenre);
//    auto* addFilterYear = new QAction("Year", menu);
//    QAction::connect(addFilterYear, &QAction::triggered, this, [this, splitter] {
//        createFilter(Filters::FilterType::Year, splitter);
//    });
//    filterMenu->addAction(addFilterYear);

//    menu->addMenu(filterMenu);
//}
