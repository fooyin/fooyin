/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy {

namespace Utils {
class ActionManager;
class SettingsManager;
} // namespace Utils

namespace Gui {
namespace Widgets {
class EditableLayout;
}

class ViewMenu : public QObject
{
    Q_OBJECT

public:
    explicit ViewMenu(Utils::ActionManager* actionManager, Widgets::EditableLayout* editableLayout,
                      Utils::SettingsManager* settings, QObject* parent = nullptr);

signals:
    void openQuickSetup();

private:
    Utils::ActionManager* m_actionManager;
    Widgets::EditableLayout* m_editableLayout;
    Utils::SettingsManager* m_settings;

    QAction* m_layoutEditing;
    QAction* m_openQuickSetup;
};
} // namespace Gui
} // namespace Fy
