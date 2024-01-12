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

#include <gui/widgetprovider.h>

#include <gui/fywidget.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QAction>
#include <QMenu>

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

bool atLimit(const FactoryWidget& widget)
{
    return widget.limit > 0 && widget.count >= widget.limit;
}
} // namespace

namespace Fooyin {
struct WidgetProvider::Private
{
    ActionManager* actionManager;

    std::map<QString, FactoryWidget> widgets;

    explicit Private(ActionManager* actionManager)
        : actionManager{actionManager}
    { }
};

WidgetProvider::WidgetProvider(ActionManager* actionManager)
    : p{std::make_unique<Private>(actionManager)}
{ }

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

    if(!widget.instantiator || atLimit(widget)) {
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

void WidgetProvider::setupWidgetMenu(ActionContainer* menu, const std::function<void(FyWidget*)>& func)
{
    if(!menu->isEmpty()) {
        return;
    }

    for(const auto& [key, widget] : p->widgets) {
        auto* parentMenu = menu;

        for(const auto& subMenu : widget.subMenus) {
            const Id id     = Id{menu->id()}.append(subMenu);
            auto* childMenu = p->actionManager->actionContainer(id);

            if(!childMenu) {
                childMenu = p->actionManager->createMenu(id);
                childMenu->menu()->setTitle(subMenu);
                parentMenu->addMenu(childMenu);
            }
            parentMenu = childMenu;
        }

        auto* addWidgetAction = new QAction(widget.name, parentMenu);
        addWidgetAction->setDisabled(atLimit(widget));
        QObject::connect(addWidgetAction, &QAction::triggered, menu, [this, func, key] {
            FyWidget* newWidget = createWidget(key);
            func(newWidget);
        });

        parentMenu->addAction(addWidgetAction);
    }
}
} // namespace Fooyin
