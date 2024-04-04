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
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

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
    WidgetProvider* self;

    ActionManager* actionManager;
    QUndoStack* layoutCommands{nullptr};

    std::map<QString, FactoryWidget> widgets;
    std::map<QString, std::vector<QPointer<QAction>>> actions;

    explicit Private(WidgetProvider* self_, ActionManager* actionManager_)
        : self{self_}
        , actionManager{actionManager_}
    { }

    bool canCreateWidget(const QString& key)
    {
        if(!widgets.contains(key)) {
            return false;
        }

        const auto& widget = widgets.at(key);

        return widget.limit == 0 || widget.count < widget.limit;
    }

    template <typename Func>
    void setupWidgetMenu(ActionContainer* menu, Func&& func)
    {
        if(!menu->isEmpty()) {
            return;
        }

        for(const auto& [key, widget] : widgets) {
            auto* parentMenu = menu;

            for(const auto& subMenu : widget.subMenus) {
                const Id id     = Id{menu->id()}.append(subMenu);
                auto* childMenu = actionManager->actionContainer(id);

                if(!childMenu) {
                    childMenu = actionManager->createMenu(id);
                    childMenu->menu()->setTitle(subMenu);
                    parentMenu->addMenu(childMenu);
                }
                parentMenu = childMenu;
            }

            auto* addWidgetAction = new QAction(widget.name, parentMenu);
            actions[key].emplace_back(addWidgetAction);

            addWidgetAction->setEnabled(canCreateWidget(key));

            QObject::connect(addWidgetAction, &QAction::triggered, menu, [func, key] { func(key); });

            parentMenu->addAction(addWidgetAction);
        }
    }
};

WidgetProvider::WidgetProvider(ActionManager* actionManager)
    : p{std::make_unique<Private>(this, actionManager)}
{ }

void WidgetProvider::setCommandStack(QUndoStack* layoutCommands)
{
    p->layoutCommands = layoutCommands;
}

WidgetProvider::~WidgetProvider() = default;

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

void WidgetProvider::setupAddWidgetMenu(ActionContainer* menu, WidgetContainer* container)
{
    if(!p->layoutCommands) {
        return;
    }

    p->setupWidgetMenu(menu, [this, container](const QString& key) {
        if(p->layoutCommands) {
            p->layoutCommands->push(new AddWidgetCommand(this, container, key));
        }
    });
}

void WidgetProvider::setupReplaceWidgetMenu(ActionContainer* menu, WidgetContainer* container, const Id& widgetId)
{
    if(!p->layoutCommands) {
        return;
    }

    p->setupWidgetMenu(menu, [this, container, widgetId](const QString& key) {
        if(p->layoutCommands) {
            p->layoutCommands->push(new ReplaceWidgetCommand(this, container, key, widgetId));
        }
    });
}

void WidgetProvider::updateActionState()
{
    for(const auto& [key, widget] : p->widgets) {
        const bool canCreate = canCreateWidget(key);
        for(const auto& action : p->actions.at(key)) {
            if(action) {
                action->setEnabled(canCreate);
            }
        }
    }
}
} // namespace Fooyin
