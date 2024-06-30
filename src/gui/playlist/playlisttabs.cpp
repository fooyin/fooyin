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
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>
#include <utils/widgets/editabletabbar.h>
#include <utils/widgets/singletabbedwidget.h>

#include <QContextMenuEvent>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QTabBar>
#include <QTimer>

#if(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#include <chrono>
#endif

using namespace std::chrono_literals;

namespace Fooyin {
struct PlaylistTabs::Private
{
    PlaylistTabs* m_self;

    ActionManager* m_actionManager;
    PlaylistController* m_playlistController;
    PlaylistHandler* m_playlistHandler;
    TrackSelectionController* m_selectionController;
    SettingsManager* m_settings;

    QVBoxLayout* m_layout;
    SingleTabbedWidget* m_tabs;
    QPointer<FyWidget> m_tabsWidget;

    QBasicTimer m_hoverTimer;
    int m_currentHoverIndex{-1};

    QIcon m_playIcon{Utils::iconFromTheme(Constants::Icons::Play)};
    QIcon m_pauseIcon{Utils::iconFromTheme(Constants::Icons::Pause)};

    Id m_lastActivePlaylist;

    Private(PlaylistTabs* self, ActionManager* actionManager, PlaylistController* playlistController,
            SettingsManager* settings)
        : m_self{self}
        , m_actionManager{actionManager}
        , m_playlistController{playlistController}
        , m_playlistHandler{m_playlistController->playlistHandler()}
        , m_selectionController{m_playlistController->selectionController()}
        , m_settings{settings}
        , m_layout{new QVBoxLayout(self)}
        , m_tabs{new SingleTabbedWidget(self)}
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setAlignment(Qt::AlignTop);

        m_tabs->setDocumentMode(true);
        m_tabs->setMovable(true);
        m_tabs->setTabsClosable(m_settings->value<Settings::Gui::Internal::PlaylistTabsCloseButton>());
        m_tabs->tabBar()->setExpanding(m_settings->value<Settings::Gui::Internal::PlaylistTabsExpand>());
        setupAddButton(m_settings->value<Settings::Gui::Internal::PlaylistTabsAddButton>());

        m_layout->addWidget(m_tabs);

        m_settings->subscribe<Settings::Gui::Internal::PlaylistTabsAddButton>(
            m_self, [this](bool enabled) { setupAddButton(enabled); });
        m_settings->subscribe<Settings::Gui::Internal::PlaylistTabsCloseButton>(
            m_self, [this](bool enabled) { m_tabs->setTabsClosable(enabled); });
        m_settings->subscribe<Settings::Gui::Internal::PlaylistTabsExpand>(m_self, [this](bool enabled) {
            m_tabs->tabBar()->setExpanding(enabled);
            m_tabs->update();
        });
    }

    void setupAddButton(bool enable)
    {
        if(enable && !m_tabs->cornerWidget(Qt::TopLeftCorner)) {
            auto* addButton = new QPushButton(Utils::iconFromTheme(Constants::Icons::Add), QStringLiteral(""), m_self);
            addButton->setFlat(true);
            QObject::connect(addButton, &QPushButton::pressed, m_self, [this]() { createEmptyPlaylist(); });
            m_tabs->setCornerWidget(addButton, Qt::TopLeftCorner);
        }
        else if(!enable && m_tabs->cornerWidget(Qt::TopLeftCorner)) {
            m_tabs->setCornerWidget(nullptr, Qt::TopLeftCorner);
        }
    }

    void tabChanged(int index) const
    {
        const Id id = m_tabs->tabBar()->tabData(index).value<Id>();
        if(id == m_playlistController->currentPlaylistId()) {
            return;
        }

        m_tabs->tabBar()->closeEditor();
        if(id.isValid()) {
            m_playlistController->changeCurrentPlaylist(id);
        }
    }

    void tabMoved(int /*from*/, int to) const
    {
        const Id id = m_tabs->tabBar()->tabData(to).value<Id>();
        if(id.isValid()) {
            m_playlistController->changePlaylistIndex(id, to);
        }
    }

    void playlistChanged(const Playlist* playlist) const
    {
        if(!playlist) {
            return;
        }

        const int count = m_tabs->tabBar()->count();
        const Id id     = playlist->id();

        for(int i{0}; i < count; ++i) {
            if(m_tabs->tabBar()->tabData(i).value<Id>() == id) {
                m_tabs->tabBar()->setCurrentIndex(i);
            }
        }
    }

    void updateTabIcon(const int i, PlayState state) const
    {
        if(state == PlayState::Playing) {
            m_tabs->tabBar()->setTabIcon(i, m_playIcon);
        }
        else if(state == PlayState::Paused) {
            m_tabs->tabBar()->setTabIcon(i, m_pauseIcon);
        }
        else {
            m_tabs->tabBar()->setTabIcon(i, {});
        }
    }

    void createEmptyPlaylist() const
    {
        if(auto* playlist = m_playlistHandler->createEmptyPlaylist()) {
            m_playlistController->changeCurrentPlaylist(playlist);
        }
    }

    void activatePlaylistChanged(const Playlist* playlist)
    {
        if(!playlist) {
            return;
        }

        const int count = m_tabs->tabBar()->count();
        const Id id     = playlist->id();

        for(int i{0}; i < count; ++i) {
            const Id tabId = m_tabs->tabBar()->tabData(i).value<Id>();

            if(tabId == id) {
                updateTabIcon(i, m_playlistController->playState());
            }
            else if(m_lastActivePlaylist.isValid() && tabId == m_lastActivePlaylist) {
                updateTabIcon(i, PlayState::Stopped);
            }
        }

        m_lastActivePlaylist = id;
    }

    void playStateChanged(PlayState state) const
    {
        if(!m_lastActivePlaylist.isValid()) {
            return;
        }

        const int count = m_tabs->tabBar()->count();

        for(int i{0}; i < count; ++i) {
            const Id tabId = m_tabs->tabBar()->tabData(i).value<Id>();

            if(tabId == m_lastActivePlaylist) {
                updateTabIcon(i, state);
            }
        }
    }

    void playlistRenamed(const Playlist* playlist) const
    {
        if(!playlist) {
            return;
        }

        const int count = m_tabs->tabBar()->count();

        for(int i{0}; i < count; ++i) {
            if(m_tabs->tabBar()->tabData(i).value<Id>() == playlist->id()) {
                m_tabs->tabBar()->setTabText(i, playlist->name());
            }
        }
    }
};

PlaylistTabs::PlaylistTabs(ActionManager* actionManager, WidgetProvider* widgetProvider,
                           PlaylistController* playlistController, SettingsManager* settings, QWidget* parent)
    : WidgetContainer{widgetProvider, settings, parent}
    , p{std::make_unique<Private>(this, actionManager, playlistController, settings)}
{
    QObject::setObjectName(PlaylistTabs::name());

    setAcceptDrops(true);

    setupTabs();

    QObject::connect(p->m_tabs->tabBar(), &EditableTabBar::middleClicked, this, [this](const int index) {
        if(index < 0) {
            p->createEmptyPlaylist();
        }
        else if(p->m_settings->value<Settings::Gui::Internal::PlaylistTabsMiddleClose>()) {
            const Id id = p->m_tabs->tabBar()->tabData(index).value<Id>();
            p->m_playlistHandler->removePlaylist(id);
        }
    });
    QObject::connect(p->m_tabs, &SingleTabbedWidget::tabCloseRequested, this, [this](const int index) {
        if(index >= 0) {
            const Id id = p->m_tabs->tabBar()->tabData(index).value<Id>();
            p->m_playlistHandler->removePlaylist(id);
        }
    });
    QObject::connect(p->m_tabs, &SingleTabbedWidget::tabBarDoubleClicked, this, [this](const int index) {
        if(index < 0) {
            if(auto* playlist = p->m_playlistHandler->createEmptyPlaylist()) {
                p->m_playlistController->changeCurrentPlaylist(playlist);
            }
        }
    });
    QObject::connect(p->m_tabs->tabBar(), &EditableTabBar::tabTextChanged, this,
                     [this](int index, const QString& text) {
                         const Id id = p->m_tabs->tabBar()->tabData(index).value<Id>();
                         p->m_playlistHandler->renamePlaylist(id, text);
                     });
    QObject::connect(p->m_tabs, &SingleTabbedWidget::tabBarClicked, this, [this](int index) { p->tabChanged(index); });
    QObject::connect(p->m_tabs, &SingleTabbedWidget::tabMoved, this,
                     [this](int from, int to) { p->tabMoved(from, to); });
    QObject::connect(p->m_playlistController, &PlaylistController::playlistsLoaded, this,
                     [this]() { p->playlistChanged(p->m_playlistController->currentPlaylist()); });
    QObject::connect(
        p->m_playlistController, &PlaylistController::currentPlaylistChanged, this,
        [this](const Playlist* /*prevPlaylist*/, const Playlist* playlist) { p->playlistChanged(playlist); });
    QObject::connect(p->m_playlistController->playerController(), &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->playStateChanged(state); });
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     [this](const Playlist* playlist) { p->activatePlaylistChanged(playlist); });
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistAdded, this, &PlaylistTabs::addPlaylist);
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistRemoved, this, &PlaylistTabs::removePlaylist);
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistRenamed, this,
                     [this](const Playlist* playlist) { p->playlistRenamed(playlist); });
}

PlaylistTabs::~PlaylistTabs() = default;

void PlaylistTabs::setupTabs()
{
    const auto& playlists = p->m_playlistHandler->playlists();
    for(const auto& playlist : playlists) {
        addPlaylist(playlist);
    }
    // Workaround for issue where QTabBar is scrolled to the right when initialised, hiding tabs before current.
    p->m_tabs->tabBar()->adjustSize();
}

int PlaylistTabs::addPlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return -1;
    }

    const int index = addNewTab(playlist->name());
    if(index >= 0) {
        p->m_tabs->tabBar()->setTabData(index, QVariant::fromValue(playlist->id()));
        if(playlist->id() == p->m_playlistController->currentPlaylistId()) {
            p->m_tabs->setCurrentIndex(index);
        }
    }

    return index;
}

void PlaylistTabs::removePlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    for(int i{0}; i < p->m_tabs->count(); ++i) {
        if(p->m_tabs->tabBar()->tabData(i).value<Id>() == playlist->id()) {
            p->m_tabs->removeTab(i);
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

    const int index = p->m_tabs->addTab(icon, name);

    return index;
}

void PlaylistTabs::contextMenuEvent(QContextMenuEvent* event)
{
    const QPoint point = event->pos();
    auto* tabBar       = p->m_tabs->tabBar();
    const int index    = tabBar->tabAt(tabBar->mapFrom(this, point));

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* createPlaylist = new QAction(tr("Add New Playlist"), menu);
    QObject::connect(createPlaylist, &QAction::triggered, this, [this]() {
        if(auto* playlist = p->m_playlistHandler->createEmptyPlaylist()) {
            p->m_playlistController->changeCurrentPlaylist(playlist);
        }
    });
    menu->addAction(createPlaylist);

    if(index >= 0) {
        const Id id = tabBar->tabData(index).value<Id>();

        auto* renamePlAction = new QAction(tr("Rename Playlist"), menu);
        QObject::connect(renamePlAction, &QAction::triggered, tabBar, &EditableTabBar::showEditor);

        auto* removePlAction = new QAction(tr("Remove Playlist"), menu);
        QObject::connect(removePlAction, &QAction::triggered, this,
                         [this, id]() { p->m_playlistHandler->removePlaylist(id); });

        menu->addAction(renamePlAction);
        menu->addAction(removePlAction);
    }

    menu->addAction(createPlaylist);

    if(index >= 0) {
        menu->addSeparator();

        const Id id = tabBar->tabData(index).value<Id>();

        auto* moveLeft = new QAction(tr("Move Left"), menu);
        moveLeft->setEnabled(index > 0);
        QObject::connect(moveLeft, &QAction::triggered, p->m_tabs,
                         [tabBar, index]() { tabBar->moveTab(index, index - 1); });

        auto* moveRight = new QAction(tr("Move Right"), menu);
        moveRight->setEnabled(index < p->m_tabs->count() - 1);
        QObject::connect(moveRight, &QAction::triggered, p->m_tabs,
                         [tabBar, index]() { tabBar->moveTab(index, index + 1); });

        menu->addAction(moveLeft);
        menu->addAction(moveRight);

        menu->addSeparator();

        if(auto* savePlaylist = p->m_actionManager->command(Constants::Actions::SavePlaylist)) {
            menu->addAction(savePlaylist->action());
        }
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
    p->m_currentHoverIndex = p->m_tabs->tabBar()->tabAt(event->position().toPoint());

    if(p->m_currentHoverIndex >= 0) {
        event->setDropAction(Qt::CopyAction);
        event->accept(p->m_tabs->tabBar()->tabRect(p->m_currentHoverIndex));

        if(!p->m_hoverTimer.isActive()) {
#if(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
            p->m_hoverTimer.start(1s, this);
#else
            p->m_hoverTimer.start(1000, this);
#endif
        }
    }
    else {
        p->m_hoverTimer.stop();
    }
}

void PlaylistTabs::dragLeaveEvent(QDragLeaveEvent* /*event*/)
{
    p->m_hoverTimer.stop();
}

void PlaylistTabs::timerEvent(QTimerEvent* event)
{
    QWidget::timerEvent(event);

    if(event->timerId() == p->m_hoverTimer.timerId()) {
        p->m_hoverTimer.stop();
        if(p->m_currentHoverIndex >= 0) {
            p->m_tabs->setCurrentIndex(p->m_currentHoverIndex);
            p->tabChanged(p->m_currentHoverIndex);
        }
    }
}

void PlaylistTabs::dropEvent(QDropEvent* event)
{
    p->m_hoverTimer.stop();

    const QPoint point = event->position().toPoint();
    auto* tabBar       = p->m_tabs->tabBar();
    const int index    = tabBar->tabAt(tabBar->mapFrom(this, point));

    const Id id = tabBar->tabData(index).value<Id>();

    if(event->mimeData()->hasUrls()) {
        emit filesDropped(event->mimeData()->urls(), id);
        event->acceptProposedAction();
    }
    else if(event->mimeData()->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        emit tracksDropped(event->mimeData()->data(QString::fromLatin1(Constants::Mime::TrackIds)), id);
        event->acceptProposedAction();
    }
    else {
        event->ignore();
    }
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
    if(!p->m_tabsWidget) {
        return;
    }

    QJsonArray children;
    p->m_tabsWidget->saveLayout(children);

    layout[QStringLiteral("Widgets")] = children;
}

void PlaylistTabs::loadLayoutData(const QJsonObject& layout)
{
    const auto children = layout[QStringLiteral("Widgets")].toArray();

    WidgetContainer::loadWidgets(children);
}

bool PlaylistTabs::canAddWidget() const
{
    return !p->m_tabsWidget;
}

bool PlaylistTabs::canMoveWidget(int /*index*/, int /*newIndex*/) const
{
    return false;
}

int PlaylistTabs::widgetIndex(const Id& id) const
{
    if(!id.isValid() || !p->m_tabsWidget) {
        return -1;
    }

    if(p->m_tabsWidget->id() == id) {
        return 0;
    }

    return -1;
}

FyWidget* PlaylistTabs::widgetAtId(const Id& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    if(!p->m_tabsWidget || p->m_tabsWidget->id() != id) {
        return nullptr;
    }

    return p->m_tabsWidget;
}

FyWidget* PlaylistTabs::widgetAtIndex(int index) const
{
    if(index != 0) {
        return nullptr;
    }

    return p->m_tabsWidget;
}

int PlaylistTabs::widgetCount() const
{
    return p->m_tabsWidget ? 1 : 0;
}

WidgetList PlaylistTabs::widgets() const
{
    if(!p->m_tabsWidget) {
        return {};
    }

    return {p->m_tabsWidget};
}

int PlaylistTabs::addWidget(FyWidget* widget)
{
    p->m_tabsWidget = widget;
    p->m_tabs->setWidget(p->m_tabsWidget);

    return 0;
}

void PlaylistTabs::insertWidget(int index, FyWidget* widget)
{
    if(p->m_tabsWidget) {
        return;
    }

    if(index != 0) {
        return;
    }

    addWidget(widget);
}

void PlaylistTabs::removeWidget(int index)
{
    if(index == 0 && p->m_tabsWidget) {
        p->m_tabsWidget->deleteLater();
    }
}

void PlaylistTabs::replaceWidget(int index, FyWidget* newWidget)
{
    if(index != 0) {
        return;
    }

    removeWidget(index);
    p->m_tabsWidget = newWidget;
    p->m_tabs->setWidget(p->m_tabsWidget);
}

void PlaylistTabs::moveWidget(int /*index*/, int /*newIndex*/) { }
} // namespace Fooyin

#include "moc_playlisttabs.cpp"
