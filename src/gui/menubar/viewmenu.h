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

#include <QObject>

#include <unordered_map>
#include <unordered_set>
#include <vector>

class QAction;

namespace Fooyin {
class ActionManager;
class Command;
class DspSettingsController;
class DspSettingsRegistry;
class SettingsManager;

class ViewMenu : public QObject
{
    Q_OBJECT

public:
    explicit ViewMenu(ActionManager* actionManager, SettingsManager* settings, QObject* parent = nullptr);

    void registerDspSettingsActions(DspSettingsRegistry* registry, DspSettingsController* controller);

Q_SIGNALS:
    void focusSearchBar();
    void openQuickSetup();
    void openPlaybackQueue();
    void openPlaylistManager();
    void openLog();
    void openScriptEditor();
    void showNowPlaying();

private:
    void refreshDspSettingsActions(DspSettingsController* controller);

    ActionManager* m_actionManager;
    SettingsManager* m_settings;

    QAction* m_dspInsertBefore;
    std::unordered_set<QString> m_registeredDspActions;
    std::unordered_map<QString, Command*> m_dspActions;
    std::vector<QString> m_dspActionOrder;
};
} // namespace Fooyin
