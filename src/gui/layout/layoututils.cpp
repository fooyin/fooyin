/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "layoututils.h"

#include "layoutcommands.h"
#include "splitters/splitterwidget.h"
#include "splitters/tabstackwidget.h"
#include "widgets/dummy.h"

#include <gui/widgetcontainer.h>

#include <QTabBar>

namespace Fooyin {
namespace {
int edgeThreshold(int length)
{
    return std::clamp(length / 8, 6, 12);
}

QRect edgeMarker(const QRect& rect, Qt::Orientation orientation, bool after)
{
    if(orientation == Qt::Vertical) {
        return {rect.left(), after ? rect.bottom() - 3 : rect.top(), rect.width(), 4};
    }
    return {after ? rect.right() - 3 : rect.left(), rect.top(), 4, rect.height()};
}
} // namespace

bool LayoutDropTarget::isValid() const
{
    if(placement == Placement::Split) {
        return target != nullptr;
    }
    return container && index >= 0 && (placement != Placement::Replace || target != nullptr);
}

QRect LayoutDropTarget::previewGeometry(QWidget* root, QWidget* surface) const
{
    const auto* window = root->window();
    return preview.translated(root->mapTo(window, QPoint{}) - surface->mapTo(window, QPoint{}));
}

QRect LayoutUtils::relativeGeometry(const QWidget* widget, const QWidget* root)
{
    return {widget->mapTo(root, QPoint{}), widget->size()};
}

WidgetList LayoutUtils::layoutWidgets(WidgetContainer* root)
{
    WidgetList result;

    WidgetList pending{root};
    while(!pending.empty()) {
        auto* widget = pending.back();
        pending.pop_back();

        if(!widget) {
            continue;
        }

        result.push_back(widget);

        if(const auto* container = qobject_cast<WidgetContainer*>(widget)) {
            const auto children = container->widgets();
            pending.insert(pending.end(), children.begin(), children.end());
        }
    }

    return result;
}

LayoutDropTarget LayoutUtils::resolveDropTarget(FyWidget* source, WidgetContainer* root, const QPoint& position,
                                                bool replace)
{
    LayoutDropTarget target;
    target.description = tr("Drop beside a panel or on a tab bar");

    auto* sourceContainer = source ? qobject_cast<WidgetContainer*>(source->findParent()) : nullptr;
    if(!sourceContainer || !root || !root->rect().contains(position)) {
        return target;
    }

    const int sourceIndex = sourceContainer->widgetIndex(source->id());
    const auto tryTarget  = [sourceContainer, sourceIndex, &target](WidgetContainer* container, int gap,
                                                                    const QRect& preview, const QString& description) {
        int index{gap};

        if(container == sourceContainer && gap > sourceIndex) {
            --index;
        }

        if(TransferWidgetCommand::canTransfer(sourceContainer, sourceIndex, container, index)) {
            target = {
                .container   = container,
                .index       = index,
                .preview     = preview,
                .description = description,
                .target      = {},
            };
        }
        else {
            target.description = tr("Cannot move this panel here");
        }
    };

    const auto widgets = layoutWidgets(root);

    FyWidget* hovered{nullptr};
    for(auto* widget : widgets) {
        if(widget->isVisibleTo(root) && relativeGeometry(widget, root).contains(position)
           && (!hovered || hovered->isAncestorOf(widget))) {
            hovered = widget;
        }
    }

    if(replace) {
        target.description = tr("Choose a widget to replace");
        if(!hovered || qobject_cast<WidgetContainer*>(hovered)) {
            return target;
        }

        auto* parent    = qobject_cast<WidgetContainer*>(hovered->findParent());
        const int index = parent ? parent->widgetIndex(hovered->id()) : -1;
        if(index < 0 || parent->widgetAtIndex(index) != hovered
           || !TransferWidgetCommand::canReplace(sourceContainer, sourceIndex, parent, index)) {
            target.description = tr("Cannot replace this widget");
            return target;
        }

        target = {
            .container   = parent,
            .index       = index,
            .preview     = relativeGeometry(hovered, root),
            .description = tr("Replace %1").arg(hovered->name()),
            .placement   = Placement::Replace,
            .target      = hovered,
        };
        return target;
    }

    // Tab bar insertion takes priority over panel splitting and splitter edges
    for(auto* widget : widgets) {
        auto* tabs = qobject_cast<TabStackWidget*>(widget);
        if(!tabs || !tabs->isVisibleTo(root)) {
            continue;
        }

        const auto* tabWidget = tabs->findChild<EditableTabWidget*>(QString{}, Qt::FindDirectChildrenOnly);
        const auto* bar       = tabWidget ? tabWidget->tabBar() : nullptr;
        if(!bar || !relativeGeometry(bar, root).contains(position)) {
            continue;
        }

        const auto localPosition = bar->mapFrom(root, position);
        const bool vertical
            = tabWidget->tabPosition() == QTabWidget::West || tabWidget->tabPosition() == QTabWidget::East;
        const auto orientation = vertical ? Qt::Vertical : Qt::Horizontal;

        int gap = bar->tabAt(localPosition);
        QRect marker;
        if(gap < 0) {
            gap    = bar->count();
            marker = bar->count() ? bar->tabRect(bar->count() - 1) : bar->rect();
            marker = edgeMarker(marker, orientation, true);
        }
        else {
            marker = bar->tabRect(gap);

            const bool after
                = vertical ? localPosition.y() > marker.center().y() : localPosition.x() > marker.center().x();
            gap += after ? 1 : 0;
            marker = edgeMarker(marker, orientation, after);
        }

        marker.moveTopLeft(bar->mapTo(root, marker.topLeft()));
        tryTarget(tabs, gap, marker, tr("Insert tab"));
        return target;
    }

    // Resolve panel drops against the deepest visible layout widget under the pointer
    while(hovered && hovered != root) {
        auto* parent = qobject_cast<WidgetContainer*>(hovered->findParent());
        if(parent) {
            break;
        }
        hovered = parent;
    }
    if(!hovered) {
        return target;
    }

    // Fill blank placeholders directly; missing widget placeholders remain ordinary panels
    if(const auto* dummy = qobject_cast<Dummy*>(hovered); dummy && dummy->missingName().isEmpty()) {
        auto* container = qobject_cast<WidgetContainer*>(dummy->findParent());
        const int index = container ? container->widgetIndex(dummy->id()) : -1;
        if(TransferWidgetCommand::canTransfer(sourceContainer, sourceIndex, container, index)) {
            target = {
                .container   = container,
                .index       = index,
                .preview     = relativeGeometry(dummy, root),
                .description = tr("Fill empty area"),
                .target      = {},
            };
            target.placement = Placement::Fill;
        }
        return target;
    }

    // Reserve panel edges for insertion into an existing splitter
    const QRect panelRect    = relativeGeometry(hovered, root);
    const QPoint local       = position - panelRect.topLeft();
    const int horizontalEdge = edgeThreshold(panelRect.width());
    const int verticalEdge   = edgeThreshold(panelRect.height());
    const bool onEdge        = local.x() < horizontalEdge || local.x() >= panelRect.width() - horizontalEdge
                            || local.y() < verticalEdge || local.y() >= panelRect.height() - verticalEdge;
    if(!onEdge) {
        // Interior drops populate empty containers before considering a new split
        if(auto* container = qobject_cast<WidgetContainer*>(hovered); container && container->widgets().empty()) {
            tryTarget(container, 0, panelRect, tr("Move into empty area"));
            return target;
        }

        if(hovered == root || !SplitTransferWidgetCommand::canSplit(source, hovered)) {
            return target;
        }

        const bool vertical = local.x() >= panelRect.width() / 3 && local.x() < panelRect.width() * 2 / 3;
        const bool after    = vertical ? local.y() >= panelRect.height() / 2 : local.x() >= panelRect.width() / 2;

        QRect preview{panelRect};
        if(vertical) {
            preview.setHeight(panelRect.height() / 2);
            if(after) {
                preview.moveBottom(panelRect.bottom());
            }
        }
        else {
            preview.setWidth(panelRect.width() / 2);
            if(after) {
                preview.moveRight(panelRect.right());
            }
        }

        target.placement   = Placement::Split;
        target.target      = hovered;
        target.orientation = vertical ? Qt::Vertical : Qt::Horizontal;
        target.after       = after;
        target.preview     = preview;
        target.description = (vertical ? (after ? tr("Split %1: place below") : tr("Split %1: place above"))
                                       : (after ? tr("Split %1: place right") : tr("Split %1: place left")))
                                 .arg(hovered->name());
        return target;
    }

    // Walk outward so the nearest matching splitter edge determines the insertion gap
    const auto* current{hovered};
    while(current && current != root) {
        auto* container = qobject_cast<WidgetContainer*>(current->findParent());
        if(auto* splitter = qobject_cast<SplitterWidget*>(container)) {
            const QRect rect       = relativeGeometry(current, root);
            const auto orientation = splitter->orientation();
            const bool vertical    = orientation == Qt::Vertical;
            const int length       = vertical ? rect.height() : rect.width();
            const int coordinate   = vertical ? position.y() - rect.top() : position.x() - rect.left();
            const int edge         = edgeThreshold(length);

            if(coordinate < edge || coordinate >= length - edge) {
                const bool after = coordinate >= length - edge;
                const int gap    = splitter->widgetIndex(current->id()) + (after ? 1 : 0);

                const QRect marker = edgeMarker(rect, orientation, after);

                tryTarget(splitter, gap, marker,
                          vertical ? (after ? tr("Place below %1") : tr("Place above %1")).arg(current->name())
                                   : (after ? tr("Place after %1") : tr("Place before %1")).arg(current->name()));
                return target;
            }
        }

        if(container && container->widgets().empty()) {
            tryTarget(container, 0, relativeGeometry(container, root), tr("Move into empty area"));
            return target;
        }

        current = container;
    }

    if(auto* container = qobject_cast<WidgetContainer*>(hovered); container && container->widgets().empty()) {
        tryTarget(container, 0, relativeGeometry(container, root), tr("Move into empty area"));
    }

    return target;
}
} // namespace Fooyin
