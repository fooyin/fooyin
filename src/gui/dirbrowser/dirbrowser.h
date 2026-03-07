/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playbackqueue.h>
#include <core/player/playerdefs.h>
#include <gui/fywidget.h>

#include <QDir>
#include <QList>
#include <QPointer>
#include <QUndoStack>

class QAction;
class QFileIconProvider;
class QFileSystemModel;
class QHBoxLayout;
class QJsonObject;
class QLineEdit;
class QModelIndex;
class QUrl;

namespace Fooyin {
class ActionManager;
class DirProxyModel;
class DirTree;
class PlaylistInteractor;
class Playlist;
class PlaylistHandler;
struct PlaylistTrack;
class SettingsManager;
class ToolButton;
class WidgetContext;
enum class TrackAction;

class DirBrowser : public FyWidget
{
    Q_OBJECT

public:
    enum class Mode : int
    {
        Tree,
        List,
    };

    DirBrowser(const QStringList& supportedExtensions, ActionManager* actionManager,
               PlaylistInteractor* playlistInteractor, SettingsManager* settings, QWidget* parent = nullptr);
    ~DirBrowser() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    void updateDir(const QString& dir);

    struct ConfigData
    {
        int doubleClickAction{0};
        int middleClickAction{0};
        bool sendPlayback{true};
        bool showIcons{true};
        bool indentList{true};
        bool showHorizScrollbar{true};
        Mode mode{Mode::List};
        bool showControls{true};
        bool showLocation{true};
        bool showSymLinks{false};
        bool showHidden{false};
        QString rootPath;
    };

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

signals:
    void rootChanged();

public slots:
    void playstateChanged(Fooyin::Player::PlayState state);
    void activePlaylistChanged(Fooyin::Playlist* playlist);
    void playlistTrackChanged(const Fooyin::PlaylistTrack& track);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void checkIconProvider();

    void handleModelUpdated() const;

    [[nodiscard]] QueueTracks loadQueueTracks(const TrackList& tracks) const;

    void handleAction(TrackAction action, bool onlySelection);
    void handlePlayAction(const QList<QUrl>& files, const QString& startingFile);
    void handleDoubleClick(const QModelIndex& index);
    void handleMiddleClick();
    void handleModelReset();

    void changeRoot(const QString& root);
    void updateIndent(bool show);
    void setDoubleClickAction(int action);
    void setMiddleClickAction(int action);
    void setSendPlayback(bool enabled);
    void setShowIconsEnabled(bool enabled);
    void setListIndentEnabled(bool enabled);
    void setShowHorizontalScrollbar(bool enabled);
    void setRootPath(const QString& rootPath);
    [[nodiscard]] QString rootPath() const;

    void setControlsEnabled(bool enabled);
    void setLocationEnabled(bool enabled);
    void setShowSymLinksEnabled(bool enabled);
    void setShowHidden(bool enabled);
    void updateFilters();
    void changeMode(Mode newMode);
    void startPlayback(const TrackList& tracks, int row);
    void updateControlState() const;
    void goUp();

    static void saveConfigToLayout(const ConfigData& config, QJsonObject& layout);
    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void openConfigDialog() override;

    QStringList m_supportedExtensions;
    ActionManager* m_actionManager;
    PlaylistInteractor* m_playlistInteractor;
    PlaylistHandler* m_playlistHandler;
    SettingsManager* m_settings;

    std::unique_ptr<QFileIconProvider> m_iconProvider;

    QHBoxLayout* m_controlLayout;
    QPointer<QLineEdit> m_dirEdit;
    QPointer<ToolButton> m_backDir;
    QPointer<ToolButton> m_forwardDir;
    QPointer<ToolButton> m_upDir;

    bool m_setup;
    Mode m_mode;
    DirTree* m_dirTree;
    QFileSystemModel* m_model;
    DirProxyModel* m_proxyModel;
    QUndoStack m_dirHistory;

    QFlags<QDir::Filter> m_defaultFilters;
    bool m_showSymLinks;
    bool m_showHidden;
    bool m_sendPlayback;
    bool m_listIndent;
    QString m_rootPath;

    Playlist* m_playlist;

    TrackAction m_doubleClickAction;
    TrackAction m_middleClickAction;

    WidgetContext* m_context;

    QAction* m_goUp;
    QAction* m_goBack;
    QAction* m_goForward;

    QAction* m_playAction;
    QAction* m_addCurrent;
    QAction* m_addActive;
    QAction* m_sendCurrent;
    QAction* m_sendNew;
    QAction* m_addQueue;
    QAction* m_queueNext;
    QAction* m_sendQueue;

    ConfigData m_config;
};
} // namespace Fooyin
