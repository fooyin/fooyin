/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "widgetprovider.h"

#include "core/gui/widgets/splitterwidget.h"
#include "widgetfactory.h"

#include <QMenu>
#include <pluginsystem/pluginmanager.h>
#include <utils/enumhelper.h>

struct WidgetProvider::Private
{
    Widgets::WidgetFactory* widgetFactory;

    QMap<QString, QMenu*> menus;

    explicit Private(Widgets::WidgetFactory* widgetFactory)
        : widgetFactory{widgetFactory}
    { }
};

WidgetProvider::WidgetProvider(Widgets::WidgetFactory* widgetFactory, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(widgetFactory)}
{ }

WidgetProvider::~WidgetProvider() = default;

FyWidget* WidgetProvider::createWidget(const QString& widget, SplitterWidget* splitter)
{
    auto* createdWidget = p->widgetFactory->make(widget);
    splitter->addWidget(createdWidget);
    return createdWidget;
}

SplitterWidget* WidgetProvider::createSplitter(Qt::Orientation type, QWidget* parent)
{
    auto* splitter = new SplitterWidget(parent);
    splitter->setOrientation(type);
    return splitter;
}

void WidgetProvider::addMenuActions(QMenu* menu, SplitterWidget* splitter)
{
    p->menus.clear();
    auto widgets = p->widgetFactory->widgetNames();
    auto widgetSubMenus = p->widgetFactory->menus();
    for(const auto& widget : widgets) {
        const QStringList subMenus = widgetSubMenus.value(widget);
        auto* parentMenu = menu;
        for(const auto& subMenu : subMenus) {
            if(!p->menus.contains(subMenu)) {
                auto* childMenu = new QMenu(subMenu, parentMenu);
                p->menus.insert(subMenu, childMenu);
            }
            auto* childMenu = p->menus.value(subMenu);
            parentMenu->addMenu(childMenu);
            parentMenu = childMenu;
        }
        auto* addWidget = new QAction(widget, parentMenu);
        QAction::connect(addWidget, &QAction::triggered, this, [this, widget, splitter] {
            createWidget(widget, splitter);
        });
        parentMenu->addAction(addWidget);
    }
}
