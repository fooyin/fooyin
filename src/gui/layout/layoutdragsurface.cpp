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

#include "layoutdragsurface.h"

#include "layoutcommands.h"
#include "layoutdragcontroller.h"
#include "widgetpalette.h"

#include <gui/layout/editablelayout.h>
#include <gui/widgetprovider.h>

#include <QPainter>

namespace Fooyin {
LayoutDragSurface::LayoutDragSurface(LayoutDragController* controller, QWidget* parent)
    : QWidget{parent}
    , m_controller{controller}
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAcceptDrops(true);
    hide();
}

void LayoutDragSurface::reset()
{
    m_target = {};
    update();
}

void LayoutDragSurface::updateTarget(const QPointF& position, Qt::KeyboardModifiers modifiers)
{
    m_controller->updateTabHover(position);
    m_target = dropTarget(position, modifiers);
    update();
}

bool LayoutDragSurface::hasValidTarget() const
{
    return m_target.isValid();
}

std::unique_ptr<QUndoCommand> LayoutDragSurface::dropCommand(const QPointF& position, Qt::KeyboardModifiers modifiers)
{
    if(!m_controller->hasActiveDrag()) {
        return {};
    }

    const auto target = dropTarget(position, modifiers);
    auto* source      = qobject_cast<WidgetContainer*>(m_controller->m_drag->source->findParent());
    if(!source || !target.isValid()) {
        return {};
    }

    if(target.placement == Placement::Replace && !m_controller->m_drag->creationKey.isEmpty()) {
        auto command
            = std::make_unique<ReplaceWidgetCommand>(m_controller->m_layout, m_controller->m_provider, target.container,
                                                     m_controller->m_drag->creationKey, target.target->id());
        //: %1 and %2 are widget names. e.g. Replace Playlist with Lyrics
        command->setText(
            tr("Replace %1 with %2")
                .arg(target.target->name(), m_controller->m_provider->displayName(m_controller->m_drag->creationKey)));
        return command;
    }

    if(!m_controller->m_drag->creationKey.isEmpty()) {
        auto command = std::make_unique<CreateLayoutWidgetCommand>(m_controller->m_drag->creationKey, target,
                                                                   m_controller->m_provider, m_controller->m_settings,
                                                                   m_controller->m_layout);
        if(command->isObsolete()) {
            return {};
        }
        return command;
    }

    if(target.placement == Placement::Split) {
        return std::make_unique<SplitTransferWidgetCommand>(m_controller->m_drag->source, target.target,
                                                            target.orientation, target.after, m_controller->m_provider,
                                                            m_controller->m_settings, m_controller->m_layout);
    }

    auto command = std::make_unique<TransferWidgetCommand>(
        source, source->widgetIndex(m_controller->m_drag->source->id()), target.container, target.index,
        target.placement == Placement::Fill || target.placement == Placement::Replace, m_controller->m_layout,
        m_controller->m_provider);
    if(target.placement == Placement::Replace) {
        //: %1 and %2 are widget names. e.g. Replace Playlist with Lyrics
        command->setText(tr("Replace %1 with %2").arg(target.target->name(), m_controller->m_drag->source->name()));
    }
    return command;
}

void LayoutDragSurface::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    const auto highlight = palette().color(QPalette::Highlight);

    if(m_target.isValid()) {
        auto* root         = m_controller->m_layout->root();
        const auto preview = m_target.previewGeometry(root, this);

        auto fill{highlight};
        fill.setAlpha(85);

        painter.fillRect(preview, fill);
        painter.setPen(QPen{highlight, 2, m_target.placement == Placement::Split ? Qt::DashLine : Qt::SolidLine});
        painter.drawRect(preview.adjusted(0, 0, -1, -1));
    }

    if(m_controller->hasActiveDrag() && !m_target.description.isEmpty()) {
        QRect textRect = painter.fontMetrics().boundingRect(m_target.description).adjusted(-8, -5, 8, 5);
        textRect.moveTopLeft(mapFrom(m_controller->m_layout, m_controller->m_drag->position).toPoint()
                             + QPoint{12, 18 - textRect.height() - 6});
        textRect.moveLeft(std::clamp(textRect.left(), 0, std::max(0, width() - textRect.width())));
        textRect.moveTop(std::clamp(textRect.top(), 0, std::max(0, height() - textRect.height())));

        painter.fillRect(textRect, palette().color(QPalette::Window));
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(textRect, Qt::AlignCenter, m_target.description);
    }
}

LayoutDropTarget LayoutDragSurface::dropTarget(const QPointF& position, Qt::KeyboardModifiers modifiers) const
{
    if(!m_controller->hasActiveDrag()) {
        return {};
    }

    if(m_controller->m_palette->isVisible()
       && m_controller->m_palette->rect().contains(
           m_controller->m_palette->mapFrom(m_controller->m_layout, position).toPoint())) {
        LayoutDropTarget target;
        target.description = tr("Drop into the layout");
        return target;
    }

    const auto& key = m_controller->m_drag->creationKey;
    if(!key.isEmpty() && !m_controller->m_provider->canCreateWidget(key)) {
        LayoutDropTarget target;
        target.description = tr("This widget is no longer available");
        return target;
    }

    auto* root         = qobject_cast<WidgetContainer*>(m_controller->m_layout->root());
    const bool replace = modifiers.testFlag(Qt::ControlModifier);
    return LayoutUtils::resolveDropTarget(m_controller->m_drag->source, root,
                                          root->mapFrom(m_controller->m_layout, position).toPoint(), replace);
}
} // namespace Fooyin
