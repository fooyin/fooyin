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
#include <core/playlist/playlist.h>
#include <gui/fywidget.h>

class QJsonObject;
class QModelIndex;

namespace Fooyin {
class ActionManager;
class Command;
class AudioLoader;
class PlayerController;
class PlaylistInteractor;
class QueueViewerModel;
class QueueViewerView;
class SettingsManager;
class WidgetContext;

class QueueViewer : public FyWidget
{
    Q_OBJECT

public:
    explicit QueueViewer(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                         std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                         QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    struct ConfigData
    {
        QString leftScript;
        QString rightScript;
        bool showCurrent{true};
        bool showIcon{true};
        QSize iconSize{36, 36};
        bool showHeader{true};
        bool showScrollBar{true};
        bool alternatingRows{false};
    };

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct ViewRowState
    {
        PlaylistTrack track;
        int occurrence{0};
        bool currentRow{false};

        [[nodiscard]] bool isValid() const
        {
            return track.isValid();
        }
    };

    struct ViewState
    {
        ViewRowState current;
        ViewRowState top;
        std::vector<ViewRowState> selection;
        int scrollValue{0};
    };

    void setupActions();
    void setupConnections();

    void resetModel() const;

    [[nodiscard]] ViewState captureViewState() const;
    void restoreViewState(const ViewState& state) const;
    [[nodiscard]] ViewRowState viewRowState(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexForViewRowState(const ViewRowState& state) const;

    [[nodiscard]] bool canRemoveSelected() const;

    void handleRowsChanged() const;
    void removeSelectedTracks() const;
    void handleQueueTracksMoved(int row, const QList<int>& indexes) const;
    void handleTracksDropped(int row, const QByteArray& mimeData) const;
    void handlePlaylistTracksDropped(int row, const QByteArray& mimeData) const;
    void handleQueueDoubleClicked(const QModelIndex& index) const;
    void replaceQueueTracks(QueueTracks tracks) const;
    void insertQueueTracks(int row, const QueueTracks& tracks) const;

    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;
    void openConfigDialog() override;

    ActionManager* m_actionManager;
    PlaylistInteractor* m_playlistInteractor;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    QueueViewerView* m_view;
    QueueViewerModel* m_model;
    WidgetContext* m_context;
    ConfigData m_config;

    QAction* m_remove;
    Command* m_removeCmd;

    QAction* m_clear;
    Command* m_clearCmd;
};
} // namespace Fooyin
