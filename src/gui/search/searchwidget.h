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

#include "gui/fywidget.h"

class QHBoxLayout;
class QLineEdit;

namespace Fooyin {
class ActionManager;
class SettingsManager;
class WidgetContext;
class SearchController;

class SearchWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(ActionManager* actionManager, SearchController* controller, SettingsManager* settings,
                          QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    ActionManager* m_actionManager;
    SearchController* m_controller;
    SettingsManager* m_settings;

    QLineEdit* m_searchBox;
    WidgetContext* m_searchContext;
};
} // namespace Fooyin
