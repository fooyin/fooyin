/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "projectmpresetlibrary.h"
#include "projectmsettings.h"

#include <gui/fywidget.h>

#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>

#include <vector>

class QAction;
class QCloseEvent;
class QContextMenuEvent;
class QLabel;
class QEvent;
class QShowEvent;
class QWidget;

namespace Fooyin {
class ActionManager;
class Command;
class EngineController;
class SettingsManager;
class WidgetContext;

namespace ProjectM {
class ProjectMView;

class ProjectMWidget : public FyWidget
{
    Q_OBJECT

public:
    struct ConfigData
    {
        QString presetDir;
        ProjectMSettings settings;
    };

    ProjectMWidget(ActionManager* actionManager, EngineController* engine, SettingsManager* settings,
                   QWidget* parent = nullptr);
    ~ProjectMWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void applyConfig(const ConfigData& config);
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;

    void showDetachedWindow();

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    void openConfigDialog() override;

private:
    void setupActions();
    void updateActionState();
    void showContextMenu(const QPoint& globalPos);
    void showPresetDialog();
    void toggleFullScreen();
    void openDetachedWindow(bool fullScreen);
    void enterFullScreen();
    void leaveFullScreen();
    void saveSelectedPreset();
    void scanPresetLibrary();
    void selectRandomPreset();
    void setRememberPreset(bool remember);
    void setPresetLocked(bool locked);
    void setShuffle(bool shuffle);
    void setPresetDuration(int seconds);
    void installSplitterEventFilters();
    void beginSplitterResize();
    void endSplitterResize();
    void updateResizeSnapshotGeometry();
    [[nodiscard]] bool isWindowWidget() const;
    void saveTopLevelState();
    void loadTopLevelState();

    ProjectMView* m_view;
    QWidget* m_viewContainer;
    QLabel* m_statusLabel;
    QLabel* m_resizeSnapshot;
    QPointer<QWidget> m_fullScreenWindow;
    ActionManager* m_actionManager;
    WidgetContext* m_context;
    SettingsManager* m_settings;
    QAction* m_fullScreenAction;
    QAction* m_selectPresetAction;
    QAction* m_previousPresetAction;
    QAction* m_nextPresetAction;
    QAction* m_randomPresetAction;
    QAction* m_lockPresetAction;
    QAction* m_shufflePresetsAction;
    QAction* m_rememberPresetAction;
    Command* m_fullScreenCmd;
    Command* m_selectPresetCmd;
    Command* m_previousPresetCmd;
    Command* m_nextPresetCmd;
    Command* m_randomPresetCmd;
    Command* m_lockPresetCmd;
    Command* m_shufflePresetsCmd;
    std::vector<QPointer<QObject>> m_splitterEventObjects;
    ConfigData m_config;
    ProjectMPresetLibrary m_library;
    QString m_presetPath;
    bool m_rememberPreset;
    bool m_detachedWindowFullScreen;
    bool m_splitterResizeActive;
    bool m_topLevelStateLoaded;
};
} // namespace ProjectM
} // namespace Fooyin
