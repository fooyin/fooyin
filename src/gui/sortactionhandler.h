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

#include <utils/actions/widgetcontext.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <vector>

class QAction;
class QMenu;

namespace Fooyin {
class ActionManager;
class Command;
class SortingRegistry;

enum class SortScope : uint8_t
{
    SelectedOrAll = 0,
    All,
};

class SortActionHandler : public QObject
{
    Q_OBJECT

public:
    SortActionHandler(ActionManager* actionManager, SortingRegistry* sortingRegistry, Context actionContext,
                      QObject* parent = nullptr);
    ~SortActionHandler() override;

    Command* registerRandomiseAction(QAction* action, const QString& statusTip);
    Command* registerReverseAction(QAction* action, const QString& statusTip);

    void refreshPresetActions(const QString& statusTip);
    void addSortMenu(QMenu* parent, bool disabled, SortScope scope);
    void setEnabled(bool enabled) const;

Q_SIGNALS:
    void randomiseRequested(Fooyin::SortScope scope);
    void reverseRequested(Fooyin::SortScope scope);
    void sortPresetRequested(const QString& script, Fooyin::SortScope scope);
    void settingsRequested();

private:
    struct SortPresetAction
    {
        int presetId;
        QAction* action;
        Command* command;
    };

    void unregisterPresetActions();

    ActionManager* m_actionManager;
    SortingRegistry* m_sortingRegistry;
    Context m_actionContext;

    Command* m_randomiseCmd;
    Command* m_reverseCmd;
    SortScope m_scope;
    std::vector<SortPresetAction> m_sortPresetActions;
};
} // namespace Fooyin
