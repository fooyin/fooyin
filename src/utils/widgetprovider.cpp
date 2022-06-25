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

#include "core/library/librarymanager.h"
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

#include <QMenu>

struct WidgetProvider::Private
{
    PlayerManager* playerManager;
    Library::LibraryManager* libraryManager;
    Library::MusicLibrary* library;
    SettingsDialog* settingsDialog;

    QList<Library::FilterWidget*> filters;

    Private(PlayerManager* playerManager, Library::LibraryManager* libraryManager, SettingsDialog* settingsDialog)
        : playerManager(playerManager)
        , libraryManager(libraryManager)
        , library(libraryManager->musicLibrary())
        , settingsDialog(settingsDialog)
    { }
};

WidgetProvider::WidgetProvider(PlayerManager* playerManager, Library::LibraryManager* libraryManager,
                               SettingsDialog* settingsDialog, QObject* parent)
    : QObject(parent)
{
    p = std::make_unique<Private>(playerManager, libraryManager, settingsDialog);
}

WidgetProvider::~WidgetProvider() = default;

Widget* WidgetProvider::createWidget(Widgets::WidgetType type, SplitterWidget* splitter)
{
    switch(type) {
        case(Widgets::WidgetType::Dummy): {
            auto* dummy = new Dummy(splitter);
            splitter->addToSplitter(Widgets::WidgetType::Dummy, dummy);
            return dummy;
        }
        case(Widgets::WidgetType::Filter): {
            return createFilter(Filters::FilterType::AlbumArtist, splitter);
        }
        case(Widgets::WidgetType::Playlist): {
            auto* playlist = new Library::PlaylistWidget(p->playerManager, p->libraryManager);
            connect(playlist, &Library::PlaylistWidget::openSettings, p->settingsDialog, &SettingsDialog::exec);
            splitter->addToSplitter(Widgets::WidgetType::Playlist, playlist);
            return playlist;
        }
        case(Widgets::WidgetType::Status): {
            auto* status = new StatusWidget(p->playerManager);
            splitter->addToSplitter(Widgets::WidgetType::Status, status);
            return status;
        }
        case(Widgets::WidgetType::Info): {
            auto* info = new InfoWidget(p->playerManager, p->library);
            splitter->addToSplitter(Widgets::WidgetType::Info, info);
            return info;
        }
        case(Widgets::WidgetType::Controls): {
            auto* controls = new ControlWidget(p->playerManager);
            splitter->addToSplitter(Widgets::WidgetType::Controls, controls);
            return controls;
        }
        case(Widgets::WidgetType::Artwork): {
            auto* artwork = new CoverWidget(p->playerManager, p->library);
            splitter->addToSplitter(Widgets::WidgetType::Artwork, artwork);
            return artwork;
        }
        case(Widgets::WidgetType::Search): {
            auto* search = new SearchWidget(p->library);
            splitter->addToSplitter(Widgets::WidgetType::Search, search);
            search->setFocus();
            return search;
        }
        case(Widgets::WidgetType::HorizontalSplitter): {
            auto* hSplitterWidget = createSplitter(Qt::Horizontal, splitter);
            splitter->addToSplitter(Widgets::WidgetType::HorizontalSplitter, hSplitterWidget);
            return hSplitterWidget;
        }
        case(Widgets::WidgetType::VerticalSplitter): {
            auto* vSplitterWidget = createSplitter(Qt::Vertical, splitter);
            splitter->addToSplitter(Widgets::WidgetType::VerticalSplitter, vSplitterWidget);
            return vSplitterWidget;
        }
        case(Widgets::WidgetType::Spacer): {
            auto* spacer = new Spacer();
            splitter->addToSplitter(Widgets::WidgetType::Spacer, spacer);
            return spacer;
        }
        default:
            return {};
    }
}

Widget* WidgetProvider::createFilter(Filters::FilterType filterType, SplitterWidget* splitter)
{
    const int index = static_cast<int>(p->filters.size());
    auto* filter = new Library::FilterWidget(filterType, index, p->playerManager, p->library);
    splitter->addToSplitter(Widgets::WidgetType::Filter, filter);
    p->filters.append(filter);
    return filter;
}

SplitterWidget* WidgetProvider::createSplitter(Qt::Orientation type, QWidget* parent)
{
    auto* splitter = new SplitterWidget(type, this, parent);
    return splitter;
}

void WidgetProvider::addMenuActions(QMenu* menu, SplitterWidget* splitter)
{
    auto* addHSplitterWidget = new QAction("Horizontal Splitter", menu);
    QAction::connect(addHSplitterWidget, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::HorizontalSplitter, splitter);
    });
    menu->addAction(addHSplitterWidget);
    auto* addVSplitterWidget = new QAction("Vertical Splitter", menu);
    QAction::connect(addVSplitterWidget, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::VerticalSplitter, splitter);
    });
    menu->addAction(addVSplitterWidget);

    auto* addSpacer = new QAction("Spacer", menu);
    QAction::connect(addSpacer, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::Spacer, splitter);
    });
    menu->addAction(addSpacer);

    addWidgetMenu(menu, splitter);
}

void WidgetProvider::addWidgetMenu(QMenu* menu, SplitterWidget* splitter)
{
    auto* widgetMenu = new QMenu("Widget", menu);

    auto* addArtwork = new QAction("Artwork", menu);
    QAction::connect(addArtwork, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::Artwork, splitter);
    });
    widgetMenu->addAction(addArtwork);
    auto* addInfo = new QAction("Info", menu);
    QAction::connect(addInfo, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::Info, splitter);
    });
    widgetMenu->addAction(addInfo);
    auto* addControls = new QAction("Controls", menu);
    QAction::connect(addControls, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::Controls, splitter);
    });
    widgetMenu->addAction(addControls);

    addFilterMenu(widgetMenu, splitter);

    auto* addPlayList = new QAction("Playlist", menu);
    QAction::connect(addPlayList, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::Playlist, splitter);
    });
    widgetMenu->addAction(addPlayList);
    auto* addSearch = new QAction("Search", menu);
    QAction::connect(addSearch, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::Search, splitter);
    });
    widgetMenu->addAction(addSearch);
    auto* addStatus = new QAction("Status Bar", menu);
    QAction::connect(addStatus, &QAction::triggered, splitter, [=] {
        createWidget(Widgets::WidgetType::Status, splitter);
    });
    widgetMenu->addAction(addStatus);

    menu->addMenu(widgetMenu);
}

void WidgetProvider::addFilterMenu(QMenu* menu, SplitterWidget* splitter)
{
    auto* filterMenu = new QMenu("Filter", menu);

    auto* addFilterAlbumArtist = new QAction("Album Artist", menu);
    QAction::connect(addFilterAlbumArtist, &QAction::triggered, splitter, [=] {
        createFilter(Filters::FilterType::AlbumArtist, splitter);
    });
    filterMenu->addAction(addFilterAlbumArtist);
    auto* addFilterAlbum = new QAction("Album", menu);
    QAction::connect(addFilterAlbum, &QAction::triggered, splitter, [=] {
        createFilter(Filters::FilterType::Album, splitter);
    });
    filterMenu->addAction(addFilterAlbum);
    auto* addFilterArtist = new QAction("Artist", menu);
    QAction::connect(addFilterArtist, &QAction::triggered, splitter, [=] {
        createFilter(Filters::FilterType::Artist, splitter);
    });
    filterMenu->addAction(addFilterArtist);
    auto* addFilterGenre = new QAction("Genre", menu);
    QAction::connect(addFilterGenre, &QAction::triggered, splitter, [=] {
        createFilter(Filters::FilterType::Genre, splitter);
    });
    filterMenu->addAction(addFilterGenre);
    auto* addFilterYear = new QAction("Year", menu);
    QAction::connect(addFilterYear, &QAction::triggered, splitter, [=] {
        createFilter(Filters::FilterType::Year, splitter);
    });
    filterMenu->addAction(addFilterYear);

    menu->addMenu(filterMenu);
}
