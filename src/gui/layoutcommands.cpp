/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "layoutcommands.h"

#include <gui/editablelayout.h>
#include <gui/widgetcontainer.h>
#include <gui/widgetprovider.h>

namespace Fooyin {
LayoutChangeCommand::LayoutChangeCommand(WidgetProvider* provider, WidgetContainer* container)
    : m_provider{provider}
    , m_container{container}
{ }

AddWidgetCommand::AddWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QString widgetKey)
    : LayoutChangeCommand{provider, container}
    , m_widgetKey{std::move(widgetKey)}
    , m_index{-1}
{ }

void AddWidgetCommand::undo()
{
    if(m_index >= 0) {
        m_containerState = m_container->saveState();

        if(auto* widget = m_container->widgetAtIndex(m_index)) {
            m_container->removeWidget(widget->id());
            m_provider->updateActionState();
        }
    }
}

void AddWidgetCommand::redo()
{
    if(auto* widget = m_provider->createWidget(m_widgetKey)) {
        m_container->addWidget(widget);
        m_index = m_container->widgetIndex(widget->id());
        widget->finalise();

        m_container->restoreState(m_containerState);
        m_provider->updateActionState();
    }
}

ReplaceWidgetCommand::ReplaceWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QString widgetKey,
                                           const Id& widgetToReplace)
    : LayoutChangeCommand{provider, container}
    , m_widgetKey{std::move(widgetKey)}
    , m_index{m_container->widgetIndex(widgetToReplace)}
{ }

void ReplaceWidgetCommand::undo()
{
    if(m_index >= 0) {
        if(auto* oldWidget = m_container->widgetAtIndex(m_index)) {
            if(auto* widget = m_provider->createWidget(m_oldWidgetKey)) {
                m_container->replaceWidget(oldWidget->id(), widget);
                widget->finalise();

                m_container->restoreState(m_containerState);
                m_provider->updateActionState();
            }
        }
    }
}

void ReplaceWidgetCommand::redo()
{
    if(m_index >= 0) {
        if(auto* oldWidget = m_container->widgetAtIndex(m_index)) {
            m_oldWidgetKey = oldWidget->layoutName();

            if(auto* widget = m_provider->createWidget(m_widgetKey)) {
                m_containerState = m_container->saveState();

                m_container->replaceWidget(oldWidget->id(), widget);
                widget->finalise();

                m_provider->updateActionState();
            }
        }
    }
}

RemoveWidgetCommand::RemoveWidgetCommand(WidgetProvider* provider, WidgetContainer* container, const Id& widgetId)
    : LayoutChangeCommand{provider, container}
    , m_index{m_container->widgetIndex(widgetId)}
{ }

void RemoveWidgetCommand::undo()
{
    if(m_index >= 0) {
        if(auto* widget = m_provider->createWidget(m_widgetKey)) {
            m_container->insertWidget(m_index, widget);
            widget->finalise();

            m_container->restoreState(m_containerState);
            m_provider->updateActionState();
        }
    }
}

void RemoveWidgetCommand::redo()
{
    if(m_index >= 0) {
        if(auto* widget = m_container->widgetAtIndex(m_index)) {
            m_containerState = m_container->saveState();
            m_widgetKey      = widget->layoutName();

            m_container->removeWidget(widget->id());

            m_provider->updateActionState();
        }
    }
}
} // namespace Fooyin
