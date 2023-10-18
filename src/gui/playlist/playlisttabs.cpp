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

#include "playlisttabs.h"

#include "playlistcontroller.h"

#include <core/playlist/playlistmanager.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QContextMenuEvent>
#include <QInputDialog>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QTabBar>

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistTabs::Private : QObject
{
    PlaylistTabs* self;

    Utils::ActionManager* actionManager;
    WidgetProvider* widgetProvider;
    Core::Playlist::PlaylistManager* playlistHandler;
    PlaylistController* controller;

    QVBoxLayout* layout;
    QTabBar* tabs;
    FyWidget* tabsWidget{nullptr};

    Private(PlaylistTabs* self, Utils::ActionManager* actionManager, WidgetProvider* widgetProvider,
            PlaylistController* controller)
        : self{self}
        , actionManager{actionManager}
        , widgetProvider{widgetProvider}
        , playlistHandler{controller->playlistHandler()}
        , controller{controller}
        , layout{new QVBoxLayout(self)}
        , tabs{new QTabBar(self)}
    {
        layout->addWidget(tabs);
        layout->setContentsMargins(0, 0, 0, 0);

        tabs->setMovable(false);
        tabs->setExpanding(false);
    }

    void tabChanged(int index)
    {
        const int id = tabs->tabData(index).toInt();
        if(id >= 0) {
            controller->changeCurrentPlaylist(id);
        }
    }

    void playlistChanged(const Core::Playlist::Playlist& playlist)
    {
        for(int i = 0; i < tabs->count(); ++i) {
            if(tabs->tabData(i).toInt() == playlist.id()) {
                tabs->setCurrentIndex(i);
            }
        }
    }

    void playlistRenamed(const Core::Playlist::Playlist& playlist)
    {
        for(int i = 0; i < tabs->count(); ++i) {
            if(tabs->tabData(i).toInt() == playlist.id()) {
                tabs->setTabText(i, playlist.name());
            }
        }
    }
};

PlaylistTabs::PlaylistTabs(Utils::ActionManager* actionManager, WidgetProvider* widgetProvider,
                           PlaylistController* controller, QWidget* parent)
    : WidgetContainer{widgetProvider, parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider, controller)}
{
    QObject::setObjectName(PlaylistTabs::name());

    setupTabs();

    QObject::connect(p->tabs, &QTabBar::tabBarClicked, p.get(), &PlaylistTabs::Private::tabChanged);
    QObject::connect(p->controller, &PlaylistController::currentPlaylistChanged, p.get(),
                     &PlaylistTabs::Private::playlistChanged);
    //    QObject::connect(p->playlistHandler, &Core::Playlist::PlaylistHandler::playlistTracksChanged, this,
    //                     &PlaylistTabs::playlistChanged);
    QObject::connect(p->playlistHandler, &Core::Playlist::PlaylistManager::playlistAdded, this,
                     &PlaylistTabs::addPlaylist);
    QObject::connect(p->playlistHandler, &Core::Playlist::PlaylistManager::playlistRemoved, this,
                     &PlaylistTabs::removePlaylist);
    QObject::connect(p->playlistHandler, &Core::Playlist::PlaylistManager::playlistRenamed, p.get(),
                     &PlaylistTabs::Private::playlistRenamed);
}

PlaylistTabs::~PlaylistTabs() = default;

void PlaylistTabs::setupTabs()
{
    const auto& playlists = p->playlistHandler->playlists();
    for(const auto& playlist : playlists) {
        addPlaylist(playlist, false);
    }
    // Workaround for issue where QTabBar is scrolled to the right when initialised, hiding tabs before current.
    p->tabs->adjustSize();
}

int PlaylistTabs::addPlaylist(const Core::Playlist::Playlist& playlist, bool switchTo)
{
    const int index = addNewTab(playlist.name());
    if(index >= 0) {
        p->tabs->setTabData(index, playlist.id());
        if(switchTo) {
            p->tabs->setCurrentIndex(index);
        }
    }
    return index;
}

void PlaylistTabs::removePlaylist(const Core::Playlist::Playlist& playlist)
{
    for(int i = 0; i < p->tabs->count(); ++i) {
        if(p->tabs->tabData(i).toInt() == playlist.id()) {
            p->tabs->removeTab(i);
        }
    }
}

int PlaylistTabs::addNewTab(const QString& name, const QIcon& icon)
{
    if(name.isEmpty()) {
        return -1;
    }
    return p->tabs->addTab(icon, name);
}

void PlaylistTabs::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* createPlaylist = new QAction("Add New Playlist", menu);
    QObject::connect(createPlaylist, &QAction::triggered, this, [this]() {
        p->playlistHandler->createEmptyPlaylist(true);
    });
    menu->addAction(createPlaylist);

    const QPoint point = event->pos();
    const int index    = p->tabs->tabAt(point);
    if(index >= 0) {
        const int id = p->tabs->tabData(index).toInt();

        auto* renamePlAction = new QAction("Rename Playlist", menu);
        QObject::connect(renamePlAction, &QAction::triggered, this, [this, index, id]() {
            bool success       = false;
            const QString text = QInputDialog::getText(this, tr("Rename Playlist"), tr("Playlist Name:"),
                                                       QLineEdit::Normal, p->tabs->tabText(index), &success);

            if(success && !text.isEmpty()) {
                p->playlistHandler->renamePlaylist(id, text);
            }
        });

        auto* removePlAction = new QAction("Remove Playlist", menu);
        QObject::connect(removePlAction, &QAction::triggered, this, [this, id]() {
            p->playlistHandler->removePlaylist(id);
        });

        menu->addAction(renamePlAction);
        menu->addAction(removePlAction);
    }
    menu->addAction(createPlaylist);

    menu->popup(mapToGlobal(point));
}

QString PlaylistTabs::name() const
{
    return "Playlist Tabs";
}

QString PlaylistTabs::layoutName() const
{
    return "PlaylistTabs";
}

void PlaylistTabs::layoutEditingMenu(Utils::ActionContainer* menu)
{
    if(p->tabsWidget) {
        // Can only contain 1 widget
        return;
    }

    const QString addTitle{tr("&Add")};
    auto addMenuId = id().append(addTitle);

    auto* addMenu = p->actionManager->createMenu(addMenuId);
    addMenu->menu()->setTitle(addTitle);

    p->widgetProvider->setupWidgetMenu(addMenu, [this](FyWidget* newWidget) {
        addWidget(newWidget);
    });
    menu->addMenu(addMenu);
}

void PlaylistTabs::saveLayout(QJsonArray& array)
{
    QJsonArray widget;
    p->tabsWidget->saveLayout(widget);

    QJsonObject tabsObject;
    tabsObject[layoutName()] = widget;
    array.append(tabsObject);
}

void PlaylistTabs::loadLayout(const QJsonObject& object)
{
    const auto widget = object[layoutName()].toArray();

    WidgetContainer::loadWidgets(widget);
}

void PlaylistTabs::addWidget(FyWidget* widget)
{
    p->tabsWidget = widget;
    p->layout->addWidget(p->tabsWidget);
}

void PlaylistTabs::removeWidget(FyWidget* widget)
{
    if(widget == p->tabsWidget) {
        p->tabsWidget->deleteLater();
        p->tabsWidget = nullptr;
    }
}

void PlaylistTabs::replaceWidget(FyWidget* oldWidget, FyWidget* newWidget)
{
    if(oldWidget == p->tabsWidget) {
        oldWidget->deleteLater();
        p->tabsWidget = newWidget;
        p->layout->addWidget(p->tabsWidget);
    }
}
} // namespace Fy::Gui::Widgets::Playlist
