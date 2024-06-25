/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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
class Id;
class LayoutProvider;
class SettingsManager;
class WidgetProvider;
struct FyLayout;

class FYGUI_EXPORT EditableLayout : public QWidget
{
    Q_OBJECT

public:
    explicit EditableLayout(ActionManager* actionManager, WidgetProvider* widgetProvider,
                            LayoutProvider* layoutProvider, SettingsManager* settings, QWidget* parent = nullptr);
    ~EditableLayout() override;

    void initialise();
    FyLayout saveCurrentToLayout(const QString& name);

    [[nodiscard]] FyWidget* findWidget(const Id& id) const;
    [[nodiscard]] WidgetList findWidgetsByName(const QString& name) const;
    [[nodiscard]] WidgetList findWidgetsByFeatures(const FyWidget::Features& features) const;

    bool eventFilter(QObject* watched, QEvent* event) override;

    void changeLayout(const FyLayout& layout);
    void saveLayout();
    bool loadLayout(const FyLayout& layout);

    static QJsonObject saveWidget(FyWidget* widget);
    static QJsonObject saveBaseWidget(FyWidget* widget);
    static FyWidget* loadWidget(WidgetProvider* provider, const QJsonObject& layout);

    void showQuickSetup();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
