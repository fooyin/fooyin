/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgetcontainer.h>

namespace Fooyin {
class ActionManager;
class WidgetProvider;

class TabStackWidget : public WidgetContainer
{
    Q_OBJECT

public:
    TabStackWidget(ActionManager* actionManager, WidgetProvider* widgetProvider, QWidget* parent = nullptr);
    ~TabStackWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void layoutEditingMenu(ActionContainer* menu) override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    void addWidget(FyWidget* widget) override;
    void removeWidget(FyWidget* widget) override;
    void replaceWidget(FyWidget* oldWidget, FyWidget* newWidget) override;

    WidgetList widgets() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
