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

#include <vector>

class QAction;

namespace Fooyin {
class ActionManager;
class Command;
class SettingsManager;
class SortingRegistry;

class EditMenu : public QObject
{
    Q_OBJECT

public:
    EditMenu(ActionManager* actionManager, SortingRegistry* sortingRegistry, SettingsManager* settings,
             QObject* parent = nullptr);

Q_SIGNALS:
    void requestSearch();

private:
    struct SortPresetAction
    {
        int presetId;
        QAction* action;
    };

    void refreshSortActions();

    ActionManager* m_actionManager;
    SortingRegistry* m_sortingRegistry;
    SettingsManager* m_settings;

    QAction* m_randomiseAction;
    QAction* m_reverseAction;
    std::vector<SortPresetAction> m_sortPresetActions;
};
} // namespace Fooyin
