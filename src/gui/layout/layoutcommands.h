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

#include <gui/layout/fylayout.h>
#include <utils/id.h>

#include <QCoreApplication>
#include <QJsonObject>
#include <QMargins>
#include <QPointer>
#include <QUndoCommand>

#include <optional>
#include <vector>

namespace Fooyin {
class FyWidget;
class EditableLayout;
class EditableLayoutPrivate;
class WidgetContainer;
class WidgetProvider;
class SettingsManager;
class SplitterWidget;
class RootContainer;
struct LayoutDropTarget;

class LayoutChangeCommand : public QUndoCommand
{
public:
    LayoutChangeCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container);
    ~LayoutChangeCommand() override;

protected:
    bool checkContainer();
    FyWidget* takeWidget(WidgetContainer* container, int index);
    FyWidget* exchangeWidget(WidgetContainer* container, int index, FyWidget* widget);
    void insertWidget(WidgetContainer* container, int index, FyWidget* widget, bool fillPlaceholder = false);

    EditableLayout* m_layout;
    WidgetProvider* m_provider;
    QPointer<WidgetContainer> m_container;
    Id m_containerId;
    QJsonObject m_containerBefore;
    QJsonObject m_containerAfter;

private:
    std::vector<QPointer<FyWidget>> m_retainedWidgets;
};

class SetWidgetMarginsCommand : public QUndoCommand
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::SetWidgetMarginsCommand)

public:
    SetWidgetMarginsCommand(EditableLayout* layout, FyWidget* widget, std::optional<QMargins> margins,
                            const Id& session);

    [[nodiscard]] int id() const override;
    bool mergeWith(const QUndoCommand* other) override;
    void undo() override;
    void redo() override;

private:
    void apply(const std::optional<QMargins>& margins);

    QPointer<EditableLayout> m_layout;
    QPointer<FyWidget> m_widget;
    Id m_widgetId;
    Id m_session;
    std::optional<QMargins> m_before;
    std::optional<QMargins> m_after;
};

class SwitchLayoutCommand : public QUndoCommand
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
    QPointer<FyWidget> m_created;
    Id m_createdId;
    QPointer<FyWidget> m_placeholder;
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
    QPointer<FyWidget> m_oldWidget;
    Id m_oldWidgetId;
    QPointer<FyWidget> m_replacement;
    Id m_replacementId;
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
    QPointer<FyWidget> m_splitWidget;
    Id m_splitWidgetId;
    QPointer<WidgetContainer> m_splitContainer;
    Id m_splitContainerId;
    QJsonObject m_splitContainerState;
    int m_splitIndex;
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
    QPointer<FyWidget> m_widget;
    Id m_widgetId;
    QPointer<FyWidget> m_placeholder;
    Id m_placeholderId;
};

class CollapseContainerCommand : public LayoutChangeCommand
{
public:
    CollapseContainerCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                             const Id& containerId);

    void undo() override;
    void redo() override;

private:
    QPointer<WidgetContainer> m_collapsedContainer;
    Id m_collapsedContainerId;
    QPointer<FyWidget> m_promotedWidget;
    Id m_promotedWidgetId;
    int m_promotedIndex;
    QJsonObject m_collapsedState;
};

class TransferWidgetCommand : public QUndoCommand
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::TransferWidgetCommand)

public:
    TransferWidgetCommand(WidgetContainer* source, int index, WidgetContainer* destination, int destinationIndex,
                          bool replaceTarget = false, EditableLayout* layout = nullptr,
                          WidgetProvider* provider = nullptr);
    ~TransferWidgetCommand() override;

    static bool canTransfer(WidgetContainer* source, int index, WidgetContainer* destination, int destinationIndex);
    static bool canReplace(WidgetContainer* source, int index, WidgetContainer* destination, int destinationIndex);

    void undo() override;
    void redo() override;

private:
    struct CollapsedSplitter
    {
        QPointer<SplitterWidget> splitter;
        QPointer<WidgetContainer> parent;
        QPointer<FyWidget> promoted;
        Id splitterId;
        Id parentId;
        Id promotedId;
        QJsonObject parentState;
        QJsonObject parentAfter;
        int index;
    };

    void refreshWidgets();
    bool transfer(WidgetContainer* source, WidgetContainer* destination, int index, FyWidget* replaced = nullptr);
    bool restoreTransfer(int index);
    bool collapseSplitter(CollapsedSplitter& collapsed);
    bool restoreSplitter(const CollapsedSplitter& collapsed);
    bool collapseSplitters();
    bool restoreSplitters();
    [[nodiscard]] std::vector<QJsonObject> saveParentStates() const;
    void restoreParentStates(const std::vector<QJsonObject>& states);

    QPointer<EditableLayout> m_editableLayout;
    WidgetProvider* m_provider;
    Id m_sourceId;
    Id m_destinationId;
    Id m_widgetId;
    Id m_replacedId;

    QPointer<WidgetContainer> m_source;
    QPointer<WidgetContainer> m_destination;
    QPointer<FyWidget> m_widget;
    QPointer<FyWidget> m_replacedWidget;
    int m_sourceIndex;
    int m_destinationIndex;
    QJsonObject m_sourceBefore;
    QJsonObject m_destinationBefore;
    QJsonObject m_sourceAfter;
    QJsonObject m_destinationAfter;
    QJsonObject m_widgetState;
    std::vector<CollapsedSplitter> m_collapsedSplitters;
    bool m_applied;
};

class SplitTransferWidgetCommand : public QUndoCommand
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::SplitTransferWidgetCommand)

public:
    SplitTransferWidgetCommand(FyWidget* source, FyWidget* target, Qt::Orientation orientation, bool after,
                               WidgetProvider* provider, SettingsManager* settings, EditableLayout* layout = nullptr);
    ~SplitTransferWidgetCommand() override;

    static bool canSplit(FyWidget* source, FyWidget* target);

    void undo() override;
    void redo() override;

private:
    void refreshWidgets();
    bool unwrapTarget();

    QPointer<EditableLayout> m_editableLayout;
    Id m_sourceId;
    Id m_parentId;
    Id m_widgetId;
    Id m_targetId;
    Id m_splitterId;
    QPointer<FyWidget> m_widget;
    QPointer<FyWidget> m_target;
    QPointer<WidgetContainer> m_source;
    QPointer<WidgetContainer> m_parent;
    QPointer<SplitterWidget> m_splitter;
    std::unique_ptr<TransferWidgetCommand> m_transfer;
    QJsonObject m_sourceBefore;
    QJsonObject m_parentBefore;
    QJsonObject m_sourceAfter;
    QJsonObject m_parentAfter;
    QJsonObject m_splitterAfter;
    int m_targetIndex;
    bool m_after;
    bool m_applied;
};

class CreateLayoutWidgetCommand : public QUndoCommand
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::CreateLayoutWidgetCommand)

public:
    CreateLayoutWidgetCommand(const QString& key, const LayoutDropTarget& target, WidgetProvider* provider,
                              SettingsManager* settings, EditableLayout* layout = nullptr);
    ~CreateLayoutWidgetCommand() override;

    void undo() override;
    void redo() override;

private:
    WidgetProvider* m_provider;
    std::unique_ptr<RootContainer> m_staging;
    QPointer<FyWidget> m_created;
    bool m_finalised;
    std::unique_ptr<QUndoCommand> m_placement;
};

class MoveWidgetCommand : public LayoutChangeCommand
{
public:
    MoveWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container, int index,
                      int newIndex);

    void undo() override;
    void redo() override;

private:
    Id m_widgetId;
    int m_oldIndex;
    int m_index;
};
} // namespace Fooyin
