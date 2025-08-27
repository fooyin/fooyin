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
#include <gui/widgetcontainer.h>

#include <QWidget>

#include <stack>

namespace Fooyin {
class ActionManager;
class EditableLayoutPrivate;
class FyLayout;
class Id;
class LayoutProvider;
class SettingsManager;
class WidgetProvider;

class FYGUI_EXPORT EditableLayout : public QWidget
{
    Q_OBJECT

public:
    explicit EditableLayout(ActionManager* actionManager, WidgetProvider* widgetProvider,
                            LayoutProvider* layoutProvider, SettingsManager* settings, QWidget* parent = nullptr);
    ~EditableLayout() override;

    void initialise();
    FyLayout saveCurrentToLayout(const QString& name);

    [[nodiscard]] FyWidget* root() const;

    [[nodiscard]] FyWidget* findWidget(const Id& id) const;
    [[nodiscard]] WidgetList findWidgetsByName(const QString& name) const;
    [[nodiscard]] WidgetList findWidgetsByFeatures(const FyWidget::Features& features) const;

    template <typename T>
    [[nodiscard]] std::vector<T*> findWidgetsByType() const
    {
        if(!root()) {
            return {};
        }

        std::vector<T*> widgets;

        std::stack<FyWidget*> widgetsToCheck;
        widgetsToCheck.push(root());

        while(!widgetsToCheck.empty()) {
            auto* current = widgetsToCheck.top();
            widgetsToCheck.pop();

            if(!current) {
                continue;
            }

            if(auto* w = qobject_cast<T*>(current)) {
                widgets.push_back(w);
            }

            if(const auto* container = qobject_cast<WidgetContainer*>(current)) {
                const auto containerWidgets = container->widgets();
                for(FyWidget* containerWidget : containerWidgets) {
                    widgetsToCheck.push(containerWidget);
                }
            }
        }

        return widgets;
    }

    bool eventFilter(QObject* watched, QEvent* event) override;

    void changeLayout(const FyLayout& layout);
    void saveLayout();
    bool loadLayout(const FyLayout& layout);
    void exportLayout(QWidget* parent);

    static QJsonObject saveWidget(FyWidget* widget);
    static QJsonObject saveBaseWidget(FyWidget* widget);
    static FyWidget* loadWidget(WidgetProvider* provider, const QJsonObject& layout);

    void showQuickSetup();

signals:
    void layoutChanged();

private:
    std::unique_ptr<EditableLayoutPrivate> p;
};
} // namespace Fooyin
