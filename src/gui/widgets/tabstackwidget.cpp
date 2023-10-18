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

#include "tabstackwidget.h"
#include "gui/widgetprovider.h"

#include <QTabWidget>
#include <QVBoxLayout>

namespace Fy::Gui::Widgets {
struct TabStackWidget::Private
{
    TabStackWidget* self;
    Utils::ActionManager* actionManager;
    WidgetProvider* widgetProvider;

    std::map<int, FyWidget*> widgets;
    QTabWidget* tabs;

    Private(TabStackWidget* self, Utils::ActionManager* actionManager, WidgetProvider* widgetProvider)
        : actionManager{actionManager}
        , widgetProvider{widgetProvider}
        , tabs{new QTabWidget(self)}
    { }
};

TabStackWidget::TabStackWidget(Utils::ActionManager* actionManager, WidgetProvider* widgetProvider, QWidget* parent)
    : WidgetContainer{widgetProvider, parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider)}
{
    QObject::setObjectName(TabStackWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(p->tabs);
}

TabStackWidget::~TabStackWidget() = default;

QString TabStackWidget::name() const
{
    return "Tab Stack";
}

QString TabStackWidget::layoutName() const
{
    return "TabStack";
}

void TabStackWidget::layoutEditingMenu(Utils::ActionContainer* menu)
{
    const QString addTitle{tr("&Add")};
    auto addMenuId = id().append(addTitle);

    auto* addMenu = p->actionManager->createMenu(addMenuId);
    addMenu->menu()->setTitle(addTitle);

    p->widgetProvider->setupWidgetMenu(addMenu, [this](FyWidget* newWidget) {
        addWidget(newWidget);
    });
    menu->addMenu(addMenu);
}

void TabStackWidget::saveLayout(QJsonArray& array)
{
    QJsonArray widgets;
    for(const auto& [_, widget] : p->widgets) {
        widget->saveLayout(widgets);
    }

    QJsonObject tabStack;
    tabStack[layoutName()] = widgets;
    array.append(tabStack);
}

void TabStackWidget::loadLayout(const QJsonObject& object)
{
    const auto widgets = object[layoutName()].toArray();

    WidgetContainer::loadWidgets(widgets);
}

void TabStackWidget::addWidget(FyWidget* widget)
{
    const int index = p->tabs->addTab(widget, widget->name());
    p->widgets.emplace(index, widget);
}

void TabStackWidget::removeWidget(FyWidget* widget)
{
    const auto widgetIt = std::ranges::find_if(p->widgets, [&widget](const auto& pair) {
        return pair.second == widget;
    });

    if(widgetIt != p->widgets.end()) {
        const int removeIndex = widgetIt->first;
        p->tabs->removeTab(removeIndex);
        p->widgets.erase(widgetIt);
    }
}

void TabStackWidget::replaceWidget(FyWidget* oldWidget, FyWidget* newWidget)
{
    const auto widgetIt = std::ranges::find_if(p->widgets, [&oldWidget](const auto& pair) {
        return pair.second == oldWidget;
    });

    if(widgetIt != p->widgets.end()) {
        const int replaceIndex = widgetIt->first;

        p->tabs->removeTab(replaceIndex);
        p->tabs->insertTab(replaceIndex, newWidget, newWidget->name());
        p->widgets.erase(widgetIt);
        p->widgets.emplace(replaceIndex, newWidget);
    }
}
} // namespace Fy::Gui::Widgets
