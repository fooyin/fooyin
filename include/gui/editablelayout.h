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

#include "fygui_export.h"

#include <gui/fywidget.h>

#include <QWidget>

namespace Fooyin {
class ActionManager;
class LayoutProvider;
class SettingsManager;
class WidgetProvider;
class Id;
struct Layout;

class FYGUI_EXPORT EditableLayout : public QWidget
{
    Q_OBJECT

public:
    explicit EditableLayout(ActionManager* actionManager, WidgetProvider* widgetProvider,
                            LayoutProvider* layoutProvider, SettingsManager* settings, QWidget* parent = nullptr);
    ~EditableLayout() override;

    void initialise();

    [[nodiscard]] FyWidget* findWidget(const Id& id) const;
    [[nodiscard]] WidgetList findWidgetsByFeatures(const FyWidget::Features& features) const;

    bool eventFilter(QObject* watched, QEvent* event) override;

    void changeLayout(const Layout& layout);
    void saveLayout();
    bool loadLayout(const Layout& layout);
    bool loadLayout();

    void showQuickSetup();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
