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

#include "menustyle.h"

#include "styleutils.h"
#include "theme/menuappearance.h"

#include <QComboBox>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionMenuItem>
#include <QVariant>

#include <array>
#include <cmath>

constexpr auto BackgroundAttributesProperty = "_fooyin_menu_background";

namespace {
bool hasCheck(const QStyleOptionMenuItem& option, const QWidget* widget)
{
    const bool combo = qobject_cast<const QComboBox*>(widget);
    return option.checkType != QStyleOptionMenuItem::NotCheckable
        && (!combo || (option.state & (QStyle::State_On | QStyle::State_Off)));
}

int checkExtent(const QStyle& style, const QStyleOptionMenuItem& option, const QWidget* widget,
                const Fooyin::MenuAppearance& appearance)
{
    return std::max({appearance.checkExtent, style.pixelMetric(QStyle::PM_IndicatorWidth, &option, widget),
                     style.pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, &option, widget)});
}

int checkColumn(const QStyle& style, const QStyleOptionMenuItem& option, const QWidget* widget,
                const Fooyin::MenuAppearance& appearance)
{
    // Combo delegates can mark every entry checkable, even when no check indicator is drawn
    const bool menuChecks = option.menuHasCheckableItems && !qobject_cast<const QComboBox*>(widget);
    return menuChecks || hasCheck(option, widget)
             ? checkExtent(style, option, widget, appearance) + appearance.columnSpacing
             : 0;
}

int iconColumn(const QStyleOptionMenuItem& option, const QWidget* widget, const Fooyin::MenuAppearance& appearance)
{
    if(const auto* combo = qobject_cast<const QComboBox*>(widget)) {
        // Qt supplies maxIconWidth for text-only combo entries as well
        return option.icon.isNull()
                 ? 0
                 : std::max(appearance.iconExtent, combo->iconSize().width() + appearance.columnSpacing);
    }
    return option.maxIconWidth > 0 || !option.icon.isNull()
             ? std::max({appearance.iconExtent, option.maxIconWidth, appearance.iconSize + appearance.columnSpacing})
             : 0;
}

QFont itemFont(const QStyleOptionMenuItem& option, const Fooyin::MenuAppearance::Separator& separator)
{
    auto font{option.font};
    if(option.menuItemType == QStyleOptionMenuItem::DefaultItem
       || (option.menuItemType == QStyleOptionMenuItem::Separator && separator.bold)) {
        font.setBold(true);
    }
    return font;
}

template <typename Appearance>
const Fooyin::MenuAppearance::State& itemState(const Appearance& appearance, const QStyleOption& option,
                                               bool highlighted)
{
    if(!(option.state & QStyle::State_Enabled)) {
        return appearance.disabled;
    }
    if(highlighted) {
        return option.state & QStyle::State_Sunken ? appearance.pressed : appearance.selected;
    }
    return appearance.normal;
}

template <typename Appearance>
int itemPadding(int padding, const Appearance& appearance)
{
    const double border = std::max({appearance.normal.borderWidth, appearance.selected.borderWidth,
                                    appearance.pressed.borderWidth, appearance.disabled.borderWidth});
    return std::max(padding, static_cast<int>(std::ceil(std::max(appearance.inset, border / 2.0) + border / 2.0)));
}

QPalette::ColorGroup surfaceGroup(const QStyleOption& option, const QWidget* widget)
{
    if(widget) {
        return !widget->isEnabled()               ? QPalette::Disabled
             : widget->window()->isActiveWindow() ? QPalette::Active
                                                  : QPalette::Inactive;
    }
    return Fooyin::Style::colourGroup(option);
}

QPainterPath surfacePath(const QRect& bounds, const Fooyin::MenuAppearance::Surface& surface)
{
    const double inset = surface.borderWidth / 2.0;
    const QRectF rect  = QRectF{bounds}.adjusted(inset, inset, -inset, -inset);
    QPainterPath path;
    if(!rect.isEmpty()) {
        const double radius = std::min({surface.radius, rect.width() / 2.0, rect.height() / 2.0});
        path.addRoundedRect(rect, radius, radius);
    }
    return path;
}

void clipMenuItems(QPainter& painter, const QWidget* widget, const Fooyin::MenuAppearance& appearance)
{
    if(qobject_cast<const QMenu*>(widget)) {
        painter.setClipPath(surfacePath(widget->rect(), appearance.panel), Qt::IntersectClip);
    }
}

void drawSurface(const QStyleOption& option, QPainter& painter, const Fooyin::MenuAppearance::Surface& surface,
                 QPalette::ColorGroup group)
{
    const auto path = surfacePath(option.rect, surface);
    if(path.isEmpty()) {
        return;
    }

    const double opacity = painter.opacity();
    if(surface.backgroundOpacity > 0) {
        painter.setOpacity(opacity * surface.backgroundOpacity / 100.0);
        painter.fillPath(path,
                         Fooyin::resolveBrush(surface.backgroundColour, option.palette, group, path.boundingRect(),
                                              Qt::Horizontal, option.direction == Qt::RightToLeft));
        painter.setOpacity(opacity);
    }

    const QRectF rect = path.boundingRect();
    if(surface.borderWidth > 0 && surface.borderOpacity > 0) {
        painter.setOpacity(opacity * surface.borderOpacity / 100.0);
        QBrush brush      = Fooyin::resolveBrush(surface.borderColour, option.palette, group);
        const auto border = brush.color();
        if(surface.reliefOpacity > 0) {
            const auto shade = [&](QPalette::ColorRole role) {
                const auto tint    = option.palette.color(group, role);
                const float amount = static_cast<float>(surface.reliefOpacity) / 100.0F * tint.alphaF();
                return QColor::fromRgbF(std::lerp(border.redF(), tint.redF(), amount),
                                        std::lerp(border.greenF(), tint.greenF(), amount),
                                        std::lerp(border.blueF(), tint.blueF(), amount), border.alphaF());
            };

            QLinearGradient gradient{rect.topLeft(), rect.bottomLeft()};
            gradient.setColorAt(0.0, shade(QPalette::Light));
            gradient.setColorAt(1.0, shade(QPalette::Shadow));
            brush = QBrush{gradient};
        }
        painter.strokePath(path, QPen{brush, surface.borderWidth});
        painter.setOpacity(opacity);
    }
}

void drawSelection(const QStyleOption& option, QPainter& painter, const Fooyin::MenuAppearance::State& state,
                   double inset, double radius)
{
    inset = std::max(inset, state.borderWidth / 2.0);

    const QRectF rect = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(rect.isEmpty()) {
        return;
    }

    const auto group = Fooyin::Style::colourGroup(option);
    radius           = std::min({radius, rect.width() / 2.0, rect.height() / 2.0});

    if(state.backgroundOpacity > 0) {
        const double opacity = painter.opacity();
        painter.setOpacity(opacity * state.backgroundOpacity / 100.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Fooyin::resolveBrush(state.backgroundColour, option.palette, group, rect, Qt::Horizontal,
                                              option.direction == Qt::RightToLeft));
        painter.drawRoundedRect(rect, radius, radius);
        painter.setOpacity(opacity);
    }
    if(state.borderWidth > 0) {
        painter.setPen(QPen{Fooyin::resolveBrush(state.borderColour, option.palette, group), state.borderWidth});
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect, radius, radius);
    }
}

void drawSection(const QStyleOptionMenuItem& option, QPainter& painter,
                 const Fooyin::MenuAppearance::Separator& appearance)
{
    const auto logical = [&](const QRect& rect) {
        return QStyle::visualRect(option.direction, option.rect, rect);
    };

    QRect content = option.rect.adjusted(appearance.inset, 0, -appearance.inset, 0);
    if(!option.icon.isNull()) {
        const int extent = option.fontMetrics.height();
        const QRect icon = logical(QRect{content.left(), content.center().y() - (extent / 2), extent, extent});
        option.icon.paint(&painter, icon, Qt::AlignCenter,
                          option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled);
        content.setLeft(content.left() + extent + appearance.spacing);
    }

    const auto group = Fooyin::Style::colourGroup(option);
    if(!option.text.isEmpty()) {
        const QFont font = itemFont(option, appearance);
        painter.setFont(font);

        const QFontMetrics fm{font};
        const int width = std::min(content.width(), fm.horizontalAdvance(option.text));
        const QRect text{content.left(), content.top(), width, content.height()};

        painter.setPen(QPen{Fooyin::resolveBrush(appearance.foregroundColour, option.palette, group), 1.0});
        painter.drawText(logical(text), QStyle::visualAlignment(option.direction, Qt::AlignLeft) | Qt::AlignVCenter,
                         option.text);
        content.setLeft(content.left() + width + appearance.spacing);
    }
    if(content.width() > 0 && appearance.lineWidth > 0) {
        const QRect line = logical(content);
        painter.setPen(QPen{Fooyin::resolveBrush(appearance.lineColour, option.palette, group), appearance.lineWidth});
        const double y = QRectF{line}.center().y();
        painter.drawLine(QPointF{static_cast<double>(line.left()), y}, QPointF{static_cast<double>(line.right()), y});
    }
}
} // namespace

namespace Fooyin::Style::Menu {
void drawPanel(const QStyleOption& option, QPainter& painter, const QWidget* widget, const MenuAppearance& appearance)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    drawSurface(option, painter, appearance.panel, surfaceGroup(option, widget));
    painter.restore();
}

void drawMenuBarBackground(const QStyleOption& option, QPainter& painter, const QWidget* widget,
                           const MenuAppearance& appearance)
{
    auto panel{option};
    if(widget) {
        panel.rect = widget->rect();
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    drawSurface(panel, painter, appearance.bar.panel, surfaceGroup(option, widget));
    painter.restore();
}

void drawItem(const QStyle& style, const QStyleOptionMenuItem& option, QPainter& painter, const QWidget* widget,
              const MenuAppearance& appearance)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    clipMenuItems(painter, widget, appearance);

    if(option.menuItemType == QStyleOptionMenuItem::Separator) {
        drawSection(option, painter, appearance.separator);
        painter.restore();
        return;
    }

    const auto group  = colourGroup(option);
    const auto visual = [&](const QRect& rect) {
        return QStyle::visualRect(option.direction, option.rect, rect);
    };

    const auto& state = itemState(appearance, option, option.state & QStyle::State_Selected);
    drawSelection(option, painter, state, appearance.inset, appearance.radius);

    const int padding = itemPadding(appearance.horizontalPadding, appearance);
    QRect content     = option.rect.adjusted(padding, 0, -padding, 0);

    if(hasCheck(option, widget)) {
        QStyleOption indicator{option};
        indicator.rect = visual(
            QRect{content.left(), content.top(), checkExtent(style, option, widget, appearance), content.height()});
        indicator.state &= ~(QStyle::State_HasFocus | QStyle::State_Selected | QStyle::State_On | QStyle::State_Off);
        indicator.state |= option.checked ? QStyle::State_On : QStyle::State_Off;
        style.drawPrimitive(option.checkType == QStyleOptionMenuItem::Exclusive ? QStyle::PE_IndicatorRadioButton
                                                                                : QStyle::PE_IndicatorCheckBox,
                            &indicator, &painter, widget);
    }

    content.setLeft(content.left() + checkColumn(style, option, widget, appearance));

    if(!option.icon.isNull()) {
        const int column  = iconColumn(option, widget, appearance) - appearance.columnSpacing;
        const QRect rect  = visual(QRect{content.left(), content.top(), column, content.height()});
        const auto* combo = qobject_cast<const QComboBox*>(widget);
        const int extent  = appearance.iconSize;
        const QSize size  = combo ? combo->iconSize() : QSize{extent, extent};
        const auto mode   = option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled;
        const auto icon   = state.tintIcon ? tintedIcon(option.icon, size, painter.device()->devicePixelRatioF(), mode,
                                                        option.checked ? QIcon::On : QIcon::Off,
                                                        resolveBrush(state.iconColour, option.palette, group).color())
                                           : option.icon;
        const auto pixmap
            = icon.pixmap(size, painter.device()->devicePixelRatioF(), mode, option.checked ? QIcon::On : QIcon::Off);
        style.drawItemPixmap(&painter, rect, Qt::AlignCenter, pixmap);
    }

    content.setLeft(content.left() + iconColumn(option, widget, appearance));

    const QRect arrow{content.right() - appearance.arrowExtent + 1, content.top(), appearance.arrowExtent,
                      content.height()};
    content.setRight(arrow.left() - appearance.columnSpacing);

    const qsizetype tab = option.text.indexOf(u'\t');
    if(tab >= 0) {
        const auto shortcutText = option.text.mid(tab + 1);
        const int width = std::max(option.reservedShortcutWidth, option.fontMetrics.horizontalAdvance(shortcutText));
        const QRect shortcut{content.right() - width + 1, content.top(), width, content.height()};
        content.setRight(shortcut.left() - appearance.shortcutSpacing);

        painter.setFont(option.font);
        painter.setPen(QPen{resolveBrush(state.foregroundColour, option.palette, group), 1.0});
        painter.drawText(visual(shortcut),
                         QStyle::visualAlignment(option.direction, Qt::AlignRight) | Qt::AlignVCenter
                             | Qt::TextSingleLine,
                         shortcutText);
    }

    painter.setFont(itemFont(option, appearance.separator));
    painter.setPen(QPen{resolveBrush(state.foregroundColour, option.palette, group), 1.0});

    auto alignment = QStyle::visualAlignment(option.direction, Qt::AlignLeft) | Qt::AlignVCenter | Qt::TextSingleLine
                   | Qt::TextShowMnemonic;
    if(!style.styleHint(QStyle::SH_UnderlineShortcut, &option, widget)) {
        alignment |= Qt::TextHideMnemonic;
    }

    painter.drawText(visual(content), alignment, option.text.left(tab < 0 ? option.text.size() : tab));

    if(option.menuItemType == QStyleOptionMenuItem::SubMenu && appearance.arrowWidth > 0 && appearance.arrowSize > 0
       && appearance.arrowExtent > 0) {
        const auto centre      = QRectF{visual(arrow)}.center();
        const double direction = option.direction == Qt::RightToLeft ? -1.0 : 1.0;
        const double size
            = std::max(0.0, std::min({appearance.arrowSize, static_cast<double>(arrow.height()) - appearance.arrowWidth,
                                      (static_cast<double>(arrow.width()) * 2.0) - (appearance.arrowWidth * 2.0)}));
        const std::array points{centre + QPointF{-size / 4.0 * direction, -size / 2.0},
                                centre + QPointF{size / 4.0 * direction, 0.0},
                                centre + QPointF{-size / 4.0 * direction, size / 2.0}};
        painter.setPen(QPen{resolveBrush(state.foregroundColour, option.palette, group), appearance.arrowWidth,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        painter.drawPolyline(points.data(), static_cast<int>(points.size()));
    }

    painter.restore();
}

void drawMenuBarItem(const QStyle& style, const QStyleOptionMenuItem& option, QPainter& painter, const QWidget* widget,
                     const MenuAppearance& appearance)
{
    auto item{option};
    if(widget) {
        item.state.setFlag(QStyle::State_Active, widget->window()->isActiveWindow());
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setClipRect(item.rect, Qt::IntersectClip);
    drawMenuBarBackground(item, painter, widget, appearance);
    painter.setClipPath(surfacePath(widget ? widget->rect() : item.rect, appearance.bar.panel), Qt::IntersectClip);

    const bool selected = item.state & QStyle::State_Selected;
    const auto& state   = itemState(appearance.bar, item, selected || (item.state & QStyle::State_MouseOver));
    drawSelection(item, painter, state, appearance.bar.inset, appearance.bar.radius);

    auto palette{item.palette};
    palette.setCurrentColorGroup(colourGroup(item));

    if(!item.icon.isNull()) {
        const int extent = appearance.iconSize;
        const auto mode  = item.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled;
        const auto icon
            = state.tintIcon
                ? tintedIcon(item.icon, QSize{extent, extent}, painter.device()->devicePixelRatioF(), mode, QIcon::Off,
                             resolveBrush(state.iconColour, palette, palette.currentColorGroup()).color())
                : item.icon;
        const auto pixmap = icon.pixmap(QSize{extent, extent}, painter.device()->devicePixelRatioF(), mode);
        style.drawItemPixmap(&painter, item.rect, Qt::AlignCenter, pixmap);
    }
    else {
        if(widget) {
            painter.setFont(widget->font());
        }
        int flags = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextSingleLine;
        if(!style.styleHint(QStyle::SH_UnderlineShortcut, &item, widget)) {
            flags |= Qt::TextHideMnemonic;
        }
        palette.setBrush(palette.currentColorGroup(), QPalette::WindowText,
                         resolveBrush(state.foregroundColour, palette, palette.currentColorGroup()));
        style.drawItemText(&painter, item.rect, flags, palette, item.state & QStyle::State_Enabled, item.text,
                           QPalette::WindowText);
    }

    const auto& focus = appearance.bar.focus;
    if(selected && (item.state & QStyle::State_HasFocus) && (item.state & QStyle::State_Enabled)
       && (!focus.keyboardOnly || (item.state & QStyle::State_KeyboardFocusChange)) && focus.borderWidth > 0) {
        const double inset = std::max(focus.inset, focus.borderWidth / 2.0);
        const QRectF rect  = QRectF{item.rect}.adjusted(inset, inset, -inset, -inset);
        if(!rect.isEmpty()) {
            painter.setPen(
                QPen{resolveBrush(focus.borderColour, palette, palette.currentColorGroup()), focus.borderWidth});
            painter.setBrush(Qt::NoBrush);
            const double radius = std::min({focus.radius, rect.width() / 2.0, rect.height() / 2.0});
            painter.drawRoundedRect(rect, radius, radius);
        }
    }

    painter.restore();
}

void drawAuxiliary(QStyle::ControlElement element, const QStyleOption& option, QPainter& painter, const QWidget* widget,
                   const MenuAppearance& appearance)
{
    auto item{option};
    if(widget) {
        item.state.setFlag(QStyle::State_Enabled, widget->isEnabled());
        item.state.setFlag(QStyle::State_Active, widget->window()->isActiveWindow());
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    clipMenuItems(painter, widget, appearance);

    const auto& state = itemState(appearance, item, item.state & QStyle::State_Selected);
    drawSelection(item, painter, state, appearance.inset, appearance.radius);

    if(element == QStyle::CE_MenuScroller) {
        drawChevron(item, item.rect, painter, !(item.state & QStyle::State_DownArrow),
                    resolveBrush(state.foregroundColour, item.palette, colourGroup(item)));
    }
    else if(appearance.separator.lineWidth > 0) {
        painter.setPen(QPen{resolveBrush(appearance.separator.lineColour, item.palette, colourGroup(item)),
                            appearance.separator.lineWidth, Qt::DashLine});
        const QRectF rect = QRectF{item.rect}.adjusted(appearance.separator.inset, 0, -appearance.separator.inset, 0);
        painter.drawLine(QPointF{rect.left(), rect.center().y()}, QPointF{rect.right(), rect.center().y()});
    }

    painter.restore();
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const MenuAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_MenuPanelWidth:
            return static_cast<int>(std::ceil(appearance.panel.borderWidth));
        case QStyle::PM_MenuHMargin:
            return appearance.horizontalMargin;
        case QStyle::PM_MenuVMargin:
            return appearance.verticalMargin;
        case QStyle::PM_MenuBarHMargin:
            return appearance.bar.horizontalMargin;
        case QStyle::PM_MenuBarItemSpacing:
            return appearance.bar.spacing;
        case QStyle::PM_MenuBarVMargin:
            return appearance.bar.verticalMargin;
        case QStyle::PM_MenuBarPanelWidth:
            return static_cast<int>(std::ceil(appearance.bar.panel.borderWidth));
        case QStyle::PM_MenuTearoffHeight:
        case QStyle::PM_MenuScrollerHeight:
            return appearance.auxiliaryExtent;
        default:
            return {};
    }
}

std::optional<int> styleHint(QStyle::StyleHint hint)
{
    switch(hint) {
        case QStyle::SH_Menu_MouseTracking:
        case QStyle::SH_MenuBar_MouseTracking:
        case QStyle::SH_Menu_SupportsSections:
            return 1;
        case QStyle::SH_Menu_SubMenuPopupDelay:
            return 225;
        default:
            return {};
    }
}

QSize itemSize(const QStyle& style, const QStyleOptionMenuItem& option, const QSize& contentsSize,
               const QWidget* widget, const MenuAppearance& appearance)
{
    const QFontMetrics fm{itemFont(option, appearance.separator)};

    const qsizetype tab = option.text.indexOf(u'\t');
    const auto text     = option.text.left(tab < 0 ? option.text.size() : tab);

    if(option.menuItemType == QStyleOptionMenuItem::Separator) {
        const auto& separator = appearance.separator;
        return {std::max(separator.minimumWidth, fm.horizontalAdvance(text) + (separator.inset * 2)
                                                     + (option.icon.isNull() ? 0 : fm.height() + separator.spacing)),
                text.isEmpty() && option.icon.isNull() ? separator.height
                                                       : fm.height() + (separator.sectionPadding * 2)};
    }
    // QMenu adds the widest shortcut to every row after calling sizeFromContents()
    const int width   = std::max(contentsSize.width(), fm.size(Qt::TextShowMnemonic, text).width())
                      + (itemPadding(appearance.horizontalPadding, appearance) * 2)
                      + checkColumn(style, option, widget, appearance) + iconColumn(option, widget, appearance)
                      + appearance.arrowExtent + appearance.columnSpacing + (tab >= 0 ? appearance.shortcutSpacing : 0);
    const auto* combo = qobject_cast<const QComboBox*>(widget);
    const int iconHeight = option.icon.isNull() ? 0 : combo ? combo->iconSize().height() : appearance.iconSize;
    const int height     = std::max(
        {fm.height() + (itemPadding(appearance.verticalPadding, appearance) * 2),
         (checkColumn(style, option, widget, appearance) ? checkExtent(style, option, widget, appearance) : 0) + 4,
         iconHeight + 6, appearance.minimumHeight});
    return {std::max(appearance.minimumWidth, width), height};
}

QSize menuBarItemSize(const QStyleOptionMenuItem& option, const QSize& contentsSize, const MenuAppearance& appearance)
{
    const int width = option.icon.isNull() ? contentsSize.width() : std::max(contentsSize.width(), appearance.iconSize);
    const int height
        = option.icon.isNull() ? contentsSize.height() : std::max(contentsSize.height(), appearance.iconSize);
    return {width + (itemPadding(appearance.bar.horizontalPadding, appearance.bar) * 2),
            std::max(height + 6,
                     option.fontMetrics.height() + (itemPadding(appearance.bar.verticalPadding, appearance.bar) * 2))};
}

void polish(QWidget* widget)
{
    if(!qobject_cast<QMenu*>(widget) || !widget->isWindow()) {
        return;
    }

    if(!widget->property(BackgroundAttributesProperty).isValid()) {
        const int attributes = (widget->testAttribute(Qt::WA_NoSystemBackground) ? 1 : 0)
                             | (widget->testAttribute(Qt::WA_OpaquePaintEvent) ? 2 : 0)
                             | (widget->autoFillBackground() ? 4 : 0)
                             | (widget->testAttribute(Qt::WA_TranslucentBackground) ? 8 : 0);
        widget->setProperty(BackgroundAttributesProperty, attributes);
    }
    // Menubar popups can inherit an opaque fill. Clear it even if translucency was already enabled.
    widget->setAutoFillBackground(false);
    widget->setAttribute(Qt::WA_TranslucentBackground, true);
    widget->setAttribute(Qt::WA_NoSystemBackground, true);
    widget->setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void unpolish(QWidget* widget)
{
    if(const auto saved = widget->property(BackgroundAttributesProperty); saved.isValid()) {
        const int attributes = saved.toInt();
        widget->setAttribute(Qt::WA_TranslucentBackground, attributes & 8);
        widget->setAutoFillBackground(attributes & 4);
        widget->setAttribute(Qt::WA_NoSystemBackground, attributes & 1);
        widget->setAttribute(Qt::WA_OpaquePaintEvent, attributes & 2);
        widget->setProperty(BackgroundAttributesProperty, {});
    }
}
} // namespace Fooyin::Style::Menu
