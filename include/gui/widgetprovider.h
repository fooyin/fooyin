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

#pragma once

#include <gui/widgetfactory.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QAction>
#include <QMenu>
#include <QObject>

namespace Fooyin {
class FyWidget;

class WidgetProvider : public QObject
{
    Q_OBJECT

public:
    explicit WidgetProvider(ActionManager* actionManager, QObject* parent = nullptr)
        : QObject{parent}
        , m_actionManager{actionManager}
        , m_widgetFactory{std::make_unique<WidgetFactory>()}
    { }

    WidgetFactory* widgetFactory()
    {
        return m_widgetFactory.get();
    }

    FyWidget* createWidget(const QString& key)
    {
        const auto widget = m_widgetFactory->widget(key);

        if(!widget || !widget->instantiator) {
            return nullptr;
        }

        if(widget->limit > 0 && m_widgetCount.contains(key) && m_widgetCount.at(key) >= widget->limit) {
            return nullptr;
        }

        m_widgetCount[key]++;
        auto* newWidget = widget->instantiator();
        QObject::connect(newWidget, &QObject::destroyed, this, [this, key]() {
            if(m_widgetCount.contains(key)) {
                m_widgetCount[key]--;
            }
        });

        return newWidget;
    }

    template <typename Func>
    void setupWidgetMenu(ActionContainer* menu, Func func)
    {
        if(!menu->isEmpty()) {
            return;
        }

        const auto widgets = m_widgetFactory->registeredWidgets();
        for(const auto& [key, widget] : widgets) {
            auto* parentMenu = menu;

            for(const auto& subMenu : widget.subMenus) {
                const Id id     = Id{menu->id()}.append(subMenu);
                auto* childMenu = m_actionManager->actionContainer(id);

                if(!childMenu) {
                    childMenu = m_actionManager->createMenu(id);
                    childMenu->menu()->setTitle(subMenu);
                    parentMenu->addMenu(childMenu);
                }
                parentMenu = childMenu;
            }

            auto* addWidgetAction = new QAction(widget.name, parentMenu);
            addWidgetAction->setDisabled(widget.limit > 0 && m_widgetCount.contains(key)
                                         && m_widgetCount.at(key) >= widget.limit);
            QObject::connect(addWidgetAction, &QAction::triggered, this, [this, func, key] {
                FyWidget* newWidget = createWidget(key);
                func(newWidget);
            });
            parentMenu->addAction(addWidgetAction);
        }
    }

private:
    ActionManager* m_actionManager;
    std::unique_ptr<WidgetFactory> m_widgetFactory;
    std::unordered_map<QString, int> m_widgetCount;
};
} // namespace Fooyin
