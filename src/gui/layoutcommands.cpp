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
LayoutChangeCommand::LayoutChangeCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container)
    : m_layout{layout}
    , m_provider{provider}
    , m_container{container}
    , m_containerId{container->id()}
{ }

void LayoutChangeCommand::checkContainer()
{
    if(!m_container) {
        if(auto* container = qobject_cast<WidgetContainer*>(m_layout->findWidget(m_containerId))) {
            m_container = container;
        }
    }
}

AddWidgetCommand::AddWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                   QString key, int index)
    : LayoutChangeCommand{layout, provider, container}
    , m_key{std::move(key)}
    , m_index{index}
{ }

AddWidgetCommand::AddWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                   QJsonObject widget, int index)
    : LayoutChangeCommand{layout, provider, container}
    , m_widget{std::move(widget)}
    , m_index{index}
{ }

void AddWidgetCommand::undo()
{
    checkContainer();

    if(m_container && m_index >= 0) {
        m_containerState = m_container->saveState();

        m_container->removeWidget(m_index);
    }
}

void AddWidgetCommand::redo()
{
    if(!m_widget.empty()) {
        if(auto* widget = EditableLayout::loadWidget(m_provider, m_widget)) {
            m_container->insertWidget(m_index, widget);
            widget->finalise();

            m_container->restoreState(m_containerState);
        }
    }
    else if(!m_key.isEmpty()) {
        if(auto* widget = m_provider->createWidget(m_key)) {
            m_container->insertWidget(m_index, widget);
            widget->finalise();

            m_container->restoreState(m_containerState);
        }
    }
}

ReplaceWidgetCommand::ReplaceWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                           QString key, const Id& widgetToReplace)
    : LayoutChangeCommand{layout, provider, container}
    , m_key{std::move(key)}
    , m_index{m_container->widgetIndex(widgetToReplace)}
{
    if(auto* oldWidget = m_container->widgetAtIndex(m_index)) {
        m_oldWidget = EditableLayout::saveWidget(oldWidget);
    }
}

ReplaceWidgetCommand::ReplaceWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                           QJsonObject widget, const Id& widgetToReplace)
    : LayoutChangeCommand{layout, provider, container}
    , m_widget{std::move(widget)}
    , m_index{m_container->widgetIndex(widgetToReplace)}
{
    if(auto* oldWidget = m_container->widgetAtIndex(m_index)) {
        m_oldWidget = EditableLayout::saveWidget(oldWidget);
    }
}

void ReplaceWidgetCommand::undo()
{
    checkContainer();

    if(m_container && !m_oldWidget.empty()) {
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

SplitWidgetCommand::SplitWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                       QString key, const Id& widgetToSplit)
    : LayoutChangeCommand{layout, provider, container}
    , m_key{std::move(key)}
    , m_index{m_container->widgetIndex(widgetToSplit)}
{
    if(auto* splitWidget = m_container->widgetAtIndex(m_index)) {
        m_splitWidget = EditableLayout::saveWidget(splitWidget);
    }
}

void SplitWidgetCommand::undo()
{
    checkContainer();

    if(m_container && !m_splitWidget.empty()) {
        m_container->removeWidget(m_index);

        QMetaObject::invokeMethod(
            m_container,
            [this]() {
                if(auto* splitWidget = EditableLayout::loadWidget(m_provider, m_splitWidget)) {
                    m_container->insertWidget(m_index, splitWidget);
                    m_container->restoreState(m_containerState);
                }
            },
            Qt::QueuedConnection);
    }
}

void SplitWidgetCommand::redo()
{
    if(!m_key.isEmpty()) {
        if(auto* widget = m_provider->createWidget(m_key)) {
            m_containerState = m_container->saveState();

            m_container->replaceWidget(m_index, widget);
            widget->finalise();

            if(auto* widgetContainer = qobject_cast<WidgetContainer*>(widget)) {
                QMetaObject::invokeMethod(
                    widgetContainer,
                    [this, widgetContainer]() {
                        if(auto* splitWidget = EditableLayout::loadWidget(m_provider, m_splitWidget)) {
                            widgetContainer->insertWidget(0, splitWidget);
                        }
                    },
                    Qt::QueuedConnection);
            }
        }
    }
}

RemoveWidgetCommand::RemoveWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                         const Id& widgetId)
    : LayoutChangeCommand{layout, provider, container}
    , m_index{-1}
{
    if(auto* widget = container->widgetAtId(widgetId)) {
        m_widget = EditableLayout::saveWidget(widget);
        m_index  = container->widgetIndex(widgetId);
    }
}

void RemoveWidgetCommand::undo()
{
    checkContainer();

    if(m_container && !m_widget.empty()) {
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

MoveWidgetCommand::MoveWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                     int index, int newIndex)
    : LayoutChangeCommand{layout, provider, container}
    , m_oldIndex{index}
    , m_index{newIndex}
{ }

void MoveWidgetCommand::undo()
{
    checkContainer();

    if(m_container && m_container->canMoveWidget(m_index, m_oldIndex)) {
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
