/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/fylayout.h>
#include <utils/id.h>

#include <QJsonObject>
#include <QPointer>
#include <QUndoCommand>

namespace Fooyin {
class EditableLayout;
class EditableLayoutPrivate;
class WidgetContainer;
class WidgetProvider;

class LayoutChangeCommand : public QUndoCommand
{
public:
    explicit LayoutChangeCommand(EditableLayout* layout);
    LayoutChangeCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container);

protected:
    bool checkContainer();

    EditableLayout* m_layout;
    WidgetProvider* m_provider;
    QPointer<WidgetContainer> m_container;
    Id m_containerId;
    QByteArray m_containerState;
};

class SwitchLayoutCommand : public LayoutChangeCommand
{
public:
    SwitchLayoutCommand(EditableLayoutPrivate* editableLayout, FyLayout layout);

    void undo() override;
    void redo() override;

private:
    EditableLayoutPrivate* m_editableLayout;
    FyLayout m_oldLayout;
    FyLayout m_newLayout;
};

class AddWidgetCommand : public LayoutChangeCommand
{
public:
    AddWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container, QString key,
                     int index);
    AddWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container, QJsonObject widget,
                     int index);

    void undo() override;
    void redo() override;

private:
    QString m_key;
    QJsonObject m_widget;
    int m_index;
};

class ReplaceWidgetCommand : public LayoutChangeCommand
{
public:
    ReplaceWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container, QString key,
                         const Id& widgetToReplace);
    ReplaceWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                         QJsonObject widget, const Id& widgetToReplace);

    void undo() override;
    void redo() override;

private:
    QString m_key;
    QJsonObject m_widget;
    QJsonObject m_oldWidget;
    int m_index;
};

class SplitWidgetCommand : public LayoutChangeCommand
{
public:
    SplitWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container, QString key,
                       const Id& widgetToSplit);

    void undo() override;
    void redo() override;

private:
    QString m_key;
    QJsonObject m_splitWidget;
    int m_index;
};

class RemoveWidgetCommand : public LayoutChangeCommand
{
public:
    RemoveWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                        const Id& widgetId);

    void undo() override;
    void redo() override;

private:
    int m_index;
    QJsonObject m_widget;
};

class CollapseContainerCommand : public LayoutChangeCommand
{
public:
    CollapseContainerCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                             const Id& containerId);

    void undo() override;
    void redo() override;

private:
    int m_index;
    QJsonObject m_collapsedContainer;
    QJsonObject m_promotedWidget;
};

class MoveWidgetCommand : public LayoutChangeCommand
{
public:
    MoveWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container, int index,
                      int newIndex);

    void undo() override;
    void redo() override;

private:
    QJsonObject m_widget;
    int m_oldIndex;
    int m_index;
};
} // namespace Fooyin
