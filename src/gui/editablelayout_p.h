/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "widgets/dummy.h"

#include <gui/fywidget.h>
#include <gui/widgetcontainer.h>
#include <gui/widgets/overlaywidget.h>

#include <QJsonObject>
#include <QMenu>
#include <QPointer>
#include <QVBoxLayout>

#include <stack>

class QUndoStack;

namespace Fooyin {
class ActionManager;
class EditableLayout;
class FyLayout;
class LayoutProvider;
class SettingsManager;
class WidgetContext;
class WidgetProvider;

class RootContainer : public WidgetContainer
{
    Q_OBJECT

public:
    explicit RootContainer(WidgetProvider* provider, SettingsManager* settings, QWidget* parent = nullptr);

    void reset();

    [[nodiscard]] FyWidget* widget() const;
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    [[nodiscard]] bool canAddWidget() const override;
    [[nodiscard]] bool canMoveWidget(int index, int newIndex) const override;
    [[nodiscard]] int widgetIndex(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtId(const Id& id) const override;
    [[nodiscard]] FyWidget* widgetAtIndex(int index) const override;
    [[nodiscard]] int widgetCount() const override;
    [[nodiscard]] WidgetList widgets() const override;

    int addWidget(FyWidget* widget) override;
    void insertWidget(int index, FyWidget* widget) override;
    void removeWidget(int index) override;
    void replaceWidget(int index, FyWidget* newWidget) override;
    void moveWidget(int index, int newIndex) override;

private:
    SettingsManager* m_settings;
    QVBoxLayout* m_layout;
    QPointer<FyWidget> m_widget;
};

class EditableLayoutPrivate
{
public:
    EditableLayoutPrivate(EditableLayout* self, ActionManager* actionManager, WidgetProvider* widgetProvider,
                          LayoutProvider* layoutProvider, SettingsManager* settings);

    void setupDefault() const;
    void updateMargins() const;
    void changeEditingState(bool editing);

    void changeLayout(const FyLayout& layout) const;

    void showOverlay(FyWidget* widget) const;
    void hideOverlay() const;

    void setupAddWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* prev, FyWidget* current) const;
    bool setupMoveWidgetMenu(QMenu* menu, WidgetContainer* parent, FyWidget* current) const;
    void setupUnsplitAction(QMenu* menu, WidgetContainer* parent, FyWidget* current) const;
    void setupPasteMenu(QMenu* menu, WidgetContainer* parent, FyWidget* prev, FyWidget* current, bool isDummy);
    void setupContextMenu(FyWidget* widget, QMenu* menu);

    [[nodiscard]] WidgetList findAllWidgets() const;

    template <typename T, typename Predicate>
    [[nodiscard]] T findWidgets(const Predicate& predicate) const
    {
        if(!m_root) {
            if constexpr(std::is_same_v<T, FyWidget*>) {
                return nullptr;
            }
            return {};
        }

        T widgets;

        std::stack<FyWidget*> widgetsToCheck;
        widgetsToCheck.push(m_root);

        while(!widgetsToCheck.empty()) {
            auto* current = widgetsToCheck.top();
            widgetsToCheck.pop();

            if(!current) {
                continue;
            }

            if(predicate(current)) {
                if constexpr(std::is_same_v<T, WidgetList>) {
                    widgets.push_back(current);
                }
                else {
                    return current;
                }
            }

            if(const auto* container = qobject_cast<WidgetContainer*>(current)) {
                const auto containerWidgets = container->widgets();
                for(FyWidget* containerWidget : containerWidgets) {
                    widgetsToCheck.push(containerWidget);
                }
            }
        }

        if constexpr(std::is_same_v<T, FyWidget*>) {
            return nullptr;
        }
        return widgets;
    }

    EditableLayout* m_self;

    ActionManager* m_actionManager;
    SettingsManager* m_settings;
    WidgetProvider* m_widgetProvider;
    LayoutProvider* m_layoutProvider;

    QPointer<QMenu> m_editingMenu;
    QHBoxLayout* m_box;
    QPointer<OverlayWidget> m_overlay;
    RootContainer* m_root;
    bool m_layoutEditing;

    WidgetContext* m_editingContext;
    QJsonObject m_widgetClipboard;
    QUndoStack* m_layoutHistory;
};
} // namespace Fooyin
