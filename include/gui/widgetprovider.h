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

#pragma once

#include <gui/widgetfactory.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>

#include <QAction>
#include <QMenu>
#include <QObject>

namespace Fy::Gui::Widgets {
class FyWidget;

class WidgetProvider : public QObject
{
    Q_OBJECT

public:
    explicit WidgetProvider(Utils::ActionManager* actionManager, QObject* parent = nullptr)
        : QObject{parent}
        , m_actionManager{actionManager}
        , m_widgetFactory{std::make_unique<WidgetFactory>()}
    { }

    Widgets::WidgetFactory* widgetFactory()
    {
        return m_widgetFactory.get();
    }

    FyWidget* createWidget(const QString& widget)
    {
        FyWidget* createdWidget = m_widgetFactory->make(widget);
        return createdWidget;
    }

    template <typename Func>
    void setupWidgetMenu(Utils::ActionContainer* menu, Func func)
    {
        if(!menu->isEmpty()) {
            return;
        }

        const auto widgets = m_widgetFactory->registeredWidgets();
        for(const auto& widget : widgets) {
            auto* parentMenu = menu;

            for(const auto& subMenu : widget.second.subMenus) {
                const Utils::Id id = Utils::Id{menu->id()}.append(subMenu);
                auto* childMenu    = m_actionManager->actionContainer(id);

                if(!childMenu) {
                    childMenu = m_actionManager->createMenu(id);
                    childMenu->menu()->setTitle(subMenu);
                    parentMenu->addMenu(childMenu);
                }
                parentMenu = childMenu;
            }
            auto* addWidgetAction = new QAction(widget.second.name, parentMenu);
            QObject::connect(addWidgetAction, &QAction::triggered, this, [this, func, widget] {
                FyWidget* newWidget = m_widgetFactory->make(widget.first);
                func(newWidget);
            });
            parentMenu->addAction(addWidgetAction);
        }
    }

private:
    Utils::ActionManager* m_actionManager;
    std::unique_ptr<WidgetFactory> m_widgetFactory;
};
} // namespace Fy::Gui::Widgets
