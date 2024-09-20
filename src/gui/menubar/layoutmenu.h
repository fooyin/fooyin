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

#include <QObject>

class QAction;

namespace Fooyin {
class ActionContainer;
class ActionManager;
class Command;
class FyLayout;
class LayoutProvider;
class SettingsManager;

class LayoutMenu : public QObject
{
    Q_OBJECT

public:
    LayoutMenu(ActionManager* actionManager, LayoutProvider* layoutProvider, SettingsManager* settings,
               QObject* parent = nullptr);

    void setup();

signals:
    void changeLayout(const Fooyin::FyLayout& layout);
    void importLayout();
    void exportLayout();

private:
    void addLayout(const FyLayout& layout);

    ActionManager* m_actionManager;
    LayoutProvider* m_layoutProvider;
    SettingsManager* m_settings;

    ActionContainer* m_layoutMenu;
    QAction* m_layoutEditing;
    Command* m_layoutEditingCmd;
    QAction* m_lockSplitters;
    Command* m_lockSplittersCmd;
};
} // namespace Fooyin
