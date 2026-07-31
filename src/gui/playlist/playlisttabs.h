/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#pragma once

#include <core/player/playerdefs.h>
#include <core/track.h>
#include <gui/widgetcontainer.h>

#include <QBasicTimer>
#include <QIcon>
#include <QPointer>

class QHBoxLayout;
class QVBoxLayout;

namespace Fooyin {
class Playlist;
class PlaylistController;
class PlaylistHandler;
class SettingsManager;
class SingleTabbedWidget;
class TrackSelectionController;

enum class PlaylistTabPosition : uint8_t
{
    Top = 0,
    Bottom
};

class PlaylistTabs : public WidgetContainer
{
    Q_OBJECT

public:
    struct ConfigData
    {
        PlaylistTabPosition position{PlaylistTabPosition::Top};
        bool expand{false};
        bool showAddButton{false};
        bool showClearButton{false};
        bool showCloseButton{false};
        bool closeOnMiddleClick{false};
    };

    explicit PlaylistTabs(WidgetProvider* widgetProvider, PlaylistController* playlistController,
                          TrackSelectionController* selectionController, SettingsManager* settings,
                          QWidget* parent = nullptr);

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void applyConfig(const ConfigData& config);
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;

    void setupTabs();

    int addPlaylist(const Playlist* playlist);
    void removePlaylist(const Playlist* playlist);
    int addNewTab(const QString& name);
    int addNewTab(const QString& name, const QIcon& icon);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] bool canAddWidget() const override;
    [[nodiscard]] bool canMoveWidget(int index, int newIndex) const override;
    [[nodiscard]] int widgetIndex(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtId(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtIndex(int index) const override;
    [[nodiscard]] FyWidget* widgetAtPosition(const QPoint& pos) const override;
    [[nodiscard]] QRect widgetGeometry(FyWidget* widget) const override;
    [[nodiscard]] int widgetCount() const override;
    [[nodiscard]] WidgetList widgets() const override;

    int addWidget(FyWidget* widget) override;
    void insertWidget(int index, FyWidget* widget) override;
    void removeWidget(int index) override;
    void replaceWidget(int index, FyWidget* newWidget) override;
    void moveWidget(int index, int newIndex) override;

Q_SIGNALS:
    void configChanged();
    void filesDropped(const QList<QUrl>& urls, const Fooyin::UId& playlistId);
    void tracksDropped(const QByteArray& data, const Fooyin::UId& playlistId);
    void trackListDropped(const Fooyin::TrackList& tracks, const Fooyin::UId& playlistId);
    void savePlaylistRequested(const Fooyin::UId& playlistId);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void openConfigDialog() override;

private:
    void setupConnections();
    void setupButtons();

    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);

    void tabChanged(int index) const;
    void tabMoved(int from, int to) const;

    void playlistChanged(Playlist* oldPlaylist, Playlist* playlist);
    void activePlaylistChanged(Playlist* playlist);
    void playlistRenamed(const Playlist* playlist) const;

    void playStateChanged(Player::PlayState state) const;
    void updateTabIcon(int i, Player::PlayState state) const;
    void createEmptyPlaylist() const;

    PlaylistController* m_playlistController;
    PlaylistHandler* m_playlistHandler;
    TrackSelectionController* m_selectionController;
    SettingsManager* m_settings;
    ConfigData m_config;

    QVBoxLayout* m_layout;
    SingleTabbedWidget* m_tabs;
    QPointer<FyWidget> m_tabsWidget;
    QWidget* m_buttonsWidget;
    QHBoxLayout* m_buttonsLayout;

    QBasicTimer m_hoverTimer;
    int m_currentHoverIndex;

    QIcon m_playIcon;
    QIcon m_pauseIcon;

    UId m_lastActivePlaylist;
};
} // namespace Fooyin
