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

#include <QJsonArray>

namespace Fooyin {
LayoutChangeCommand::LayoutChangeCommand(WidgetProvider* provider, WidgetContainer* container)
    : m_provider{provider}
    , m_container{container}
{ }

AddWidgetCommand::AddWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QString key)
    : LayoutChangeCommand{provider, container}
    , m_key{std::move(key)}
    , m_index{-1}
{ }

AddWidgetCommand::AddWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QJsonObject widget)
    : LayoutChangeCommand{provider, container}
    , m_widget{std::move(widget)}
    , m_index{-1}
{ }

void AddWidgetCommand::undo()
{
    if(m_index >= 0) {
        m_containerState = m_container->saveState();

        m_container->removeWidget(m_index);
    }
}

void AddWidgetCommand::redo()
{
    if(!m_widget.empty()) {
        if(auto* widget = EditableLayout::loadWidget(m_provider, m_widget)) {
            m_index = m_container->addWidget(widget);
            widget->finalise();

            m_container->restoreState(m_containerState);
        }
    }
    else if(!m_key.isEmpty()) {
        if(auto* widget = m_provider->createWidget(m_key)) {
            m_index = m_container->addWidget(widget);
            widget->finalise();

            m_container->restoreState(m_containerState);
        }
    }
}

ReplaceWidgetCommand::ReplaceWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QString key,
                                           const Id& widgetToReplace)
    : LayoutChangeCommand{provider, container}
    , m_key{std::move(key)}
    , m_index{m_container->widgetIndex(widgetToReplace)}
{
    if(auto* oldWidget = m_container->widgetAtIndex(m_index)) {
        m_oldWidget = EditableLayout::saveWidget(oldWidget);
    }
}

ReplaceWidgetCommand::ReplaceWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QJsonObject widget,
                                           const Id& widgetToReplace)
    : LayoutChangeCommand{provider, container}
    , m_widget{std::move(widget)}
    , m_index{m_container->widgetIndex(widgetToReplace)}
{
    if(auto* oldWidget = m_container->widgetAtIndex(m_index)) {
        m_oldWidget = EditableLayout::saveWidget(oldWidget);
    }
}

void ReplaceWidgetCommand::undo()
{
    if(!m_oldWidget.empty()) {
        if(auto* widget = EditableLayout::loadWidget(m_provider, m_oldWidget)) {
            m_container->replaceWidget(m_index, widget);

            m_container->restoreState(m_containerState);
        }
    }
}

void ReplaceWidgetCommand::redo()
{
    if(!m_widget.empty()) {
        if(auto* widget = EditableLayout::loadWidget(m_provider, m_widget)) {
            m_containerState = m_container->saveState();

            m_container->replaceWidget(m_index, widget);
            widget->finalise();
        }
    }
    else if(!m_key.isEmpty()) {
        if(auto* widget = m_provider->createWidget(m_key)) {
            m_containerState = m_container->saveState();

            m_container->replaceWidget(m_index, widget);
            widget->finalise();
        }
    }
}

RemoveWidgetCommand::RemoveWidgetCommand(WidgetProvider* provider, WidgetContainer* container, const Id& widgetId)
    : LayoutChangeCommand{provider, container}
    , m_index{-1}
{
    if(auto* widget = container->widgetAtId(widgetId)) {
        m_widget = EditableLayout::saveWidget(widget);
        m_index  = container->widgetIndex(widgetId);
    }
}

void RemoveWidgetCommand::undo()
{
    if(!m_widget.empty()) {
        if(auto* widget = EditableLayout::loadWidget(m_provider, m_widget)) {
            m_container->insertWidget(m_index, widget);
            widget->finalise();

            m_container->restoreState(m_containerState);
        }
    }
}

void RemoveWidgetCommand::redo()
{
    if(!m_widget.empty()) {
        m_containerState = m_container->saveState();

        m_container->removeWidget(m_index);
    }
}

MoveWidgetCommand::MoveWidgetCommand(WidgetProvider* provider, WidgetContainer* container, int index, int newIndex)
    : LayoutChangeCommand{provider, container}
    , m_oldIndex{index}
    , m_index{newIndex}
{ }

void MoveWidgetCommand::undo()
{
    if(m_container->canMoveWidget(m_index, m_oldIndex)) {
        m_container->moveWidget(m_index, m_oldIndex);
    }
}

void MoveWidgetCommand::redo()
{
    if(m_container->canMoveWidget(m_oldIndex, m_index)) {
        m_container->moveWidget(m_oldIndex, m_index);
    }
}
} // namespace Fooyin
