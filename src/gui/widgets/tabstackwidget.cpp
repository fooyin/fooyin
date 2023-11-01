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

#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Fy::Gui::Widgets {
struct TabStackWidget::Private
{
    Utils::ActionManager* actionManager;
    WidgetProvider* widgetProvider;

    std::vector<FyWidget*> widgets;
    QTabWidget* tabs;

    Private(TabStackWidget* self, Utils::ActionManager* actionManager, WidgetProvider* widgetProvider)
        : actionManager{actionManager}
        , widgetProvider{widgetProvider}
        , tabs{new QTabWidget(self)}
    {
        tabs->setMovable(true);
    }

    int indexOfWidget(FyWidget* widget)
    {
        auto it = std::ranges::find(std::as_const(widgets), widget);
        if(it != widgets.cend()) {
            return static_cast<int>(std::distance(widgets.cbegin(), it));
        }
        return -1;
    }
};

TabStackWidget::TabStackWidget(Utils::ActionManager* actionManager, WidgetProvider* widgetProvider, QWidget* parent)
    : WidgetContainer{widgetProvider, parent}
    , p{std::make_unique<Private>(this, actionManager, widgetProvider)}
{
    QObject::setObjectName(TabStackWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(p->tabs);

    QObject::connect(p->tabs->tabBar(), &QTabBar::tabMoved, this, [this](int from, int to) {
        if(from >= 0 && from < static_cast<int>(p->widgets.size())) {
            auto* widget = p->widgets.at(from);
            p->widgets.erase(p->widgets.begin() + from);
            p->widgets.insert(p->widgets.begin() + to, widget);
        }
    });
}

TabStackWidget::~TabStackWidget() = default;

QString TabStackWidget::name() const
{
    return QStringLiteral("Tab Stack");
}

QString TabStackWidget::layoutName() const
{
    return QStringLiteral("TabStack");
}

void TabStackWidget::layoutEditingMenu(Utils::ActionContainer* menu)
{
    const QString addTitle{tr("&Add")};
    auto addMenuId = id().append(addTitle);

    auto* addMenu = p->actionManager->createMenu(addMenuId);
    addMenu->menu()->setTitle(addTitle);

    p->widgetProvider->setupWidgetMenu(addMenu, [this](FyWidget* newWidget) { addWidget(newWidget); });
    menu->addMenu(addMenu);
}

void TabStackWidget::saveLayout(QJsonArray& array)
{
    QJsonArray widgets;
    for(FyWidget* widget : p->widgets) {
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
    p->widgets.insert(p->widgets.begin() + index, widget);
}

void TabStackWidget::removeWidget(FyWidget* widget)
{
    const int index = p->indexOfWidget(widget);
    if(index >= 0) {
        p->tabs->removeTab(index);
        p->widgets.erase(p->widgets.begin() + index);
    }
}

void TabStackWidget::replaceWidget(FyWidget* oldWidget, FyWidget* newWidget)
{
    const int index = p->indexOfWidget(oldWidget);
    if(index >= 0) {
        p->tabs->removeTab(index);
        p->tabs->insertTab(index, newWidget, newWidget->name());

        p->widgets.erase(p->widgets.begin() + index);
        p->widgets.insert(p->widgets.begin() + index, newWidget);

        p->tabs->setCurrentIndex(index);
    }
}
} // namespace Fy::Gui::Widgets
