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

#include "tabstyle.h"

#include "styleutils.h"
#include "theme/tabappearance.h"

#include <QAbstractButton>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QStyleOptionTab>
#include <QTabBar>
#include <QTabWidget>
#include <QVariant>

#include <cmath>

constexpr auto ButtonSizeProperty   = "_fooyin_tab_button_size";
constexpr auto SelectedRectProperty = "_fooyin_connected_tab_rect";

namespace {
QTabWidget* tabWidget(const QWidget* widget)
{
    const auto* bar = qobject_cast<const QTabBar*>(widget);
    auto* tabs      = bar ? qobject_cast<QTabWidget*>(bar->parentWidget()) : nullptr;
    return tabs && tabs->tabBar() == bar && tabs->style() == bar->style() && !tabs->documentMode() ? tabs : nullptr;
}

bool isConnected(Fooyin::TabAppearance::Design design)
{
    return design == Fooyin::TabAppearance::Design::Connected;
}

QTransform tabTransform(QTabBar::Shape shape, const QRectF& rect)
{
    QTransform transform;
    transform.translate(rect.left(), rect.top());
    switch(shape) {
        case QTabBar::RoundedSouth:
        case QTabBar::TriangularSouth:
            transform.translate(0.0, rect.height());
            transform.scale(1.0, -1.0);
            break;
        case QTabBar::RoundedWest:
        case QTabBar::TriangularWest:
            transform.translate(0.0, rect.height());
            transform.rotate(-90.0);
            break;
        case QTabBar::RoundedEast:
        case QTabBar::TriangularEast:
            transform.translate(rect.width(), 0.0);
            transform.rotate(90.0);
            break;
        default:
            break;
    }
    return transform;
}

QMargins stripMargins(const QStyleOptionTab& option, const Fooyin::TabAppearance& appearance)
{
    const bool first  = option.position == QStyleOptionTab::Beginning || option.position == QStyleOptionTab::OnlyOneTab;
    const bool last   = option.position == QStyleOptionTab::End || option.position == QStyleOptionTab::OnlyOneTab;
    const int leading = first ? appearance.strip.leadingPadding : 0;
    const int trailing = last ? appearance.strip.trailingPadding : 0;
    const int outer    = appearance.strip.outerPadding;

    switch(option.shape) {
        case QTabBar::RoundedWest:
        case QTabBar::TriangularWest:
            return {outer, leading, 0, trailing};
        case QTabBar::RoundedEast:
        case QTabBar::TriangularEast:
            return {0, leading, outer, trailing};
        case QTabBar::RoundedSouth:
        case QTabBar::TriangularSouth:
            return {option.direction == Qt::RightToLeft ? trailing : leading, 0,
                    option.direction == Qt::RightToLeft ? leading : trailing, outer};
        default:
            return {option.direction == Qt::RightToLeft ? trailing : leading, outer,
                    option.direction == Qt::RightToLeft ? leading : trailing, 0};
    }
}

QRect selectedTabRect(const QTabBar& bar, const Fooyin::TabAppearance& appearance)
{
    int first{0};
    int last{bar.count() - 1};
    while(first < bar.count() && !bar.isTabVisible(first)) {
        ++first;
    }
    while(last >= 0 && !bar.isTabVisible(last)) {
        --last;
    }

    QStyleOptionTab option;
    option.rect      = bar.tabRect(bar.currentIndex());
    option.shape     = bar.shape();
    option.direction = bar.layoutDirection();
    option.position  = bar.currentIndex() == first && bar.currentIndex() == last ? QStyleOptionTab::OnlyOneTab
                     : bar.currentIndex() == first                               ? QStyleOptionTab::Beginning
                     : bar.currentIndex() == last                                ? QStyleOptionTab::End
                                                                                 : QStyleOptionTab::Middle;
    return Fooyin::Style::Tab::paddedTabRect(option, appearance);
}

QPainterPath panePath(const QStyleOptionTabWidgetFrame& option, const Fooyin::TabAppearance& appearance)
{
    const auto transform = tabTransform(option.shape, option.rect);
    const auto inverse   = transform.inverted();
    const bool classic   = appearance.pane.reliefStyle == Fooyin::TabAppearance::Pane::ReliefStyle::Classic
                        && appearance.pane.reliefOpacity > 0 && appearance.pane.borderWidth > 0
                        && appearance.pane.borderOpacity > 0;
    const double inset   = (appearance.pane.borderWidth / 2.0) + (classic ? 1.0 : 0.0);
    const QRectF body    = inverse.mapRect(QRectF{option.rect}).adjusted(inset, inset, -inset, -inset);
    if(body.isEmpty()) {
        return {};
    }

    const QRect visibleTab = option.selectedTabRect.intersected(option.tabBarRect);
    const QRectF tab       = inverse.mapRect(QRectF{visibleTab});
    const double tabInset  = std::max(appearance.inset, inset);

    const double radius = std::min({appearance.pane.radius, body.width() / 2.0, body.height() / 2.0});
    const double top    = tab.top() + tabInset;
    const double left   = std::max(body.left(), tab.left() + tabInset);
    const double right  = std::min(body.right(), tab.right() - tabInset);

    QPainterPath path;
    if(!isConnected(appearance.design) || option.tabBarSize.isEmpty()) {
        path.addRoundedRect(body, radius, radius);
        return transform.map(path);
    }

    path.moveTo(body.topLeft());

    if(!visibleTab.isEmpty() && right > left && top < body.top()) {
        const double tabRadius = std::min({appearance.radius, (right - left) / 2.0, (body.top() - top) / 2.0});
        const double leftJoin  = std::min(tabRadius, left - body.left());
        const double rightJoin = std::min(tabRadius, body.right() - right);

        path.lineTo(left - leftJoin, body.top());
        path.quadTo(QPointF{left, body.top()}, QPointF{left, body.top() - leftJoin});
        path.lineTo(left, top + tabRadius);
        path.quadTo(QPointF{left, top}, QPointF{left + tabRadius, top});
        path.lineTo(right - tabRadius, top);
        path.quadTo(QPointF{right, top}, QPointF{right, top + tabRadius});
        path.lineTo(right, body.top() - rightJoin);
        path.quadTo(QPointF{right, body.top()}, QPointF{right + rightJoin, body.top()});
    }

    path.lineTo(body.topRight());
    path.lineTo(body.right(), body.bottom() - radius);
    path.quadTo(body.bottomRight(), QPointF{body.right() - radius, body.bottom()});
    path.lineTo(body.left() + radius, body.bottom());
    path.quadTo(body.bottomLeft(), QPointF{body.left(), body.bottom() - radius});
    path.closeSubpath();

    return transform.map(path);
}

void paintPane(const QStyleOptionTabWidgetFrame& option, QPainter& painter, const Fooyin::TabAppearance& appearance)
{
    const auto& pane{appearance.pane};
    if(pane.backgroundOpacity == 0 && (pane.borderWidth <= 0 || pane.borderOpacity == 0)) {
        return;
    }

    const auto group     = Fooyin::Style::colourGroup(option);
    const auto path      = panePath(option, appearance);
    const bool classic   = pane.reliefStyle == Fooyin::TabAppearance::Pane::ReliefStyle::Classic;
    const double relief  = pane.reliefOpacity / 100.0;
    const double opacity = painter.opacity();

    if(classic && pane.borderWidth > 0 && pane.borderOpacity > 0 && pane.reliefOpacity > 0) {
        painter.setOpacity(opacity * pane.borderOpacity / 100.0 * relief * 0.35);
        painter.strokePath(path, QPen{option.palette.brush(group, QPalette::Shadow), pane.borderWidth + 2.0});
    }

    if(pane.backgroundOpacity > 0) {
        painter.setOpacity(opacity * pane.backgroundOpacity / 100.0);
        painter.fillPath(path, Fooyin::resolveBrush(pane.backgroundColour, option.palette, group, path.boundingRect(),
                                                    Qt::Horizontal, option.direction == Qt::RightToLeft));
    }
    if(pane.borderWidth > 0 && pane.borderOpacity > 0) {
        painter.setOpacity(opacity * pane.borderOpacity / 100.0);

        if(classic && pane.reliefOpacity > 0 && isConnected(appearance.design)) {
            const QRect visibleTab = option.selectedTabRect.intersected(option.tabBarRect);
            if(!visibleTab.isEmpty()) {
                const auto transform = tabTransform(option.shape, QRectF{visibleTab});
                const auto tab       = transform.inverted().mapRect(QRectF{visibleTab});

                QLinearGradient highlight{transform.map(tab.topLeft()), transform.map(tab.bottomLeft())};
                auto light = option.palette.color(group, QPalette::Light);
                light.setAlphaF(light.alphaF() * static_cast<float>(relief * 0.18));
                highlight.setColorAt(0.0, light);
                light.setAlphaF(0.0);
                highlight.setColorAt(1.0, light);

                painter.save();
                painter.setClipRect(visibleTab, Qt::IntersectClip);
                painter.fillPath(path, highlight);
                painter.restore();
            }
        }

        painter.strokePath(path,
                           QPen{Fooyin::resolveBrush(pane.borderColour, option.palette, group), pane.borderWidth});

        if(classic && pane.reliefOpacity > 0) {
            painter.strokePath(
                path, QPen{Fooyin::Style::opacityBrush(option.palette.brush(group, QPalette::Light), relief * 0.4),
                           pane.borderWidth});
        }
        else if(pane.reliefOpacity > 0) {
            QPainterPathStroker stroker;
            stroker.setWidth(pane.borderWidth);

            const auto border      = stroker.createStroke(path);
            const bool sunken      = pane.reliefStyle == Fooyin::TabAppearance::Pane::ReliefStyle::Sunken;
            const auto topLeft     = sunken ? QPalette::Shadow : QPalette::Light;
            const auto bottomRight = sunken ? QPalette::Light : QPalette::Shadow;

            painter.fillPath(border.subtracted(path.translated(pane.borderWidth, pane.borderWidth)),
                             Fooyin::Style::opacityBrush(option.palette.brush(group, topLeft), relief));
            painter.fillPath(border.subtracted(path.translated(-pane.borderWidth, -pane.borderWidth)),
                             Fooyin::Style::opacityBrush(option.palette.brush(group, bottomRight), relief));
        }
    }

    painter.setOpacity(opacity);
}

void drawConnectedTab(const QStyleOptionTab& option, const QTabWidget& tabs, QPainter& painter,
                      const Fooyin::TabAppearance& appearance)
{
    const auto* bar = tabs.tabBar();

    QStyleOptionTabWidgetFrame frame;
    frame.initFrom(&tabs);

    frame.shape           = option.shape;
    frame.lineWidth       = tabs.style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, &tabs);
    frame.tabBarRect      = bar->geometry();
    frame.tabBarSize      = bar->size();
    frame.selectedTabRect = option.rect.translated(bar->pos());
    frame.rect            = tabs.rect();
    frame.rect            = tabs.style()->subElementRect(QStyle::SE_TabWidgetTabPane, &frame, &tabs);

    painter.save();
    painter.translate(-bar->pos());
    paintPane(frame, painter, appearance);
    painter.restore();
}

bool verticalTab(const QStyleOptionTab& option)
{
    return option.shape == QTabBar::RoundedWest || option.shape == QTabBar::TriangularWest
        || option.shape == QTabBar::RoundedEast || option.shape == QTabBar::TriangularEast;
}

const Fooyin::TabAppearance::Surface& tabSurface(const QStyleOptionTab& option, const Fooyin::TabAppearance& appearance)
{
    const bool selected = option.state & QStyle::State_Selected;
    if(!(option.state & QStyle::State_Enabled)) {
        return selected ? appearance.selectedDisabled : appearance.disabled;
    }
    if(option.state & QStyle::State_Sunken) {
        return appearance.pressed;
    }
    if(option.state & QStyle::State_MouseOver) {
        return selected ? appearance.selectedHovered : appearance.hovered;
    }
    return selected ? appearance.selected : appearance.normal;
}

double maximumBorderWidth(const Fooyin::TabAppearance& appearance)
{
    return std::max({appearance.normal.borderWidth, appearance.hovered.borderWidth, appearance.pressed.borderWidth,
                     appearance.selected.borderWidth, appearance.selectedHovered.borderWidth,
                     appearance.disabled.borderWidth, appearance.selectedDisabled.borderWidth,
                     isConnected(appearance.design) ? appearance.pane.borderWidth : 0.0});
}

int contentPadding(int padding, const Fooyin::TabAppearance& appearance)
{
    const double border = maximumBorderWidth(appearance);
    const double inset  = std::max(appearance.inset, border / 2.0);
    return std::max(padding, static_cast<int>(std::ceil(inset + (border / 2.0))));
}

int paneMargin(const QStyleOptionTabWidgetFrame& option, const Fooyin::TabAppearance& appearance)
{
    if(option.lineWidth <= 0) {
        return 0;
    }
    const int minimum = appearance.pane.borderWidth > 0 && appearance.pane.borderOpacity > 0 ? 4 : 2;
    return std::max(minimum, static_cast<int>(std::ceil(appearance.pane.borderWidth)));
}
} // namespace

namespace Fooyin::Style::Tab {
void drawStripBackground(QWidget* widget, const TabAppearance& appearance)
{
    if(appearance.strip.backgroundOpacity <= 0) {
        return;
    }

    QRect rect;
    Qt::Orientation orientation{Qt::Horizontal};
    QTabBar::Shape shape{QTabBar::RoundedNorth};

    if(const auto* tabs = qobject_cast<const QTabWidget*>(widget)) {
        const auto* bar = tabs->tabBar();
        if(!bar->isVisible() || bar->style() != tabs->style()) {
            return;
        }

        rect  = bar->geometry();
        shape = bar->shape();

        if(tabs->tabPosition() == QTabWidget::North || tabs->tabPosition() == QTabWidget::South) {
            rect.setLeft(0);
            rect.setRight(tabs->width() - 1);
        }
        else {
            rect.setTop(0);
            rect.setBottom(tabs->height() - 1);
            orientation = Qt::Vertical;
        }
    }
    else if(const auto* bar = qobject_cast<const QTabBar*>(widget)) {
        const auto* parentTabs = qobject_cast<const QTabWidget*>(bar->parentWidget());
        if(parentTabs && parentTabs->tabBar() == bar && parentTabs->style() == bar->style()) {
            // The parent paints the whole strip, including the spare space beyond its tab bar
            return;
        }

        rect  = bar->rect();
        shape = bar->shape();

        if(bar->shape() == QTabBar::RoundedWest || bar->shape() == QTabBar::RoundedEast
           || bar->shape() == QTabBar::TriangularWest || bar->shape() == QTabBar::TriangularEast) {
            orientation = Qt::Vertical;
        }
    }
    else {
        return;
    }

    QStyleOption option;
    option.initFrom(widget);

    QPainter painter{widget};
    painter.setOpacity(appearance.strip.backgroundOpacity / 100.0);

    const auto brush
        = resolveBrush(appearance.strip.backgroundColour, option.palette, colourGroup(option), QRectF{rect},
                       orientation, orientation == Qt::Horizontal && option.direction == Qt::RightToLeft);
    if(appearance.strip.radius <= 0) {
        painter.fillRect(rect, brush);
        return;
    }

    const auto transform = tabTransform(shape, QRectF{rect});
    const QRectF body    = transform.inverted().mapRect(QRectF{rect});
    const double radius  = std::min({appearance.strip.radius, body.width() / 2.0, body.height() / 2.0});

    QPainterPath path;
    path.moveTo(body.bottomLeft());
    path.lineTo(body.left(), body.top() + radius);
    path.quadTo(body.topLeft(), QPointF{body.left() + radius, body.top()});
    path.lineTo(body.right() - radius, body.top());
    path.quadTo(body.topRight(), QPointF{body.right(), body.top() + radius});
    path.lineTo(body.bottomRight());
    path.closeSubpath();

    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillPath(transform.map(path), brush);
}

void drawCloseIndicator(const QStyleOption& option, QPainter& painter, const TabAppearance& appearance)
{
    const bool enabled = option.state & QStyle::State_Enabled;
    const bool hovered = enabled && (option.state & QStyle::State_MouseOver);
    const bool pressed = enabled && (option.state & QStyle::State_Sunken);

    const auto& state = stateAppearance(appearance.close, enabled, hovered, pressed);
    const QRectF body = QRectF{option.rect}.adjusted(appearance.close.inset, appearance.close.inset,
                                                     -appearance.close.inset, -appearance.close.inset);
    if(body.isEmpty()) {
        return;
    }

    const auto group = colourGroup(option);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const double opacity = painter.opacity();

    if(state.backgroundOpacity > 0) {
        painter.setOpacity(painter.opacity() * state.backgroundOpacity / 100.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(resolveBrush(state.backgroundColour, option.palette, group, body, Qt::Horizontal,
                                      option.direction == Qt::RightToLeft));
        const double radius = std::min({appearance.close.radius, body.width() / 2.0, body.height() / 2.0});
        painter.drawRoundedRect(body, radius, radius);
    }

    painter.setOpacity(opacity);
    painter.setPen(QPen{resolveBrush(state.foregroundColour, option.palette, group), appearance.close.glyphWidth,
                        Qt::SolidLine, Qt::RoundCap});

    const double radius
        = std::max(0.0, std::min({appearance.close.glyphSize, body.width() - appearance.close.glyphWidth,
                                  body.height() - appearance.close.glyphWidth})
                            / 2.0);
    if(radius > 0 && appearance.close.glyphWidth > 0) {
        const QPointF centre = option.rect.center();
        painter.drawLine(centre + QPointF{-radius, -radius}, centre + QPointF{radius, radius});
        painter.drawLine(centre + QPointF{-radius, radius}, centre + QPointF{radius, -radius});
    }

    painter.restore();
}

void drawShape(const QStyleOptionTab& option, QPainter& painter, const QWidget* widget, const TabAppearance& appearance)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const QRectF rect{option.rect};

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    const auto* tabs     = isConnected(appearance.design) ? tabWidget(widget) : nullptr;
    const bool connected = tabs && (option.state & QStyle::State_Selected);
    if(connected) {
        drawConnectedTab(option, *tabs, painter, appearance);
    }
    const auto transform = tabTransform(option.shape, rect);
    painter.setTransform(transform, true);

    const bool vertical = verticalTab(option);
    const double width  = vertical ? rect.height() : rect.width();
    const double height = vertical ? rect.width() : rect.height();
    const double inset  = std::max(appearance.inset, maximumBorderWidth(appearance) / 2.0);

    const QRectF body{inset, inset, width - (inset * 2.0), height - (inset * 2.0)};
    if(body.isEmpty()) {
        painter.restore();
        return;
    }

    const auto& surface = tabSurface(option, appearance);
    const auto group    = colourGroup(option);
    const double radius = std::min({appearance.radius, body.width() / 2.0, body.height() / 2.0});

    painter.setPen(Qt::NoPen);

    if(!connected && surface.backgroundOpacity > 0) {
        const double opacity = painter.opacity();
        painter.setOpacity(opacity * surface.backgroundOpacity / 100.0);
        auto brush
            = resolveBrush(surface.backgroundColour, option.palette, group, transform.mapRect(body),
                           vertical ? Qt::Vertical : Qt::Horizontal, !vertical && option.direction == Qt::RightToLeft);
        // Resolve directions in widget coordinates, then undo the tab's rotation or reflection for the brush
        brush.setTransform(transform.inverted());
        painter.setBrush(brush);
        painter.drawRoundedRect(body, radius, radius);
        painter.setOpacity(opacity);
    }
    if(!connected && surface.borderWidth > 0) {
        painter.setPen(QPen{resolveBrush(surface.borderColour, option.palette, group), surface.borderWidth});
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(body, radius, radius);
    }
    if((option.state & QStyle::State_Selected) && appearance.accent.width > 0) {
        const double half   = appearance.accent.width / 2.0;
        const double margin = std::max(appearance.accent.margin, half);

        if(width > margin * 2.0) {
            const double offset = std::clamp(appearance.accent.inset, half, std::max(half, height - half));
            const double y = appearance.accent.edge == TabAppearance::Accent::Edge::Outer ? offset : height - offset;
            painter.setPen(QPen{resolveBrush(appearance.accent.colour, option.palette, group), appearance.accent.width,
                                Qt::SolidLine, appearance.accent.rounded ? Qt::RoundCap : Qt::FlatCap});
            painter.drawLine(QPointF{margin, y}, QPointF{width - margin, y});
        }
    }
    if((option.state & QStyle::State_Enabled) && (option.state & QStyle::State_HasFocus)
       && (!appearance.focus.keyboardOnly || (option.state & QStyle::State_KeyboardFocusChange))
       && appearance.focus.borderWidth > 0) {
        const double focusInset = std::max(appearance.focus.inset, appearance.focus.borderWidth / 2.0);
        const QRectF focus{focusInset, focusInset, width - (focusInset * 2.0),
                           height - (focusInset * 2.0) - appearance.focus.pageGap};

        if(!focus.isEmpty()) {
            const double focusRadius = std::min({appearance.focus.radius, focus.width() / 2.0, focus.height() / 2.0});
            painter.setPen(
                QPen{resolveBrush(appearance.focus.borderColour, option.palette, group), appearance.focus.borderWidth});
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(focus, focusRadius, focusRadius);
        }
    }

    painter.restore();
}

void drawPane(const QStyleOptionTabWidgetFrame& option, QPainter& painter, const TabAppearance& appearance,
              const QWidget* widget)
{
    if(option.lineWidth <= 0 || option.rect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(option.rect, Qt::IntersectClip);

    auto frame{option};
    if(const auto* tabs = qobject_cast<const QTabWidget*>(widget)) {
        frame.selectedTabRect = selectedTabRect(*tabs->tabBar(), appearance).translated(tabs->tabBar()->pos());
    }
    paintPane(frame, painter, appearance);

    painter.restore();
}

void drawLabel(const QCommonStyle& style, const QStyleOptionTab& option, QPainter& painter, const QWidget* widget,
               const TabAppearance& appearance)
{
    const auto& surface = tabSurface(option, appearance);
    const auto group    = colourGroup(option);
    const auto role     = widget ? widget->foregroundRole() : QPalette::WindowText;

    auto label{option};
    label.state &= ~QStyle::State_HasFocus;
    label.palette.setCurrentColorGroup(group);
    label.palette.setBrush(group, role, resolveBrush(surface.foregroundColour, option.palette, group));

    if(surface.tintIcon && !label.icon.isNull()) {
        label.icon = tintedIcon(label.icon, label.iconSize, painter.device()->devicePixelRatioF(),
                                option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled,
                                option.state & QStyle::State_Selected ? QIcon::On : QIcon::Off,
                                resolveBrush(surface.iconColour, option.palette, group).color());
    }

    style.QCommonStyle::drawControl(QStyle::CE_TabBarTabLabel, &label, &painter, widget);
}

QSize sizeFromContents(const QStyleOptionTab& option, const QSize& contentsSize, const TabAppearance& appearance)
{
    const auto margins = stripMargins(option, appearance);

    QSize size{contentsSize};
    size.rwidth() += margins.left() + margins.right();
    size.rheight() += margins.top() + margins.bottom();
    if(verticalTab(option)) {
        size.setWidth(std::max(size.width(), appearance.minimumThickness));
    }
    else {
        size.setHeight(std::max(size.height(), appearance.minimumThickness));
    }
    return size;
}

QRect paddedTabRect(const QStyleOptionTab& option, const TabAppearance& appearance)
{
    return option.rect.marginsRemoved(stripMargins(option, appearance));
}

QSize paneSize(const QStyleOptionTabWidgetFrame& option, const QSize& contentsSize, const TabAppearance& appearance)
{
    // Preserve QCommonStyle’s minimum size, including in document mode
    const int margin = std::max(2, paneMargin(option, appearance));
    return contentsSize + QSize{margin * 2, margin * 2};
}

QRect paneContentsRect(const QCommonStyle& style, const QStyleOptionTabWidgetFrame& option, const QWidget* widget,
                       const TabAppearance& appearance)
{
    const int margin = paneMargin(option, appearance);
    return style.QCommonStyle::subElementRect(QStyle::SE_TabWidgetTabPane, &option, widget)
        .adjusted(margin, margin, -margin, -margin);
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const TabAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_TabBarTabHSpace:
            return contentPadding(appearance.horizontalPadding, appearance) * 2;
        case QStyle::PM_TabBarTabVSpace:
            return contentPadding(appearance.verticalPadding, appearance) * 2;
        case QStyle::PM_TabCloseIndicatorWidth:
        case QStyle::PM_TabCloseIndicatorHeight:
            return appearance.close.extent;
        case QStyle::PM_TabBarTabOverlap:
        case QStyle::PM_TabBarTabShiftHorizontal:
        case QStyle::PM_TabBarTabShiftVertical:
        case QStyle::PM_TabBarBaseHeight:
        case QStyle::PM_TabBarBaseOverlap:
            return 0;
        default:
            return {};
    }
}

std::optional<int> styleHint(QStyle::StyleHint hint, const TabAppearance& appearance)
{
    if(hint != QStyle::SH_TabBar_Alignment) {
        return {};
    }

    // Qt uses horizontal alignment flags for both orientations and mirrors horizontal tab bars itself
    switch(appearance.alignment) {
        case TabAppearance::Alignment::Leading:
            return Qt::AlignLeft;
        case TabAppearance::Alignment::Centre:
            return Qt::AlignHCenter;
        case TabAppearance::Alignment::Trailing:
            return Qt::AlignRight;
    }
    return Qt::AlignLeft;
}

void polish(QWidget* widget)
{
    const auto* bar = qobject_cast<QTabBar*>(widget);
    if(!bar) {
        return;
    }
    // QTabBar positions existing buttons after a style change without resizing them
    for(int index{0}; index < bar->count(); ++index) {
        for(const auto side : {QTabBar::LeftSide, QTabBar::RightSide}) {
            auto* button = qobject_cast<QAbstractButton*>(bar->tabButton(index, side));
            if(button && button->sizeHint().isValid() && button->size() != button->sizeHint()) {
                if(!button->property(ButtonSizeProperty).isValid()) {
                    button->setProperty(ButtonSizeProperty, button->size());
                }
                button->resize(button->sizeHint());
            }
        }
    }
}

void unpolish(QWidget* widget)
{
    auto* bar = qobject_cast<QTabBar*>(widget);
    if(!bar) {
        return;
    }

    bar->setProperty(SelectedRectProperty, {});

    for(int index{0}; index < bar->count(); ++index) {
        for(const auto side : {QTabBar::LeftSide, QTabBar::RightSide}) {
            if(auto* button = bar->tabButton(index, side)) {
                if(const auto size = button->property(ButtonSizeProperty); size.isValid()) {
                    button->resize(size.toSize());
                    button->setProperty(ButtonSizeProperty, {});
                }
            }
        }
    }
}

void updatePane(QTabBar* bar)
{
    auto* tabs = tabWidget(bar);
    if(!tabs) {
        return;
    }
    // Scrolling an overflowing tab bar moves the selected tab without changing the page, which leaves the pane's border
    // opening at its old position
    const QRect rect = bar->tabRect(bar->currentIndex()).intersected(bar->rect()).translated(bar->pos());
    if(const auto previous = bar->property(SelectedRectProperty); !previous.isValid() || previous.toRect() != rect) {
        bar->setProperty(SelectedRectProperty, rect);
        tabs->update();
    }
}
} // namespace Fooyin::Style::Tab
