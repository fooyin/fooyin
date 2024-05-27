/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "internalguisettings.h"
#include "playlistcontroller.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>
#include <utils/widgets/editabletabbar.h>

#include <QContextMenuEvent>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QTabBar>
#include <QTimer>

#if(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#include <chrono>
#endif

using namespace std::chrono_literals;

namespace Fooyin {
struct PlaylistTabs::Private
{
    PlaylistTabs* self;

    PlaylistController* playlistController;
    PlaylistHandler* playlistHandler;
    TrackSelectionController* selectionController;
    SettingsManager* settings;

    QVBoxLayout* layout;
    EditableTabBar* tabs;
    QPointer<FyWidget> tabsWidget;

    QBasicTimer hoverTimer;
    int currentHoverIndex{-1};

    QIcon playIcon{Utils::iconFromTheme(Constants::Icons::Play)};
    QIcon pauseIcon{Utils::iconFromTheme(Constants::Icons::Pause)};

    Id lastActivePlaylist;

    Private(PlaylistTabs* self_, PlaylistController* playlistController_, SettingsManager* settings_)
        : self{self_}
        , playlistController{playlistController_}
        , playlistHandler{playlistController->playlistHandler()}
        , selectionController{playlistController->selectionController()}
        , settings{settings_}
        , layout{new QVBoxLayout(self_)}
        , tabs{new EditableTabBar(self_)}
    {
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setAlignment(Qt::AlignTop);

        tabs->setMovable(true);
        tabs->setExpanding(false);
        tabs->setAddButtonEnabled(settings->value<Settings::Gui::Internal::PlaylistTabsAddButton>());

        layout->addWidget(tabs);

        settings->subscribe<Settings::Gui::Internal::PlaylistTabsAddButton>(
            self, [this](bool enabled) { tabs->setAddButtonEnabled(enabled); });
    }

    void tabChanged(int index) const
    {
        const Id id = tabs->tabData(index).value<Id>();
        if(id == playlistController->currentPlaylistId()) {
            return;
        }

        tabs->closeEditor();
        if(id.isValid()) {
            playlistController->changeCurrentPlaylist(id);
        }
    }

    void tabMoved(int /*from*/, int to) const
    {
        const Id id = tabs->tabData(to).value<Id>();
        if(id.isValid()) {
            if(tabs->addButtonEnabled()) {
                --to;
            }
            playlistController->changePlaylistIndex(id, to);
        }
    }

    void playlistChanged(const Playlist* playlist) const
    {
        if(!playlist) {
            return;
        }

        const int count = tabs->count();
        const Id id     = playlist->id();

        for(int i{0}; i < count; ++i) {
            if(tabs->tabData(i).value<Id>() == id) {
                tabs->setCurrentIndex(i);
            }
        }
    }

    void updateTabIcon(const int i, PlayState state) const
    {
        if(state == PlayState::Playing) {
            tabs->setTabIcon(i, playIcon);
        }
        else if(state == PlayState::Paused) {
            tabs->setTabIcon(i, pauseIcon);
        }
        else {
            tabs->setTabIcon(i, {});
        }
    }

    void activatePlaylistChanged(const Playlist* playlist)
    {
        if(!playlist) {
            return;
        }

        const int count = tabs->count();
        const Id id     = playlist->id();

        for(int i{0}; i < count; ++i) {
            const Id tabId = tabs->tabData(i).value<Id>();

            if(tabId == id) {
                updateTabIcon(i, playlistController->playState());
            }
            else if(lastActivePlaylist.isValid() && tabId == lastActivePlaylist) {
                updateTabIcon(i, PlayState::Stopped);
            }
        }

        lastActivePlaylist = id;
    }

    void playStateChanged(PlayState state) const
    {
        if(!lastActivePlaylist.isValid()) {
            return;
        }

        const int count = tabs->count();

        for(int i{0}; i < count; ++i) {
            const Id tabId = tabs->tabData(i).value<Id>();

            if(tabId == lastActivePlaylist) {
                updateTabIcon(i, state);
            }
        }
    }

    void playlistRenamed(const Playlist* playlist) const
    {
        if(!playlist) {
            return;
        }

        const int count = tabs->count();

        for(int i{0}; i < count; ++i) {
            if(tabs->tabData(i).value<Id>() == playlist->id()) {
                tabs->setTabText(i, playlist->name());
            }
        }
    }
};

PlaylistTabs::PlaylistTabs(WidgetProvider* widgetProvider, PlaylistController* playlistController,
                           SettingsManager* settings, QWidget* parent)
    : WidgetContainer{widgetProvider, settings, parent}
    , p{std::make_unique<Private>(this, playlistController, settings)}
{
    QObject::setObjectName(PlaylistTabs::name());

    setAcceptDrops(true);

    setupTabs();

    QObject::connect(p->tabs, &EditableTabBar::addButtonClicked, this, [this]() {
        if(auto* playlist = p->playlistHandler->createEmptyPlaylist()) {
            p->playlistController->changeCurrentPlaylist(playlist);
        }
    });
    QObject::connect(p->tabs, &EditableTabBar::tabTextChanged, this, [this](int index, const QString& text) {
        const Id id = p->tabs->tabData(index).value<Id>();
        p->playlistHandler->renamePlaylist(id, text);
    });
    QObject::connect(p->tabs, &QTabBar::tabBarClicked, this, [this](int index) { p->tabChanged(index); });
    QObject::connect(p->tabs, &QTabBar::tabMoved, this, [this](int from, int to) { p->tabMoved(from, to); });
    QObject::connect(p->playlistController, &PlaylistController::playlistsLoaded, this,
                     [this]() { p->playlistChanged(p->playlistController->currentPlaylist()); });
    QObject::connect(
        p->playlistController, &PlaylistController::currentPlaylistChanged, this,
        [this](const Playlist* /*prevPlaylist*/, const Playlist* playlist) { p->playlistChanged(playlist); });
    QObject::connect(p->playlistController->playerController(), &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->playStateChanged(state); });
    QObject::connect(p->playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     [this](const Playlist* playlist) { p->activatePlaylistChanged(playlist); });
    QObject::connect(p->playlistHandler, &PlaylistHandler::playlistAdded, this, &PlaylistTabs::addPlaylist);
    QObject::connect(p->playlistHandler, &PlaylistHandler::playlistRemoved, this, &PlaylistTabs::removePlaylist);
    QObject::connect(p->playlistHandler, &PlaylistHandler::playlistRenamed, this,
                     [this](const Playlist* playlist) { p->playlistRenamed(playlist); });
}

PlaylistTabs::~PlaylistTabs() = default;

void PlaylistTabs::setupTabs()
{
    const auto& playlists = p->playlistHandler->playlists();
    for(const auto& playlist : playlists) {
        addPlaylist(playlist);
    }
    // Workaround for issue where QTabBar is scrolled to the right when initialised, hiding tabs before current.
    p->tabs->adjustSize();
}

int PlaylistTabs::addPlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return -1;
    }

    const int index = addNewTab(playlist->name());
    if(index >= 0) {
        p->tabs->setTabData(index, QVariant::fromValue(playlist->id()));
        if(playlist->id() == p->playlistController->currentPlaylistId()) {
            p->tabs->setCurrentIndex(index);
        }
    }

    return index;
}

void PlaylistTabs::removePlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    for(int i{0}; i < p->tabs->count(); ++i) {
        if(p->tabs->tabData(i).value<Id>() == playlist->id()) {
            p->tabs->removeTab(i);
        }
    }
}

int PlaylistTabs::addNewTab(const QString& name)
{
    return addNewTab(name, {});
}

int PlaylistTabs::addNewTab(const QString& name, const QIcon& icon)
{
    if(name.isEmpty()) {
        return -1;
    }

    const int index = p->tabs->addTab(icon, name);

    return index;
}

void PlaylistTabs::contextMenuEvent(QContextMenuEvent* event)
{
    const QPoint point = event->pos();
    const int index    = p->tabs->tabAt(point);

    if(p->tabs->addButtonEnabled() && index == 0) {
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* createPlaylist = new QAction(tr("Add New Playlist"), menu);
    QObject::connect(createPlaylist, &QAction::triggered, this, [this]() {
        if(auto* playlist = p->playlistHandler->createEmptyPlaylist()) {
            p->playlistController->changeCurrentPlaylist(playlist);
        }
    });
    menu->addAction(createPlaylist);

    if(index >= 0) {
        const Id id = p->tabs->tabData(index).value<Id>();

        auto* renamePlAction = new QAction(tr("Rename Playlist"), menu);
        QObject::connect(renamePlAction, &QAction::triggered, p->tabs, &EditableTabBar::showEditor);

        auto* removePlAction = new QAction(tr("Remove Playlist"), menu);
        QObject::connect(removePlAction, &QAction::triggered, this,
                         [this, id]() { p->playlistHandler->removePlaylist(id); });

        menu->addAction(renamePlAction);
        menu->addAction(removePlAction);
    }

    menu->addAction(createPlaylist);

    if(index >= 0) {
        menu->addSeparator();

        const Id id = p->tabs->tabData(index).value<Id>();

        auto* moveLeft = new QAction(tr("Move Left"), menu);
        moveLeft->setEnabled(p->tabs->addButtonEnabled() ? index - 1 > 0 : index > 0);
        QObject::connect(moveLeft, &QAction::triggered, p->tabs,
                         [this, index]() { p->tabs->moveTab(index, index - 1); });

        auto* moveRight = new QAction(tr("Move Right"), menu);
        moveRight->setEnabled(index < p->tabs->count() - 1);
        QObject::connect(moveRight, &QAction::triggered, p->tabs,
                         [this, index]() { p->tabs->moveTab(index, index + 1); });

        menu->addAction(moveLeft);
        menu->addAction(moveRight);
    }

    menu->popup(mapToGlobal(point));
}

void PlaylistTabs::dragEnterEvent(QDragEnterEvent* event)
{
    if(event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        event->acceptProposedAction();
    }
}

void PlaylistTabs::dragMoveEvent(QDragMoveEvent* event)
{
    p->currentHoverIndex = p->tabs->tabAt(event->position().toPoint());

    if(p->currentHoverIndex >= 0) {
        event->setDropAction(Qt::CopyAction);
        event->accept(p->tabs->tabRect(p->currentHoverIndex));

        if(!p->hoverTimer.isActive()) {
#if(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
            p->hoverTimer.start(1s, this);
#else
            p->hoverTimer.start(1000, this);
#endif
        }
    }
    else {
        p->hoverTimer.stop();
    }
}

void PlaylistTabs::dragLeaveEvent(QDragLeaveEvent* /*event*/)
{
    p->hoverTimer.stop();
}

void PlaylistTabs::timerEvent(QTimerEvent* event)
{
    QWidget::timerEvent(event);

    if(event->timerId() == p->hoverTimer.timerId()) {
        p->hoverTimer.stop();
        if(p->currentHoverIndex >= 0) {
            p->tabs->setCurrentIndex(p->currentHoverIndex);
            p->tabChanged(p->currentHoverIndex);
        }
    }
}

void PlaylistTabs::dropEvent(QDropEvent* event)
{
    if(!event->mimeData()->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        event->ignore();
        return;
    }

    if(p->currentHoverIndex < 0) {
        p->selectionController->executeAction(TrackAction::SendNewPlaylist, PlaylistAction::Switch);
    }
    else {
        p->selectionController->executeAction(TrackAction::AddCurrentPlaylist);
    }

    event->acceptProposedAction();
}

QString PlaylistTabs::name() const
{
    return tr("Playlist Tabs");
}

QString PlaylistTabs::layoutName() const
{
    return QStringLiteral("PlaylistTabs");
}

void PlaylistTabs::saveLayoutData(QJsonObject& layout)
{
    if(!p->tabsWidget) {
        return;
    }

    QJsonArray children;
    p->tabsWidget->saveLayout(children);

    layout[QStringLiteral("Widgets")] = children;
}

void PlaylistTabs::loadLayoutData(const QJsonObject& layout)
{
    const auto children = layout[QStringLiteral("Widgets")].toArray();

    WidgetContainer::loadWidgets(children);
}

bool PlaylistTabs::canAddWidget() const
{
    return !p->tabsWidget;
}

bool PlaylistTabs::canMoveWidget(int /*index*/, int /*newIndex*/) const
{
    return false;
}

int PlaylistTabs::widgetIndex(const Id& id) const
{
    if(!id.isValid() || !p->tabsWidget) {
        return -1;
    }

    if(p->tabsWidget->id() == id) {
        return 0;
    }

    return -1;
}

FyWidget* PlaylistTabs::widgetAtId(const Id& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    if(!p->tabsWidget || p->tabsWidget->id() != id) {
        return nullptr;
    }

    return p->tabsWidget;
}

FyWidget* PlaylistTabs::widgetAtIndex(int index) const
{
    if(index != 0) {
        return nullptr;
    }

    return p->tabsWidget;
}

int PlaylistTabs::widgetCount() const
{
    return p->tabsWidget ? 1 : 0;
}

WidgetList PlaylistTabs::widgets() const
{
    if(!p->tabsWidget) {
        return {};
    }

    return {p->tabsWidget};
}

int PlaylistTabs::addWidget(FyWidget* widget)
{
    p->tabsWidget = widget;
    p->layout->addWidget(p->tabsWidget);

    return 0;
}

void PlaylistTabs::insertWidget(int index, FyWidget* widget)
{
    if(p->tabsWidget) {
        return;
    }

    if(index != 0) {
        return;
    }

    addWidget(widget);
}

void PlaylistTabs::removeWidget(int index)
{
    if(p->tabsWidget && index == 0) {
        p->tabsWidget->deleteLater();
    }
}

void PlaylistTabs::replaceWidget(int index, FyWidget* newWidget)
{
    if(index != 0) {
        return;
    }

    p->tabsWidget->deleteLater();
    p->tabsWidget = newWidget;
    p->layout->addWidget(p->tabsWidget);
}

void PlaylistTabs::moveWidget(int /*index*/, int /*newIndex*/) { }
} // namespace Fooyin

#include "moc_playlisttabs.cpp"
