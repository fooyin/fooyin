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

#include <gui/widgetprovider.h>

#include "layoutcommands.h"

#include <gui/fywidget.h>
#include <gui/widgetcontainer.h>

#include <QAction>
#include <QMenu>
#include <QUndoStack>

namespace {
struct FactoryWidget
{
    QString key;
    QString name;
    std::function<Fooyin::FyWidget*()> instantiator;
    QStringList subMenus;
    int limit{0};
    int count{0};
};
} // namespace

namespace Fooyin {
struct WidgetProvider::Private
{
    QUndoStack* layoutCommands{nullptr};

    std::map<QString, FactoryWidget> widgets;

    bool canCreateWidget(const QString& key)
    {
        if(!widgets.contains(key)) {
            return false;
        }

        const auto& widget = widgets.at(key);

        return widget.limit == 0 || widget.count < widget.limit;
    }

    template <typename Func>
    void setupWidgetMenu(QMenu* menu, Func&& func)
    {
        if(!menu->isEmpty()) {
            return;
        }

        std::map<QString, QMenu*> menuCache;

        for(const auto& [key, widget] : widgets) {
            auto* parentMenu = menu;

            for(const auto& subMenu : widget.subMenus) {
                if(!menuCache.contains(subMenu)) {
                    auto* childMenu = new QMenu(subMenu, menu);
                    menuCache.emplace(subMenu, childMenu);
                    parentMenu->addMenu(childMenu);
                }
                parentMenu = menuCache.at(subMenu);
            }

            auto* addWidgetAction = new QAction(widget.name, parentMenu);
            addWidgetAction->setEnabled(canCreateWidget(key));
            QObject::connect(addWidgetAction, &QAction::triggered, menu, [func, key] { func(key); });

            parentMenu->addAction(addWidgetAction);
        }
    }
};

WidgetProvider::WidgetProvider()
    : p{std::make_unique<Private>()}
{ }

WidgetProvider::~WidgetProvider() = default;

void WidgetProvider::setCommandStack(QUndoStack* layoutCommands)
{
    p->layoutCommands = layoutCommands;
}

bool WidgetProvider::registerWidget(const QString& key, std::function<FyWidget*()> instantiator,
                                    const QString& displayName)
{
    if(p->widgets.contains(key)) {
        qDebug() << "Subclass already registered";
        return false;
    }

    FactoryWidget fw;
    fw.key          = key;
    fw.name         = displayName.isEmpty() ? key : displayName;
    fw.instantiator = std::move(instantiator);

    p->widgets.emplace(key, fw);
    return true;
}

void WidgetProvider::setSubMenus(const QString& key, const QStringList& subMenus)
{
    if(!p->widgets.contains(key)) {
        qDebug() << "Subclass not registered";
        return;
    }

    p->widgets.at(key).subMenus = subMenus;
}

bool WidgetProvider::canCreateWidget(const QString& key) const
{
    return p->canCreateWidget(key);
}

void WidgetProvider::setLimit(const QString& key, int limit)
{
    if(!p->widgets.contains(key)) {
        qDebug() << "Subclass not registered";
        return;
    }

    p->widgets.at(key).limit = limit;
}

FyWidget* WidgetProvider::createWidget(const QString& key)
{
    if(!p->widgets.contains(key)) {
        return nullptr;
    }

    auto& widget = p->widgets.at(key);

    if(!widget.instantiator || !p->canCreateWidget(key)) {
        return nullptr;
    }

    widget.count++;

    auto* newWidget = widget.instantiator();

    QObject::connect(newWidget, &QObject::destroyed, newWidget, [this, key]() {
        if(p->widgets.contains(key)) {
            p->widgets.at(key).count--;
        }
    });

    return newWidget;
}

void WidgetProvider::setupAddWidgetMenu(QMenu* menu, WidgetContainer* container)
{
    if(!p->layoutCommands) {
        return;
    }

    p->setupWidgetMenu(menu, [this, container](const QString& key) {
        p->layoutCommands->push(new AddWidgetCommand(this, container, key));
    });
}

void WidgetProvider::setupReplaceWidgetMenu(QMenu* menu, WidgetContainer* container, const Id& widgetId)
{
    if(!p->layoutCommands) {
        return;
    }

    p->setupWidgetMenu(menu, [this, container, widgetId](const QString& key) {
        p->layoutCommands->push(new ReplaceWidgetCommand(this, container, key, widgetId));
    });
}
} // namespace Fooyin
