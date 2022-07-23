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
#include "gui/widgets/dummy.h"
#include "gui/widgets/spacer.h"
#include "gui/widgets/splitterwidget.h"
#include "library/librarymanager.h"
#include "utils/enumhelper.h"
#include "utils/utils.h"
#include "widgetfactory.h"

#include <QMenu>

struct WidgetProvider::Private
{
    PlayerManager* playerManager;
    Library::LibraryManager* libraryManager;
    Library::MusicLibrary* library;
    SettingsDialog* settingsDialog;

    QMap<QString, QMenu*> menus;

    QList<Library::FilterWidget*> filters;

    Private(PlayerManager* playerManager, Library::LibraryManager* libraryManager, Library::MusicLibrary* library,
            SettingsDialog* settingsDialog)
        : playerManager(playerManager)
        , libraryManager(libraryManager)
        , library(library)
        , settingsDialog(settingsDialog)
    { }
};

WidgetProvider::WidgetProvider(PlayerManager* playerManager, Library::LibraryManager* libraryManager,
                               Library::MusicLibrary* library, SettingsDialog* settingsDialog, QObject* parent)
    : QObject(parent)
    , p(std::make_unique<Private>(playerManager, libraryManager, library, settingsDialog))
{ }

WidgetProvider::~WidgetProvider() = default;

PlayerManager* WidgetProvider::playerManager() const
{
    return p->playerManager;
}

Library::LibraryManager* WidgetProvider::libraryManager() const
{
    return p->libraryManager;
}

Library::MusicLibrary* WidgetProvider::library() const
{
    return p->library;
}

SettingsDialog* WidgetProvider::settingsDialog() const
{
    return p->settingsDialog;
}

Widget* WidgetProvider::createWidget(const QString& widget, SplitterWidget* splitter)
{
    auto* createdWidget = Util::factory()->make(widget, this);
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
    auto* splitter = new SplitterWidget(this, parent);
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
    auto widgets = Util::factory()->widgetNames();
    auto widgetSubMenus = Util::factory()->menus();
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
