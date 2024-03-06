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

#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsmanager.h>
#include <utils/widgets/editabletabbar.h>

#include <QContextMenuEvent>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QMimeData>
#include <QTabBar>
#include <QTimer>

#if(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#include <chrono>
#endif

using namespace std::chrono_literals;

namespace Fooyin {
struct PlaylistTabs::Private
{
    ActionManager* actionManager;
    WidgetProvider* widgetProvider;
    PlaylistController* playlistController;
    PlaylistHandler* playlistHandler;
    TrackSelectionController* selectionController;
    SettingsManager* settings;

    QVBoxLayout* layout;
    EditableTabBar* tabs;
    FyWidget* tabsWidget{nullptr};

    int currentTab{-1};

    QBasicTimer hoverTimer;
    int currentHoverIndex{-1};

    Private(PlaylistTabs* self_, ActionManager* actionManager_, WidgetProvider* widgetProvider_,
            PlaylistController* playlistController_, SettingsManager* settings_)
        : actionManager{actionManager_}
        , widgetProvider{widgetProvider_}
        , playlistController{playlistController_}
        , playlistHandler{playlistController->playlistHandler()}
        , selectionController{playlistController->selectionController()}
        , settings{settings_}
        , layout{new QVBoxLayout(self_)}
        , tabs{new EditableTabBar(self_)}
    {
        layout->addWidget(tabs);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setAlignment(Qt::AlignTop);

        tabs->setMovable(true);
        tabs->setExpanding(false);

        if(settings->value<Settings::Gui::Internal::PlaylistTabsHide>()) {
            tabs->hide();
        }
    }

    void tabChanged(int index)
    {
        if(std::exchange(currentTab, index) != index) {
            tabs->closeEditor();
            const Id id = tabs->tabData(index).value<Id>();
            if(id.isValid()) {
                playlistController->changeCurrentPlaylist(id);
            }
        }
    }

    void tabMoved(int /*from*/, int to) const
    {
        const Id id = tabs->tabData(to).value<Id>();
        if(id.isValid()) {
            playlistController->changePlaylistIndex(id, to);
        }
    }

    void playlistChanged(const Playlist* playlist)
    {
        const int count = tabs->count();
        const Id id     = playlist->id();

        for(int i{0}; i < count; ++i) {
            if(tabs->tabData(i).value<Id>() == id) {
                tabs->setCurrentIndex(i);
                currentTab = i;
            }
        }
    }

    void playlistRenamed(const Playlist* playlist) const
    {
        const int count = tabs->count();

        for(int i{0}; i < count; ++i) {
            if(tabs->tabData(i).value<Id>() == playlist->id()) {
                tabs->setTabText(i, playlist->name());
            }
        }
    }
};

PlaylistTabs::PlaylistTabs(ActionManager* actionManager, WidgetProvider* widgetProvider,
                           PlaylistController* playlistController, SettingsManager* settings, QWidget* parent)
    : WidgetContainer{widgetProvider, parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider, playlistController, settings)}
{
    QObject::setObjectName(PlaylistTabs::name());

    setAcceptDrops(true);

    setupTabs();

    QObject::connect(p->tabs, &EditableTabBar::tabTextChanged, this, [this](int index, const QString& text) {
        const Id id = p->tabs->tabData(index).value<Id>();
        p->playlistHandler->renamePlaylist(id, text);
    });
    QObject::connect(p->tabs, &QTabBar::tabBarClicked, this, [this](int index) { p->tabChanged(index); });
    QObject::connect(p->tabs, &QTabBar::tabMoved, this, [this](int from, int to) { p->tabMoved(from, to); });
    QObject::connect(
        p->playlistController, &PlaylistController::currentPlaylistChanged, this,
        [this](const Playlist* /*prevPlaylist*/, const Playlist* playlist) { p->playlistChanged(playlist); });
    QObject::connect(p->playlistHandler, &PlaylistHandler::playlistAdded, this, &PlaylistTabs::addPlaylist);
    QObject::connect(p->playlistHandler, &PlaylistHandler::playlistRemoved, this, &PlaylistTabs::removePlaylist);
    QObject::connect(p->playlistHandler, &PlaylistHandler::playlistRenamed, this,
                     [this](const Playlist* playlist) { p->playlistRenamed(playlist); });

    p->settings->subscribe<Settings::Gui::Internal::PlaylistTabsHide>(
        this, [this](bool hide) { p->tabs->setVisible(!hide); });
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
    for(int i = 0; i < p->tabs->count(); ++i) {
        if(p->tabs->tabData(i).value<Id>() == playlist->id()) {
            p->tabs->removeTab(i);
        }
    }

    if(p->settings->value<Settings::Gui::Internal::PlaylistTabsHide>() && p->tabs->count() < 2) {
        p->tabs->hide();
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

    if(p->settings->value<Settings::Gui::Internal::PlaylistTabsHide>() && p->tabs->count() > 1) {
        p->tabs->show();
    }

    return index;
}

void PlaylistTabs::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* createPlaylist = new QAction(QStringLiteral("Add New Playlist"), menu);
    QObject::connect(createPlaylist, &QAction::triggered, this,
                     [this]() { p->playlistHandler->createEmptyPlaylist(); });
    menu->addAction(createPlaylist);

    const QPoint point = event->pos();
    const int index    = p->tabs->tabAt(point);
    if(index >= 0) {
        const Id id = p->tabs->tabData(index).value<Id>();

        auto* renamePlAction = new QAction(QStringLiteral("Rename Playlist"), menu);
        QObject::connect(renamePlAction, &QAction::triggered, p->tabs, &EditableTabBar::showEditor);

        auto* removePlAction = new QAction(QStringLiteral("Remove Playlist"), menu);
        QObject::connect(removePlAction, &QAction::triggered, this,
                         [this, id]() { p->playlistHandler->removePlaylist(id); });

        menu->addAction(renamePlAction);
        menu->addAction(removePlAction);
    }
    menu->addAction(createPlaylist);

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
    return QStringLiteral("Playlist Tabs");
}

QString PlaylistTabs::layoutName() const
{
    return QStringLiteral("PlaylistTabs");
}

void PlaylistTabs::layoutEditingMenu(ActionContainer* menu)
{
    if(p->tabsWidget) {
        // Can only contain 1 widget
        return;
    }

    const QString addTitle{tr("&Add")};
    auto addMenuId = id().append(addTitle);

    auto* addMenu = p->actionManager->createMenu(addMenuId);
    addMenu->menu()->setTitle(addTitle);

    p->widgetProvider->setupWidgetMenu(addMenu, [this](FyWidget* newWidget) { addWidget(newWidget); });
    menu->addMenu(addMenu);
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

WidgetList PlaylistTabs::widgets() const
{
    if(!p->tabsWidget) {
        return {};
    }

    return {p->tabsWidget};
}
} // namespace Fooyin

#include "moc_playlisttabs.cpp"
