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

#include <gui/fywidget.h>

class QJsonObject;

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
    void setupActions();
    void setupConnections();

    void resetModel() const;

    [[nodiscard]] bool canRemoveSelected() const;

    void handleRowsChanged() const;
    void removeSelectedTracks() const;
    void handleTracksDropped(int row, const QByteArray& mimeData) const;
    void handlePlaylistTracksDropped(int row, const QByteArray& mimeData) const;
    void handleQueueChanged();
    void handleQueueDoubleClicked(const QModelIndex& index) const;

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
    bool m_changingQueue{false};
    ConfigData m_config;

    QAction* m_remove;
    Command* m_removeCmd;

    QAction* m_clear;
    Command* m_clearCmd;
};
} // namespace Fooyin
