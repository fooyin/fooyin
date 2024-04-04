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

#pragma once

#include <utils/id.h>

#include <QUndoCommand>

namespace Fooyin {
class FyWidget;
class WidgetContainer;
class WidgetProvider;

class LayoutChangeCommand : public QUndoCommand
{
public:
    LayoutChangeCommand(WidgetProvider* provider, WidgetContainer* container);

protected:
    WidgetProvider* m_provider;
    WidgetContainer* m_container;
    QByteArray m_containerState;
};

class AddWidgetCommand : public LayoutChangeCommand
{
public:
    AddWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QString widgetKey);

    void undo() override;
    void redo() override;

private:
    QString m_widgetKey;
    int m_index;
};

class ReplaceWidgetCommand : public LayoutChangeCommand
{
public:
    ReplaceWidgetCommand(WidgetProvider* provider, WidgetContainer* container, QString widgetKey,
                         const Id& widgetToReplace);

    void undo() override;
    void redo() override;

private:
    QString m_widgetKey;
    QString m_oldWidgetKey;
    int m_index;
};

class RemoveWidgetCommand : public LayoutChangeCommand
{
public:
    RemoveWidgetCommand(WidgetProvider* provider, WidgetContainer* container, const Id& widgetId);

    void undo() override;
    void redo() override;

private:
    QString m_widgetKey;
    int m_index;
};
} // namespace Fooyin
