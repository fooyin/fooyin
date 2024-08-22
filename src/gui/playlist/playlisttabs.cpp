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
#include <gui/widgets/toolbutton.h>
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
#include <QTabBar>
#include <QTimer>

#if(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
#include <chrono>
#endif

using namespace std::chrono_literals;

namespace Fooyin {
PlaylistTabs::PlaylistTabs(ActionManager* actionManager, WidgetProvider* widgetProvider,
                           PlaylistController* playlistController, SettingsManager* settings, QWidget* parent)
    : WidgetContainer{widgetProvider, settings, parent}
    , m_actionManager{actionManager}
    , m_playlistController{playlistController}
    , m_playlistHandler{m_playlistController->playlistHandler()}
    , m_selectionController{m_playlistController->selectionController()}
    , m_settings{settings}
    , m_layout{new QVBoxLayout(this)}
    , m_tabs{new SingleTabbedWidget(this)}
    , m_buttonsWidget{nullptr}
    , m_buttonsLayout{nullptr}
    , m_currentHoverIndex{-1}
    , m_playIcon{Utils::iconFromTheme(Constants::Icons::Play)}
    , m_pauseIcon{Utils::iconFromTheme(Constants::Icons::Pause)}
{
    QObject::setObjectName(PlaylistTabs::name());

    m_layout->setContentsMargins({});
    m_layout->setAlignment(Qt::AlignTop);
    m_layout->addWidget(m_tabs);

    m_tabs->setDocumentMode(true);
    m_tabs->setMovable(true);
    m_tabs->setTabsClosable(m_settings->value<Settings::Gui::Internal::PlaylistTabsCloseButton>());
    m_tabs->tabBar()->setExpanding(m_settings->value<Settings::Gui::Internal::PlaylistTabsExpand>());
    m_tabs->tabBar()->installEventFilter(this);

    setAcceptDrops(true);

    setupConnections();
    setupButtons();
    setupTabs();
}

void PlaylistTabs::setupTabs()
{
    const auto& playlists = m_playlistHandler->playlists();
    for(const auto& playlist : playlists) {
        addPlaylist(playlist);
    }
    // Workaround for issue where QTabBar is scrolled to the right when initialised, hiding tabs before current.
    m_tabs->tabBar()->adjustSize();
}

int PlaylistTabs::addPlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return -1;
    }

    const int index = addNewTab(playlist->name());
    if(index >= 0) {
        m_tabs->tabBar()->setTabData(index, QVariant::fromValue(playlist->id()));
        if(playlist->id() == m_playlistController->currentPlaylistId()) {
            m_tabs->setCurrentIndex(index);
        }
    }

    return index;
}

void PlaylistTabs::removePlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    for(int i{0}; i < m_tabs->count(); ++i) {
        if(m_tabs->tabBar()->tabData(i).value<UId>() == playlist->id()) {
            m_tabs->removeTab(i);
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

    const int index = m_tabs->addTab(icon, name);

    return index;
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
    if(!m_tabsWidget) {
        return;
    }

    QJsonArray children;
    m_tabsWidget->saveLayout(children);

    layout[u"Widgets"] = children;
}

void PlaylistTabs::loadLayoutData(const QJsonObject& layout)
{
    const auto children = layout[u"Widgets"].toArray();

    WidgetContainer::loadWidgets(children);
}

bool PlaylistTabs::canAddWidget() const
{
    return !m_tabsWidget;
}

bool PlaylistTabs::canMoveWidget(int /*index*/, int /*newIndex*/) const
{
    return false;
}

int PlaylistTabs::widgetIndex(const Id& id) const
{
    if(!id.isValid() || !m_tabsWidget) {
        return -1;
    }

    if(m_tabsWidget->id() == id) {
        return 0;
    }

    return -1;
}

FyWidget* PlaylistTabs::widgetAtId(const Id& id) const
{
    if(!id.isValid()) {
        return nullptr;
    }

    if(!m_tabsWidget || m_tabsWidget->id() != id) {
        return nullptr;
    }

    return m_tabsWidget;
}

FyWidget* PlaylistTabs::widgetAtIndex(int index) const
{
    if(index != 0) {
        return nullptr;
    }

    return m_tabsWidget;
}

int PlaylistTabs::widgetCount() const
{
    return m_tabsWidget ? 1 : 0;
}

WidgetList PlaylistTabs::widgets() const
{
    if(!m_tabsWidget) {
        return {};
    }

    return {m_tabsWidget};
}

int PlaylistTabs::addWidget(FyWidget* widget)
{
    m_tabsWidget = widget;
    m_tabs->setWidget(m_tabsWidget);

    return 0;
}

void PlaylistTabs::insertWidget(int index, FyWidget* widget)
{
    if(m_tabsWidget) {
        return;
    }

    if(index != 0) {
        return;
    }

    addWidget(widget);
}

void PlaylistTabs::removeWidget(int index)
{
    if(index == 0 && m_tabsWidget) {
        m_tabsWidget->deleteLater();
    }
}

void PlaylistTabs::replaceWidget(int index, FyWidget* newWidget)
{
    if(index != 0) {
        return;
    }

    removeWidget(index);
    m_tabsWidget = newWidget;
    m_tabs->setWidget(m_tabsWidget);
}

void PlaylistTabs::moveWidget(int /*index*/, int /*newIndex*/) { }

void PlaylistTabs::contextMenuEvent(QContextMenuEvent* event)
{
    const QPoint point = event->pos();
    auto* tabBar       = m_tabs->tabBar();
    const int index    = tabBar->tabAt(tabBar->mapFrom(this, point));

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* createPlaylist = new QAction(tr("Add new playlist"), menu);
    QObject::connect(createPlaylist, &QAction::triggered, this, [this]() {
        if(auto* playlist = m_playlistHandler->createEmptyPlaylist()) {
            m_playlistController->changeCurrentPlaylist(playlist);
        }
    });
    menu->addAction(createPlaylist);

    if(index >= 0) {
        const auto id = tabBar->tabData(index).value<UId>();

        auto* renamePlAction = new QAction(tr("Rename playlist"), menu);
        QObject::connect(renamePlAction, &QAction::triggered, tabBar, &EditableTabBar::showEditor);

        auto* removePlAction = new QAction(tr("Remove playlist"), menu);
        QObject::connect(removePlAction, &QAction::triggered, this,
                         [this, id]() { m_playlistHandler->removePlaylist(id); });

        menu->addAction(renamePlAction);
        menu->addAction(removePlAction);
    }

    menu->addAction(createPlaylist);

    if(index >= 0) {
        menu->addSeparator();

        const auto id = tabBar->tabData(index).value<UId>();

        auto* moveLeft = new QAction(tr("Move left"), menu);
        moveLeft->setEnabled(index > 0);
        QObject::connect(moveLeft, &QAction::triggered, m_tabs,
                         [tabBar, index]() { tabBar->moveTab(index, index - 1); });

        auto* moveRight = new QAction(tr("Move right"), menu);
        moveRight->setEnabled(index < m_tabs->count() - 1);
        QObject::connect(moveRight, &QAction::triggered, m_tabs,
                         [tabBar, index]() { tabBar->moveTab(index, index + 1); });

        menu->addAction(moveLeft);
        menu->addAction(moveRight);

        menu->addSeparator();

        if(auto* savePlaylist = m_actionManager->command(Constants::Actions::SavePlaylist)) {
            menu->addAction(savePlaylist->action());
        }

        menu->addSeparator();

        if(const auto* playlist = m_playlistHandler->playlistById(id)) {
            if(playlist->trackCount() > 0) {
                m_selectionController->changeSelectedTracks(playlist->tracks());

                auto* selectionMenu = new QMenu(tr("%1 contents").arg(playlist->name()), menu);
                m_selectionController->addTrackContextMenu(selectionMenu);
                menu->addMenu(selectionMenu);

                QObject::connect(menu, &QObject::destroyed, this,
                                 [this]() { m_selectionController->changeSelectedTracks({}); });
            }
        }
    }

    menu->popup(mapToGlobal(point));
}

bool PlaylistTabs::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() != QEvent::Wheel) {
        return QObject::eventFilter(watched, event);
    }

    const auto* wheelEvent = static_cast<const QWheelEvent*>(event);
    auto newIndex          = m_tabs->currentIndex();
    if(wheelEvent->angleDelta().y() < 0) {
        newIndex++;
    }
    else {
        newIndex--;
    }

    const auto highestIndex = m_tabs->tabBar()->count();
    if(newIndex < 0) {
        newIndex = highestIndex - 1;
    }
    else if(newIndex >= highestIndex) {
        newIndex = 0;
    }

    m_tabs->setCurrentIndex(newIndex);
    tabChanged(newIndex);
    return true;
}

void PlaylistTabs::dragEnterEvent(QDragEnterEvent* event)
{
    if(event->mimeData()->hasUrls() || event->mimeData()->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        event->acceptProposedAction();
    }
}

void PlaylistTabs::dragMoveEvent(QDragMoveEvent* event)
{
    m_currentHoverIndex = m_tabs->tabBar()->tabAt(event->position().toPoint());

    if(m_currentHoverIndex >= 0) {
        event->setDropAction(Qt::CopyAction);
        event->accept(m_tabs->tabBar()->tabRect(m_currentHoverIndex));

        if(!m_hoverTimer.isActive()) {
#if(QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
            m_hoverTimer.start(1s, this);
#else
            m_hoverTimer.start(1000, this);
#endif
        }
    }
    else {
        m_hoverTimer.stop();
    }
}

void PlaylistTabs::dragLeaveEvent(QDragLeaveEvent* /*event*/)
{
    m_hoverTimer.stop();
}

void PlaylistTabs::timerEvent(QTimerEvent* event)
{
    QWidget::timerEvent(event);

    if(event->timerId() == m_hoverTimer.timerId()) {
        m_hoverTimer.stop();
        if(m_currentHoverIndex >= 0) {
            m_tabs->setCurrentIndex(m_currentHoverIndex);
            tabChanged(m_currentHoverIndex);
        }
    }
}

void PlaylistTabs::dropEvent(QDropEvent* event)
{
    m_hoverTimer.stop();

    const QPoint point = event->position().toPoint();
    auto* tabBar       = m_tabs->tabBar();
    const int index    = tabBar->tabAt(tabBar->mapFrom(this, point));

    const auto id = tabBar->tabData(index).value<UId>();

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

void PlaylistTabs::setupConnections()
{
    QObject::connect(m_tabs->tabBar(), &EditableTabBar::middleClicked, this, [this](const int index) {
        if(index < 0) {
            createEmptyPlaylist();
        }
        else if(m_settings->value<Settings::Gui::Internal::PlaylistTabsMiddleClose>()) {
            const auto id = m_tabs->tabBar()->tabData(index).value<UId>();
            m_playlistHandler->removePlaylist(id);
        }
    });
    QObject::connect(m_tabs, &SingleTabbedWidget::tabCloseRequested, this, [this](const int index) {
        if(index >= 0) {
            const auto id = m_tabs->tabBar()->tabData(index).value<UId>();
            m_playlistHandler->removePlaylist(id);
        }
    });
    QObject::connect(m_tabs, &SingleTabbedWidget::tabBarDoubleClicked, this, [this](const int index) {
        if(index < 0) {
            if(auto* playlist = m_playlistHandler->createEmptyPlaylist()) {
                m_playlistController->changeCurrentPlaylist(playlist);
            }
        }
    });
    QObject::connect(m_tabs->tabBar(), &EditableTabBar::tabTextChanged, this, [this](int index, const QString& text) {
        const auto id = m_tabs->tabBar()->tabData(index).value<UId>();
        m_playlistHandler->renamePlaylist(id, text);
    });
    QObject::connect(m_tabs, &SingleTabbedWidget::tabBarClicked, this, [this](int index) { tabChanged(index); });
    QObject::connect(m_tabs, &SingleTabbedWidget::tabMoved, this, [this](int from, int to) { tabMoved(from, to); });
    QObject::connect(m_playlistController, &PlaylistController::playlistsLoaded, this,
                     [this]() { playlistChanged(nullptr, m_playlistController->currentPlaylist()); });
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistChanged, this,
                     &PlaylistTabs::playlistChanged);
    QObject::connect(m_playlistController->playerController(), &PlayerController::playStateChanged, this,
                     &PlaylistTabs::playStateChanged);
    QObject::connect(m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this,
                     &PlaylistTabs::activatePlaylistChanged);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistAdded, this, &PlaylistTabs::addPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRemoved, this, &PlaylistTabs::removePlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRenamed, this, &PlaylistTabs::playlistRenamed);

    m_settings->subscribe<Settings::Gui::Internal::PlaylistTabsAddButton>(this, &PlaylistTabs::setupButtons);
    m_settings->subscribe<Settings::Gui::Internal::PlaylistTabsClearButton>(this, &PlaylistTabs::setupButtons);
    m_settings->subscribe<Settings::Gui::Internal::PlaylistTabsCloseButton>(
        this, [this](bool enabled) { m_tabs->setTabsClosable(enabled); });
    m_settings->subscribe<Settings::Gui::Internal::PlaylistTabsExpand>(this, [this](bool enabled) {
        m_tabs->tabBar()->setExpanding(enabled);
        m_tabs->update();
    });
}

void PlaylistTabs::setupButtons()
{
    const bool hasAddButton   = m_settings->value<Settings::Gui::Internal::PlaylistTabsAddButton>();
    const bool hasClearButton = m_settings->value<Settings::Gui::Internal::PlaylistTabsClearButton>();

    m_tabs->setCornerWidget(nullptr, Qt::TopLeftCorner);
    m_buttonsWidget = nullptr;
    m_buttonsLayout = nullptr;

    if(!hasAddButton && !hasClearButton) {
        return;
    }

    m_buttonsWidget = new QWidget(this);
    m_buttonsLayout = new QHBoxLayout(m_buttonsWidget);

    if(hasAddButton) {
        auto* addButton = new ToolButton(this);
        addButton->setToolTip(tr("Add playlist"));
        addButton->setIcon(Utils::iconFromTheme(Constants::Icons::Add));
        addButton->setAutoRaise(true);
        QObject::connect(addButton, &ToolButton::pressed, this, [this]() { createEmptyPlaylist(); });
        m_buttonsLayout->addWidget(addButton);
    }
    if(hasClearButton) {
        auto* clearButton = new ToolButton(this);
        clearButton->setToolTip(tr("Clear playlist"));
        clearButton->setIcon(Utils::iconFromTheme(Constants::Icons::Clear));
        clearButton->setAutoRaise(true);
        QObject::connect(clearButton, &ToolButton::pressed, this, [this]() { clearCurrentPlaylist(); });
        m_buttonsLayout->addWidget(clearButton);
    }

    m_tabs->setCornerWidget(m_buttonsWidget, Qt::TopLeftCorner);
}

void PlaylistTabs::tabChanged(int index) const
{
    const auto id = m_tabs->tabBar()->tabData(index).value<UId>();
    if(id == m_playlistController->currentPlaylistId()) {
        return;
    }

    m_tabs->tabBar()->closeEditor();
    if(id.isValid()) {
        m_playlistController->changeCurrentPlaylist(id);
    }
}

void PlaylistTabs::tabMoved(int /*from*/, int to) const
{
    const auto id = m_tabs->tabBar()->tabData(to).value<UId>();
    if(id.isValid()) {
        m_playlistController->changePlaylistIndex(id, to);
    }
}

void PlaylistTabs::playlistChanged(Playlist* /*oldPlaylist*/, Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    const int count = m_tabs->tabBar()->count();
    const UId id    = playlist->id();

    for(int i{0}; i < count; ++i) {
        if(m_tabs->tabBar()->tabData(i).value<UId>() == id) {
            m_tabs->tabBar()->setCurrentIndex(i);
        }
    }
}

void PlaylistTabs::activatePlaylistChanged(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    const int count = m_tabs->tabBar()->count();
    const UId id    = playlist->id();

    for(int i{0}; i < count; ++i) {
        const auto tabId = m_tabs->tabBar()->tabData(i).value<UId>();

        if(tabId == id) {
            updateTabIcon(i, m_playlistController->playState());
        }
        else if(!m_lastActivePlaylist.isNull() && tabId == m_lastActivePlaylist) {
            updateTabIcon(i, Player::PlayState::Stopped);
        }
    }

    m_lastActivePlaylist = id;
}

void PlaylistTabs::updateTabIcon(int i, Player::PlayState state) const
{
    if(state == Player::PlayState::Playing) {
        m_tabs->tabBar()->setTabIcon(i, m_playIcon);
    }
    else if(state == Player::PlayState::Paused) {
        m_tabs->tabBar()->setTabIcon(i, m_pauseIcon);
    }
    else {
        m_tabs->tabBar()->setTabIcon(i, {});
    }
}

void PlaylistTabs::createEmptyPlaylist() const
{
    if(auto* playlist = m_playlistHandler->createEmptyPlaylist()) {
        m_playlistController->changeCurrentPlaylist(playlist);
    }
}

void PlaylistTabs::clearCurrentPlaylist() const
{
    const int index = m_tabs->currentIndex();
    if(index < 0) {
        return;
    }

    const auto id = m_tabs->tabBar()->tabData(index).value<UId>();
    m_playlistHandler->clearPlaylistTracks(id);
}

void PlaylistTabs::playStateChanged(Player::PlayState state) const
{
    if(!m_lastActivePlaylist.isValid()) {
        return;
    }

    const int count = m_tabs->tabBar()->count();

    for(int i{0}; i < count; ++i) {
        const auto tabId = m_tabs->tabBar()->tabData(i).value<UId>();

        if(tabId == m_lastActivePlaylist) {
            updateTabIcon(i, state);
        }
    }
}

void PlaylistTabs::playlistRenamed(const Playlist* playlist) const
{
    if(!playlist) {
        return;
    }

    const int count = m_tabs->tabBar()->count();

    for(int i{0}; i < count; ++i) {
        if(m_tabs->tabBar()->tabData(i).value<UId>() == playlist->id()) {
            m_tabs->tabBar()->setTabText(i, playlist->name());
        }
    }
}
} // namespace Fooyin

#include "moc_playlisttabs.cpp"
