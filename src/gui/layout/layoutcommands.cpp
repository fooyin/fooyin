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

#include "layoutcommands.h"

#include "editablelayout_p.h"
#include "layoututils.h"
#include "splitters/splitterstate.h"
#include "splitters/splitterwidget.h"
#include "widgets/dummy.h"

#include <gui/layout/editablelayout.h>
#include <gui/layout/layoutprovider.h>
#include <gui/widgetcontainer.h>
#include <gui/widgetprovider.h>

#include <QLayout>

#include <ranges>

namespace Fooyin {
namespace {
template <typename T>
void refreshWidget(QPointer<T>& widget, EditableLayout* layout, const Id& id)
{
    if(!widget && layout && id.isValid()) {
        widget = qobject_cast<T*>(layout->findWidget(id));
    }
}

void deleteDetachedWidget(FyWidget* widget)
{
    if(widget && !widget->parent()) {
        delete widget;
    }
}

void insertOrFillPlaceholder(WidgetContainer* container, int index, FyWidget* widget)
{
    const auto* placeholder = qobject_cast<Dummy*>(container->widgetAtIndex(index));
    if(placeholder && placeholder->missingName().isEmpty()) {
        container->replaceWidget(index, widget);
    }
    else {
        container->insertWidget(index, widget);
    }
}
} // namespace

LayoutChangeCommand::LayoutChangeCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container)
    : m_layout{layout}
    , m_provider{provider}
    , m_container{container}
{
    if(container) {
        m_containerId = container->id();
    }
}

LayoutChangeCommand::~LayoutChangeCommand()
{
    for(const auto& widget : m_retainedWidgets) {
        deleteDetachedWidget(widget);
    }
}

FyWidget* LayoutChangeCommand::takeWidget(WidgetContainer* container, int index)
{
    auto* widget = container->takeWidget(index);
    if(widget) {
        m_provider->setWidgetRetained(widget, true);
        m_retainedWidgets.emplace_back(widget);
    }
    return widget;
}

FyWidget* LayoutChangeCommand::exchangeWidget(WidgetContainer* container, int index, FyWidget* widget)
{
    auto* previous = container->exchangeWidget(index, widget);
    if(previous) {
        m_provider->setWidgetRetained(previous, true);
        m_retainedWidgets.emplace_back(previous);
        m_provider->setWidgetRetained(widget, false);
        std::erase(m_retainedWidgets, widget);
        widget->show();
    }
    return previous;
}

void LayoutChangeCommand::insertWidget(WidgetContainer* container, int index, FyWidget* widget, bool fillPlaceholder)
{
    if(fillPlaceholder) {
        insertOrFillPlaceholder(container, index, widget);
    }
    else {
        container->insertWidget(index, widget);
    }
    m_provider->setWidgetRetained(widget, false);
    std::erase(m_retainedWidgets, widget);
    widget->show();
}

bool LayoutChangeCommand::checkContainer()
{
    if(!m_container) {
        if(auto* container = qobject_cast<WidgetContainer*>(m_layout->findWidget(m_containerId))) {
            m_container = container;
        }
    }
    if(!m_container) {
        setObsolete(true);
        return false;
    }
    return true;
}

SetWidgetMarginsCommand::SetWidgetMarginsCommand(EditableLayout* layout, FyWidget* widget,
                                                 std::optional<QMargins> margins, const Id& session)
    : m_layout{layout}
    , m_widget{widget}
    , m_widgetId{widget->id()}
    , m_session{session}
    , m_before{widget->hasCustomLayoutMargins() ? std::optional{widget->layout()->contentsMargins()} : std::nullopt}
    , m_after{margins}
{
    setText(tr("Change widget margins"));
    setObsolete(m_before == m_after);
}

int SetWidgetMarginsCommand::id() const
{
    return 1;
}

bool SetWidgetMarginsCommand::mergeWith(const QUndoCommand* other)
{
    if(other->id() != id()) {
        return false;
    }

    const auto* command = static_cast<const SetWidgetMarginsCommand*>(other);
    if(!command || m_layout != command->m_layout || m_widgetId != command->m_widgetId
       || m_session != command->m_session) {
        return false;
    }

    m_after = command->m_after;
    setObsolete(m_before == m_after);
    return true;
}

void SetWidgetMarginsCommand::undo()
{
    apply(m_before);
}

void SetWidgetMarginsCommand::redo()
{
    apply(m_after);
}

void SetWidgetMarginsCommand::apply(const std::optional<QMargins>& margins)
{
    refreshWidget(m_widget, m_layout, m_widgetId);

    if(!m_widget || !m_widget->layout()) {
        setObsolete(true);
        return;
    }

    if(margins) {
        m_widget->setLayoutMargins(*margins);
    }
    else {
        m_widget->resetLayoutMargins();
    }
}

SwitchLayoutCommand::SwitchLayoutCommand(EditableLayoutPrivate* editableLayout, FyLayout layout)
    : m_editableLayout{editableLayout}
    , m_oldLayout{m_editableLayout->m_self->saveCurrentToLayout(
          editableLayout->m_layoutProvider->currentLayout().name(), true)}
    , m_newLayout{std::move(layout)}
{ }

void SwitchLayoutCommand::undo()
{
    m_newLayout = m_editableLayout->m_self->saveCurrentToLayout(m_newLayout.name(), true);
    m_editableLayout->m_layoutProvider->saveLayout(m_newLayout);
    m_editableLayout->changeLayout(m_oldLayout);
}

void SwitchLayoutCommand::redo()
{
    m_oldLayout = m_editableLayout->m_self->saveCurrentToLayout(m_oldLayout.name(), true);
    m_editableLayout->m_layoutProvider->saveLayout(m_oldLayout);
    m_editableLayout->changeLayout(m_newLayout);
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
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_created, m_layout, m_createdId);

    const int index = m_created ? m_container->widgetIndex(m_created->id()) : -1;
    if(index < 0) {
        setObsolete(true);
        return;
    }

    m_containerAfter = m_container->saveEditingState();
    if(m_placeholder) {
        exchangeWidget(m_container, index, m_placeholder);
    }
    else {
        takeWidget(m_container, index);
    }
    m_container->restoreEditingState(m_containerBefore);
}

void AddWidgetCommand::redo()
{
    if(!checkContainer() || !m_container->canInsertWidget(m_index)) {
        setObsolete(true);
        return;
    }

    refreshWidget(m_created, m_layout, m_createdId);

    const bool firstRedo = !m_createdId.isValid();
    if(firstRedo) {
        m_created
            = m_widget.empty() ? m_provider->createWidget(m_key) : EditableLayout::loadWidget(m_provider, m_widget);
        if(m_created) {
            m_createdId = m_created->id();
        }
    }

    if(!m_created || !m_container->canInsertWidget(m_index)) {
        setObsolete(true);
        if(firstRedo && m_created) {
            delete m_created;
        }
        return;
    }

    if(firstRedo) {
        m_containerBefore = m_container->saveEditingState();
    }

    if(const auto* dummy = qobject_cast<Dummy*>(m_container->widgetAtIndex(m_index));
       dummy && dummy->missingName().isEmpty()) {
        m_placeholder = exchangeWidget(m_container, m_index, m_created);
    }
    else {
        insertWidget(m_container, m_index, m_created);
    }

    if(firstRedo) {
        m_created->finalise();
        if(m_placeholder) {
            m_container->restoreEditingState(m_containerBefore);
        }
        m_containerAfter = m_container->saveEditingState();
    }

    m_container->restoreEditingState(m_containerAfter);
}

ReplaceWidgetCommand::ReplaceWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                           QString key, const Id& widgetToReplace)
    : LayoutChangeCommand{layout, provider, container}
    , m_key{std::move(key)}
    , m_oldWidget{container ? container->widgetAtId(widgetToReplace) : nullptr}
    , m_oldWidgetId{widgetToReplace}
{ }

ReplaceWidgetCommand::ReplaceWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                           QJsonObject widget, const Id& widgetToReplace)
    : LayoutChangeCommand{layout, provider, container}
    , m_widget{std::move(widget)}
    , m_oldWidget{container ? container->widgetAtId(widgetToReplace) : nullptr}
    , m_oldWidgetId{widgetToReplace}
{ }

void ReplaceWidgetCommand::undo()
{
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_replacement, m_layout, m_replacementId);

    const int index = m_replacement ? m_container->widgetIndex(m_replacement->id()) : -1;
    if(!m_oldWidget || index < 0) {
        setObsolete(true);
        return;
    }

    m_containerAfter = m_container->saveEditingState();
    exchangeWidget(m_container, index, m_oldWidget);
    m_container->restoreEditingState(m_containerBefore);
}

void ReplaceWidgetCommand::redo()
{
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_oldWidget, m_layout, m_oldWidgetId);

    const int index = m_oldWidget ? m_container->widgetIndex(m_oldWidget->id()) : -1;
    if(index < 0) {
        setObsolete(true);
        return;
    }

    const bool firstRedo = !m_replacementId.isValid();
    if(firstRedo) {
        m_containerBefore = m_container->saveEditingState();
        m_provider->setWidgetRetained(m_oldWidget, true);
        m_replacement
            = m_widget.empty() ? m_provider->createWidget(m_key) : EditableLayout::loadWidget(m_provider, m_widget);
        if(m_replacement) {
            m_replacementId = m_replacement->id();
        }
    }
    if(!m_replacement) {
        m_provider->setWidgetRetained(m_oldWidget, false);
        setObsolete(true);
        return;
    }

    if(!exchangeWidget(m_container, index, m_replacement)) {
        m_provider->setWidgetRetained(m_oldWidget, false);
        if(firstRedo) {
            delete m_replacement;
        }
        setObsolete(true);
        return;
    }

    if(firstRedo) {
        m_replacement->finalise();
        m_container->restoreEditingState(m_containerBefore);
        m_containerAfter = m_container->saveEditingState();
    }
    m_container->restoreEditingState(m_containerAfter);
}

SplitWidgetCommand::SplitWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                       QString key, const Id& widgetToSplit)
    : LayoutChangeCommand{layout, provider, container}
    , m_key{std::move(key)}
    , m_splitWidget{container ? container->widgetAtId(widgetToSplit) : nullptr}
    , m_splitWidgetId{widgetToSplit}
    , m_splitIndex{0}
{ }

void SplitWidgetCommand::undo()
{
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_splitWidget, m_layout, m_splitWidgetId);
    refreshWidget(m_splitContainer, m_layout, m_splitContainerId);

    const int index      = m_splitContainer ? m_container->widgetIndex(m_splitContainer->id()) : -1;
    const int childIndex = m_splitContainer && m_splitWidget ? m_splitContainer->widgetIndex(m_splitWidget->id()) : -1;
    if(index < 0 || childIndex < 0) {
        setObsolete(true);
        return;
    }

    m_containerAfter      = m_container->saveEditingState();
    m_splitContainerState = m_splitContainer->saveEditingState();
    m_splitIndex          = childIndex;
    takeWidget(m_splitContainer, childIndex);
    exchangeWidget(m_container, index, m_splitWidget);
    m_container->restoreEditingState(m_containerBefore);
}

void SplitWidgetCommand::redo()
{
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_splitWidget, m_layout, m_splitWidgetId);

    const int index = m_splitWidget ? m_container->widgetIndex(m_splitWidget->id()) : -1;
    if(index < 0) {
        setObsolete(true);
        return;
    }

    const bool firstRedo = !m_splitContainerId.isValid();
    if(firstRedo) {
        auto* created    = m_provider->createWidget(m_key);
        m_splitContainer = qobject_cast<WidgetContainer*>(created);
        if(!m_splitContainer) {
            delete created;
            setObsolete(true);
            return;
        }
        m_splitContainerId = m_splitContainer->id();
        m_splitContainer->finalise();
    }

    if(!m_splitContainer || !m_splitContainer->canInsertWidget(m_splitIndex)) {
        if(firstRedo) {
            delete m_splitContainer;
        }
        setObsolete(true);
        return;
    }

    if(firstRedo) {
        m_containerBefore = m_container->saveEditingState();
    }
    exchangeWidget(m_container, index, m_splitContainer);
    insertWidget(m_splitContainer, m_splitIndex, m_splitWidget, firstRedo);

    if(firstRedo) {
        m_container->restoreEditingState(m_containerBefore);
        m_containerAfter = m_container->saveEditingState();
    }
    else {
        m_splitContainer->restoreEditingState(m_splitContainerState);
    }
    m_container->restoreEditingState(m_containerAfter);
}

RemoveWidgetCommand::RemoveWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                         const Id& widgetId)
    : LayoutChangeCommand{layout, provider, container}
    , m_index{container ? container->widgetIndex(widgetId) : -1}
    , m_widget{container ? container->widgetAtId(widgetId) : nullptr}
    , m_widgetId{widgetId}
{ }

void RemoveWidgetCommand::undo()
{
    if(!checkContainer() || !m_widget || !m_container->canInsertWidget(m_index)) {
        setObsolete(true);
        return;
    }

    refreshWidget(m_placeholder, m_layout, m_placeholderId);
    if(m_placeholderId.isValid() && (!m_placeholder || m_container->widgetAtIndex(m_index) != m_placeholder)) {
        setObsolete(true);
        return;
    }

    m_containerAfter = m_container->saveEditingState();
    if(m_placeholder) {
        exchangeWidget(m_container, m_index, m_widget);
    }
    else {
        insertWidget(m_container, m_index, m_widget);
    }
    m_container->restoreEditingState(m_containerBefore);
}

void RemoveWidgetCommand::redo()
{
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_widget, m_layout, m_widgetId);
    const int index = m_widget ? m_container->widgetIndex(m_widget->id()) : -1;
    if(index < 0 || (m_placeholderId.isValid() && !m_placeholder)) {
        setObsolete(true);
        return;
    }

    const bool firstRedo = m_containerBefore.empty();
    if(firstRedo) {
        if(auto* splitter = qobject_cast<SplitterWidget*>(m_container); splitter && splitter->widgetCount() <= 2) {
            m_placeholder = m_provider->createWidget(QStringLiteral("Dummy"));
            if(!m_placeholder) {
                setObsolete(true);
                return;
            }
            m_placeholderId = m_placeholder->id();
        }
        m_index           = index;
        m_containerBefore = m_container->saveEditingState();
    }

    if(m_placeholder) {
        exchangeWidget(m_container, index, m_placeholder);
    }
    else {
        takeWidget(m_container, index);
    }

    if(firstRedo) {
        if(m_placeholder) {
            m_container->restoreEditingState(m_containerBefore);
        }
        m_containerAfter = m_container->saveEditingState();
    }
    m_container->restoreEditingState(m_containerAfter);
}

CollapseContainerCommand::CollapseContainerCommand(EditableLayout* layout, WidgetProvider* provider,
                                                   WidgetContainer* container, const Id& containerId)
    : LayoutChangeCommand{layout, provider, container}
    , m_collapsedContainer{container ? qobject_cast<WidgetContainer*>(container->widgetAtId(containerId)) : nullptr}
    , m_collapsedContainerId{containerId}
    , m_promotedIndex{-1}
{
    if(!m_collapsedContainer) {
        setObsolete(true);
        return;
    }

    const auto widgets = m_collapsedContainer->widgets();
    for(auto* widget : widgets) {
        if(!qobject_cast<Dummy*>(widget)) {
            if(m_promotedWidget) {
                setObsolete(true);
                return;
            }
            m_promotedWidget   = widget;
            m_promotedWidgetId = widget->id();
            m_promotedIndex    = m_collapsedContainer->widgetIndex(widget->id());
        }
    }

    if(!m_promotedWidget) {
        setObsolete(true);
    }
}

void CollapseContainerCommand::undo()
{
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_promotedWidget, m_layout, m_promotedWidgetId);

    const int index = m_promotedWidget ? m_container->widgetIndex(m_promotedWidget->id()) : -1;
    if(index < 0 || !m_collapsedContainer || !m_collapsedContainer->canInsertWidget(m_promotedIndex)) {
        setObsolete(true);
        return;
    }

    m_containerAfter = m_container->saveEditingState();
    exchangeWidget(m_container, index, m_collapsedContainer);
    insertWidget(m_collapsedContainer, m_promotedIndex, m_promotedWidget);
    m_collapsedContainer->restoreEditingState(m_collapsedState);
    m_container->restoreEditingState(m_containerBefore);
}

void CollapseContainerCommand::redo()
{
    if(!checkContainer()) {
        return;
    }

    refreshWidget(m_collapsedContainer, m_layout, m_collapsedContainerId);
    refreshWidget(m_promotedWidget, m_layout, m_promotedWidgetId);

    const int index = m_collapsedContainer ? m_container->widgetIndex(m_collapsedContainer->id()) : -1;
    const int childIndex
        = m_collapsedContainer && m_promotedWidget ? m_collapsedContainer->widgetIndex(m_promotedWidget->id()) : -1;
    if(index < 0 || childIndex < 0) {
        setObsolete(true);
        return;
    }

    const bool firstRedo = m_containerBefore.empty();
    if(firstRedo) {
        m_containerBefore = m_container->saveEditingState();
    }

    m_collapsedState = m_collapsedContainer->saveEditingState();
    m_promotedIndex  = childIndex;
    takeWidget(m_collapsedContainer, childIndex);
    exchangeWidget(m_container, index, m_promotedWidget);

    if(firstRedo) {
        m_container->restoreEditingState(m_containerBefore);
        m_containerAfter = m_container->saveEditingState();
    }
    m_container->restoreEditingState(m_containerAfter);
}

TransferWidgetCommand::TransferWidgetCommand(WidgetContainer* source, int index, WidgetContainer* destination,
                                             int destinationIndex, bool replaceTarget, EditableLayout* layout,
                                             WidgetProvider* provider)
    : m_editableLayout{layout}
    , m_provider{provider}
    , m_source{source}
    , m_destination{destination}
    , m_sourceIndex{index}
    , m_destinationIndex{destinationIndex}
    , m_applied{false}
{
    if(!(replaceTarget ? canReplace(source, index, destination, destinationIndex)
                       : canTransfer(source, index, destination, destinationIndex))) {
        setObsolete(true);
        return;
    }

    setText(tr("Move widget"));

    m_sourceId          = source->id();
    m_destinationId     = destination->id();
    m_widget            = source->widgetAtIndex(index);
    m_widgetId          = m_widget->id();
    m_widgetState       = source->saveChildState(index);
    m_sourceBefore      = source->saveEditingState();
    m_destinationBefore = destination->saveEditingState();

    if(replaceTarget) {
        m_replacedWidget = destination->widgetAtIndex(destinationIndex);
        m_replacedId     = m_replacedWidget->id();

        if(source == destination && index < destinationIndex) {
            --m_destinationIndex;
        }
    }
}

TransferWidgetCommand::~TransferWidgetCommand()
{
    if(!m_applied) {
        return;
    }

    deleteDetachedWidget(m_replacedWidget);

    for(const auto& collapsed : m_collapsedSplitters) {
        deleteDetachedWidget(collapsed.splitter);
    }
}

void TransferWidgetCommand::refreshWidgets()
{
    refreshWidget(m_source, m_editableLayout, m_sourceId);
    refreshWidget(m_destination, m_editableLayout, m_destinationId);
    refreshWidget(m_widget, m_editableLayout, m_widgetId);
    refreshWidget(m_replacedWidget, m_editableLayout, m_replacedId);

    for(auto& collapsed : m_collapsedSplitters) {
        refreshWidget(collapsed.splitter, m_editableLayout, collapsed.splitterId);
        refreshWidget(collapsed.parent, m_editableLayout, collapsed.parentId);
        refreshWidget(collapsed.promoted, m_editableLayout, collapsed.promotedId);
    }
}

bool TransferWidgetCommand::collapseSplitter(CollapsedSplitter& collapsed)
{
    if(!collapsed.splitter || !collapsed.parent || (collapsed.promotedId.isValid() && !collapsed.promoted)
       || collapsed.splitter->widgetCount() != (collapsed.promoted ? 1 : 0)
       || (collapsed.promoted && collapsed.splitter->widgetAtIndex(0) != collapsed.promoted)) {
        return false;
    }

    const int index = collapsed.parent->widgetIndex(collapsed.splitter->id());
    if(index < 0) {
        return false;
    }

    if(collapsed.promoted) {
        auto* promoted = collapsed.splitter->takeWidget(0);
        if(!collapsed.parent->exchangeWidget(index, promoted)) {
            collapsed.splitter->insertWidget(0, promoted);
            promoted->show();
            return false;
        }
        promoted->show();
        collapsed.parent->restoreEditingState(collapsed.parentState);
    }
    else {
        collapsed.parent->takeWidget(index);
        if(m_sourceAfter.empty()) {
            if(auto* parent = qobject_cast<SplitterWidget*>(collapsed.parent)) {
                parent->expandSingleWidget();
            }
            collapsed.parentAfter = collapsed.parent->saveEditingState();
        }
        collapsed.parent->restoreEditingState(collapsed.parentAfter);
    }

    return true;
}

bool TransferWidgetCommand::collapseSplitters()
{
    if(m_sourceAfter.empty()) {
        auto* splitter = qobject_cast<SplitterWidget*>(m_source);
        while(splitter && splitter->widgetCount() <= 1) {
            auto* parent   = qobject_cast<WidgetContainer*>(splitter->findParent());
            auto* promoted = splitter->widgetAtIndex(0);
            if(!parent || parent->widgetIndex(splitter->id()) < 0 || qobject_cast<Dummy*>(promoted)) {
                break;
            }

            CollapsedSplitter collapsed{
                .splitter    = splitter,
                .parent      = parent,
                .promoted    = promoted,
                .splitterId  = splitter->id(),
                .parentId    = parent->id(),
                .promotedId  = promoted ? promoted->id() : Id{},
                .parentState = parent->saveEditingState(),
                .parentAfter = {},
                .index       = parent->widgetIndex(splitter->id()),
            };

            if(!collapseSplitter(collapsed)) {
                const bool restored = restoreSplitters();
                Q_ASSERT(restored);
                if(restored) {
                    m_collapsedSplitters.clear();
                }
                return false;
            }

            m_collapsedSplitters.push_back(std::move(collapsed));
            splitter = qobject_cast<SplitterWidget*>(parent);
        }
        return true;
    }

    for(size_t index{0}; index < m_collapsedSplitters.size(); ++index) {
        if(!collapseSplitter(m_collapsedSplitters[index])) {
            while(index > 0) {
                const bool restored = restoreSplitter(m_collapsedSplitters[--index]);
                Q_ASSERT(restored);
            }
            return false;
        }
    }

    return true;
}

bool TransferWidgetCommand::restoreSplitter(const CollapsedSplitter& collapsed)
{
    if(!collapsed.splitter || !collapsed.parent || (collapsed.promotedId.isValid() && !collapsed.promoted)
       || collapsed.splitter->widgetCount() != 0 || collapsed.splitter->findParent()) {
        return false;
    }

    const int index = collapsed.promoted ? collapsed.parent->widgetIndex(collapsed.promoted->id()) : collapsed.index;
    if(index < 0 || (!collapsed.promoted && !collapsed.parent->canInsertWidget(index))) {
        return false;
    }

    if(collapsed.promoted) {
        auto* promoted = collapsed.parent->exchangeWidget(index, collapsed.splitter);
        if(!promoted) {
            return false;
        }
        collapsed.splitter->insertWidget(0, promoted);
        promoted->show();
    }
    else {
        collapsed.parent->insertWidget(index, collapsed.splitter);
    }

    collapsed.splitter->show();
    collapsed.parent->restoreEditingState(collapsed.parentState);
    return true;
}

std::vector<QJsonObject> TransferWidgetCommand::saveParentStates() const
{
    std::vector<QJsonObject> states;
    states.reserve(m_collapsedSplitters.size());
    for(const auto& collapsed : m_collapsedSplitters) {
        states.push_back(collapsed.parent ? collapsed.parent->saveEditingState() : QJsonObject{});
    }
    return states;
}

void TransferWidgetCommand::restoreParentStates(const std::vector<QJsonObject>& states)
{
    Q_ASSERT(states.size() == m_collapsedSplitters.size());
    for(size_t index{0}; index < m_collapsedSplitters.size(); ++index) {
        if(m_collapsedSplitters[index].parent) {
            m_collapsedSplitters[index].parent->restoreEditingState(states[index]);
        }
    }
}

bool TransferWidgetCommand::restoreSplitters()
{
    // Restore from outside in
    const auto parentStates = saveParentStates();
    for(size_t index = m_collapsedSplitters.size(); index > 0; --index) {
        if(!restoreSplitter(m_collapsedSplitters[index - 1])) {
            for(size_t restored = index; restored < m_collapsedSplitters.size(); ++restored) {
                const bool collapsed = collapseSplitter(m_collapsedSplitters[restored]);
                Q_ASSERT(collapsed);
            }
            restoreParentStates(parentStates);
            return false;
        }
    }

    return true;
}

bool TransferWidgetCommand::canTransfer(WidgetContainer* source, int index, WidgetContainer* destination,
                                        int destinationIndex)
{
    if(!source || !destination || !destination->canInsertWidget(destinationIndex)) {
        return false;
    }

    if(source == destination
       && (index == destinationIndex || destinationIndex < 0
           || std::cmp_greater_equal(destinationIndex, source->widgets().size()))) {
        return false;
    }

    const auto* widget = source->widgetAtIndex(index);
    return widget && widget != destination && !widget->isAncestorOf(destination);
}

bool TransferWidgetCommand::canReplace(WidgetContainer* source, int index, WidgetContainer* destination,
                                       int destinationIndex)
{
    const auto* widget = source ? source->widgetAtIndex(index) : nullptr;
    const auto* target = destination ? destination->widgetAtIndex(destinationIndex) : nullptr;
    return widget && target && widget != target && widget != destination
        && !qobject_cast<const WidgetContainer*>(target) && !widget->isAncestorOf(destination);
}

bool TransferWidgetCommand::transfer(WidgetContainer* source, WidgetContainer* destination, int index,
                                     FyWidget* replaced)
{
    if(!m_widget || !source || !destination) {
        return false;
    }

    const int sourceIndex = source->widgetIndex(m_widget->id());
    if(sourceIndex < 0 || source->widgetAtIndex(sourceIndex) != m_widget
       || (!replaced && !destination->canInsertWidget(index)) || m_widget == destination
       || m_widget->isAncestorOf(destination)) {
        return false;
    }

    const auto sourceState      = source->saveEditingState();
    const auto destinationState = destination->saveEditingState();
    auto* widget                = source->takeWidget(sourceIndex);
    if(!widget) {
        return false;
    }

    if(replaced) {
        if(destination->widgetAtIndex(index) != replaced || !destination->exchangeWidget(index, widget)) {
            source->insertWidget(sourceIndex, widget);
            widget->show();
            source->restoreEditingState(sourceState);
            destination->restoreEditingState(destinationState);
            return false;
        }
    }
    else {
        destination->insertWidget(index, widget);
    }

    if(destination->widgetAtId(widget->id()) != widget) {
        source->insertWidget(sourceIndex, widget);
        widget->show();
        source->restoreEditingState(sourceState);
        destination->restoreEditingState(destinationState);
        return false;
    }

    if(!replaced) {
        destination->restoreChildState(index, m_widgetState);
    }
    widget->show();
    return true;
}

bool TransferWidgetCommand::restoreTransfer(int index)
{
    if(!m_replacedWidget) {
        return transfer(m_destination, m_source, index);
    }

    const int destinationIndex = m_destination->widgetIndex(m_widgetId);
    if(destinationIndex < 0 || !m_source->canInsertWidget(index)) {
        return false;
    }

    auto* widget = m_destination->exchangeWidget(destinationIndex, m_replacedWidget);
    if(!widget) {
        return false;
    }

    m_source->insertWidget(index, widget);
    if(m_source->widgetAtId(m_widgetId) != widget) {
        auto* replaced = m_destination->exchangeWidget(destinationIndex, widget);
        Q_ASSERT(replaced == m_replacedWidget);
        return false;
    }

    widget->show();
    m_replacedWidget->show();
    return true;
}

void TransferWidgetCommand::undo()
{
    refreshWidgets();

    const bool promotedToParent = std::ranges::any_of(m_collapsedSplitters, [this](const auto& collapsed) {
        return collapsed.splitter == m_destination && collapsed.promoted == m_widget;
    });
    if(!m_applied || !m_source || !m_destination || !m_widget
       || (m_destination->widgetAtId(m_widgetId) != m_widget && !promotedToParent)
       || (m_replacedId.isValid() && (!m_replacedWidget || m_replacedWidget->parent()))) {
        setObsolete(true);
        return;
    }

    const auto sourceState      = m_source->saveEditingState();
    const auto destinationState = m_destination->saveEditingState();
    const auto parentStates     = saveParentStates();

    if(!restoreSplitters()) {
        setObsolete(true);
        return;
    }

    if(!restoreTransfer(m_sourceIndex)) {
        const bool collapsed = collapseSplitters();
        Q_ASSERT(collapsed);
        m_source->restoreEditingState(sourceState);
        m_destination->restoreEditingState(destinationState);
        restoreParentStates(parentStates);
        setObsolete(true);
        return;
    }

    if(m_provider && m_replacedWidget) {
        m_provider->setWidgetRetained(m_replacedWidget, false);
    }

    m_source->restoreEditingState(m_sourceBefore);
    m_destination->restoreEditingState(m_destinationBefore);
    m_applied = false;
}

void TransferWidgetCommand::redo()
{
    refreshWidgets();

    if(isObsolete() || m_applied || !m_source || !m_destination || !m_widget
       || m_source->widgetAtId(m_widgetId) != m_widget
       || (m_replacedId.isValid()
           && (!m_replacedWidget || m_destination->widgetAtId(m_replacedId) != m_replacedWidget))) {
        setObsolete(true);
        return;
    }

    const auto sourceState      = m_source->saveEditingState();
    const auto destinationState = m_destination->saveEditingState();
    const int sourceIndex       = m_source->widgetIndex(m_widgetId);
    if(!transfer(m_source, m_destination, m_destinationIndex, m_replacedWidget)) {
        m_source->restoreEditingState(sourceState);
        m_destination->restoreEditingState(destinationState);
        setObsolete(true);
        return;
    }

    const bool firstRedo = m_sourceAfter.empty();
    if(firstRedo) {
        if(auto* splitter = qobject_cast<SplitterWidget*>(m_source)) {
            splitter->expandSingleWidget();
        }
    }
    const auto sourceAfter      = firstRedo ? m_source->saveEditingState() : m_sourceAfter;
    const auto destinationAfter = firstRedo ? m_destination->saveEditingState() : m_destinationAfter;
    m_source->restoreEditingState(sourceAfter);
    m_destination->restoreEditingState(destinationAfter);

    if(!collapseSplitters()) {
        const bool restored = restoreTransfer(sourceIndex);
        Q_ASSERT(restored);
        m_source->restoreEditingState(sourceState);
        m_destination->restoreEditingState(destinationState);
        setObsolete(true);
        return;
    }

    if(firstRedo) {
        m_sourceAfter      = sourceAfter;
        m_destinationAfter = destinationAfter;
    }
    if(m_provider && m_replacedWidget) {
        m_provider->setWidgetRetained(m_replacedWidget, true);
    }
    m_applied = true;
}

bool SplitTransferWidgetCommand::canSplit(FyWidget* source, FyWidget* target)
{
    if(!source || !target || source == target || source->isAncestorOf(target) || target->isAncestorOf(source)) {
        return false;
    }

    const auto* sourceContainer = qobject_cast<WidgetContainer*>(source->findParent());
    const auto* targetContainer = qobject_cast<WidgetContainer*>(target->findParent());
    return sourceContainer && targetContainer && sourceContainer->widgetAtId(source->id()) == source
        && targetContainer->widgetAtId(target->id()) == target;
}

SplitTransferWidgetCommand::SplitTransferWidgetCommand(FyWidget* source, FyWidget* target, Qt::Orientation orientation,
                                                       bool after, WidgetProvider* provider, SettingsManager* settings,
                                                       EditableLayout* layout)
    : m_editableLayout{layout}
    , m_widget{source}
    , m_target{target}
    , m_targetIndex{-1}
    , m_after{after}
    , m_applied{false}
{
    if(!canSplit(source, target)) {
        setObsolete(true);
        return;
    }

    setText(tr("Split and move widget"));

    m_source       = qobject_cast<WidgetContainer*>(source->findParent());
    m_parent       = qobject_cast<WidgetContainer*>(target->findParent());
    m_sourceId     = m_source->id();
    m_parentId     = m_parent->id();
    m_widgetId     = source->id();
    m_targetId     = target->id();
    m_targetIndex  = m_parent->widgetIndex(target->id());
    m_sourceBefore = m_source->saveEditingState();
    m_parentBefore = m_parent->saveEditingState();

    m_splitter = new SplitterWidget(provider, settings);
    m_splitter->setOrientation(orientation);
    m_splitterId = m_splitter->id();
}

SplitTransferWidgetCommand::~SplitTransferWidgetCommand()
{
    if(!m_applied) {
        deleteDetachedWidget(m_splitter);
    }
}

void SplitTransferWidgetCommand::refreshWidgets()
{
    refreshWidget(m_source, m_editableLayout, m_sourceId);
    refreshWidget(m_parent, m_editableLayout, m_parentId);
    refreshWidget(m_widget, m_editableLayout, m_widgetId);
    refreshWidget(m_target, m_editableLayout, m_targetId);
    refreshWidget(m_splitter, m_editableLayout, m_splitterId);
}

bool SplitTransferWidgetCommand::unwrapTarget()
{
    const int index       = m_parent->widgetIndex(m_splitterId);
    const int targetIndex = m_splitter->widgetIndex(m_targetId);
    if(index != m_targetIndex || targetIndex < 0 || m_splitter->widgetCount() != 1) {
        return false;
    }

    auto* target = m_splitter->takeWidget(targetIndex);
    if(!m_parent->exchangeWidget(index, target)) {
        m_splitter->insertWidget(targetIndex, target);
        target->show();
        return false;
    }
    target->show();
    return true;
}

void SplitTransferWidgetCommand::redo()
{
    refreshWidgets();

    if(isObsolete() || m_applied || !m_source || !m_parent || !m_splitter || !canSplit(m_widget, m_target)
       || m_source->widgetAtId(m_widgetId) != m_widget || m_parent->widgetAtId(m_targetId) != m_target
       || m_parent->widgetIndex(m_targetId) != m_targetIndex || m_splitter->widgetCount() != 0
       || m_splitter->parent()) {
        setObsolete(true);
        return;
    }

    const auto sourceState = m_source->saveEditingState();
    const auto parentState = m_parent->saveEditingState();
    auto* target           = m_parent->exchangeWidget(m_targetIndex, m_splitter);
    if(!target) {
        setObsolete(true);
        return;
    }

    m_splitter->insertWidget(0, target);

    target->show();
    m_splitter->show();
    m_parent->restoreEditingState(m_parentBefore);

    const bool firstRedo = !m_transfer;
    if(firstRedo) {
        m_transfer = std::make_unique<TransferWidgetCommand>(m_source, m_source->widgetIndex(m_widgetId), m_splitter,
                                                             m_after ? 1 : 0, false, m_editableLayout);
    }
    m_transfer->redo();

    if(m_transfer->isObsolete()) {
        const bool restored = unwrapTarget();
        Q_ASSERT(restored);
        m_source->restoreEditingState(sourceState);
        m_parent->restoreEditingState(parentState);
        setObsolete(true);
        return;
    }

    if(firstRedo) {
        if(auto state = decodeSplitterState(m_splitter->saveState())) {
            state->sizes = {100, 100};
            m_splitter->restoreState(encodeSplitterState(*state));
        }
        m_splitterAfter = m_splitter->saveEditingState();
        m_sourceAfter   = m_source->saveEditingState();
        m_parentAfter   = m_parent->saveEditingState();
    }
    else {
        m_splitter->restoreEditingState(m_splitterAfter);
        m_source->restoreEditingState(m_sourceAfter);
        m_parent->restoreEditingState(m_parentAfter);
    }

    m_applied = true;
}

void SplitTransferWidgetCommand::undo()
{
    refreshWidgets();

    if(!m_applied || !m_source || !m_parent || !m_splitter || !m_target || !m_widget || !m_transfer
       || m_transfer->isObsolete() || m_splitter->widgetAtId(m_targetId) != m_target
       || m_splitter->widgetAtId(m_widgetId) != m_widget || m_splitter->widgetCount() != 2) {
        setObsolete(true);
        return;
    }

    const auto sourceState   = m_source->saveEditingState();
    const auto parentState   = m_parent->saveEditingState();
    const auto splitterState = m_splitter->saveEditingState();

    m_transfer->undo();

    if(m_transfer->isObsolete()) {
        setObsolete(true);
        return;
    }

    if(!unwrapTarget()) {
        m_transfer->redo();
        Q_ASSERT(!m_transfer->isObsolete());
        m_source->restoreEditingState(sourceState);
        m_parent->restoreEditingState(parentState);
        m_splitter->restoreEditingState(splitterState);
        setObsolete(true);
        return;
    }

    m_source->restoreEditingState(m_sourceBefore);
    m_parent->restoreEditingState(m_parentBefore);
    m_applied = false;
}

CreateLayoutWidgetCommand::CreateLayoutWidgetCommand(const QString& key, const LayoutDropTarget& target,
                                                     WidgetProvider* provider, SettingsManager* settings,
                                                     EditableLayout* layout)
    : m_provider{provider}
    , m_staging{std::make_unique<RootContainer>(provider, settings)}
    , m_finalised{false}
{
    //: %1 is the name of a widget e.g. Add Playlist
    setText(tr("Add %1").arg(provider->displayName(key)));

    auto* probe      = m_staging->widget();
    const bool valid = target.isValid() && target.placement != Placement::Replace
                    && (target.placement == Placement::Split
                            ? SplitTransferWidgetCommand::canSplit(probe, target.target)
                            : TransferWidgetCommand::canTransfer(m_staging.get(), 0, target.container, target.index));

    const auto catalogue = provider->widgetCatalogue();
    const bool available
        = std::ranges::any_of(catalogue, [&key](const auto& entry) { return entry.key == key && entry.available; });
    if(!valid || !available) {
        setObsolete(true);
        return;
    }

    auto* widget = provider->createWidget(key);
    if(!widget) {
        setObsolete(true);
        return;
    }

    m_staging->addWidget(widget);
    m_created = widget;

    if(target.placement == Placement::Split) {
        m_placement = std::make_unique<SplitTransferWidgetCommand>(widget, target.target, target.orientation,
                                                                   target.after, provider, settings, layout);
    }
    else {
        m_placement = std::make_unique<TransferWidgetCommand>(m_staging.get(), 0, target.container, target.index,
                                                              target.placement == Placement::Fill, layout);
    }
}

CreateLayoutWidgetCommand::~CreateLayoutWidgetCommand() = default;

void CreateLayoutWidgetCommand::redo()
{
    if(!m_placement || isObsolete()) {
        return;
    }

    m_placement->redo();
    setObsolete(m_placement->isObsolete());

    if(!isObsolete() && m_created) {
        m_provider->setWidgetRetained(m_created, false);
        if(!m_finalised) {
            m_created->finalise();
            m_finalised = true;
        }
    }
}

void CreateLayoutWidgetCommand::undo()
{
    if(!m_placement || isObsolete()) {
        return;
    }

    m_placement->undo();
    setObsolete(m_placement->isObsolete());
    if(!isObsolete()) {
        m_created = m_staging->widget();
        m_provider->setWidgetRetained(m_created, true);
    }
}

MoveWidgetCommand::MoveWidgetCommand(EditableLayout* layout, WidgetProvider* provider, WidgetContainer* container,
                                     int index, int newIndex)
    : LayoutChangeCommand{layout, provider, container}
    , m_oldIndex{index}
    , m_index{newIndex}
{
    if(container && container->canMoveWidget(index, newIndex)) {
        m_widgetId = container->widgetAtIndex(index)->id();
    }
    else {
        setObsolete(true);
    }
}

void MoveWidgetCommand::undo()
{
    if(!checkContainer() || m_container->widgetIndex(m_widgetId) != m_index
       || !m_container->canMoveWidget(m_index, m_oldIndex)) {
        setObsolete(true);
        return;
    }

    m_containerAfter = m_container->saveEditingState();
    m_container->moveWidget(m_index, m_oldIndex);
    m_container->restoreEditingState(m_containerBefore);
}

void MoveWidgetCommand::redo()
{
    if(isObsolete() || !checkContainer() || m_container->widgetIndex(m_widgetId) != m_oldIndex
       || !m_container->canMoveWidget(m_oldIndex, m_index)) {
        setObsolete(true);
        return;
    }

    const bool firstRedo = m_containerBefore.empty();
    if(firstRedo) {
        m_containerBefore = m_container->saveEditingState();
    }
    m_container->moveWidget(m_oldIndex, m_index);

    if(firstRedo) {
        m_containerAfter = m_container->saveEditingState();
    }
    m_container->restoreEditingState(m_containerAfter);
}
} // namespace Fooyin
