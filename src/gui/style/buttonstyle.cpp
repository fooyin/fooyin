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

#include "buttonstyle.h"

#include "styleutils.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QStyleOptionToolButton>

#include <cmath>

constexpr auto MenuIndicatorWidth = 18;

namespace {
const Fooyin::PushButtonAppearance::Surface& buttonSurface(const QStyleOptionButton& option,
                                                           const Fooyin::PushButtonAppearance& appearance)
{
    const bool enabled = option.state & QStyle::State_Enabled;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool pressed = option.state & QStyle::State_Sunken;

    if(enabled && !pressed && !hovered) {
        if(option.state & QStyle::State_On) {
            return appearance.checked;
        }
        if(option.features & QStyleOptionButton::DefaultButton) {
            return appearance.defaultButton;
        }
    }

    return Fooyin::Style::stateAppearance(appearance, enabled, hovered, pressed);
}

double surfaceBorderWidth(const Fooyin::ButtonSurface& surface)
{
    return surface.borderVisible ? surface.borderWidth : 0;
}

const Fooyin::ButtonSurface& iconSurface(const Fooyin::ButtonSurface& surface, const Fooyin::ButtonSurface& checked,
                                         bool isChecked)
{
    return !surface.tintIcon && isChecked && checked.tintIcon ? checked : surface;
}

double maximumBorderWidth(const Fooyin::PushButtonAppearance& appearance)
{
    return std::max({surfaceBorderWidth(appearance.normal), surfaceBorderWidth(appearance.hovered),
                     surfaceBorderWidth(appearance.pressed), surfaceBorderWidth(appearance.disabled),
                     surfaceBorderWidth(appearance.checked), surfaceBorderWidth(appearance.defaultButton)});
}

double maximumBorderWidth(const Fooyin::ToolButtonAppearance& appearance)
{
    return std::max({surfaceBorderWidth(appearance.normal), surfaceBorderWidth(appearance.hovered),
                     surfaceBorderWidth(appearance.pressed), surfaceBorderWidth(appearance.disabled),
                     surfaceBorderWidth(appearance.checked), surfaceBorderWidth(appearance.menu.normal),
                     surfaceBorderWidth(appearance.menu.hovered), surfaceBorderWidth(appearance.menu.pressed),
                     surfaceBorderWidth(appearance.menu.disabled), surfaceBorderWidth(appearance.menu.checked)});
}

template <typename Appearance>
double panelInset(const Appearance& appearance)
{
    // Reserve focus and border space in every state so hovering or focusing never moves the label
    const double focusSpace
        = appearance.focus.borderWidth > 0 ? appearance.focus.borderWidth + appearance.focus.spacing : 0;
    return (maximumBorderWidth(appearance) / 2.0) + focusSpace;
}

template <typename Appearance>
QSize buttonPadding(const Appearance& appearance)
{
    const int frame = static_cast<int>(std::ceil(panelInset(appearance) + (maximumBorderWidth(appearance) / 2.0)));
    return {std::max(frame, appearance.horizontalPadding), std::max(frame, appearance.verticalPadding)};
}

template <typename Appearance>
const Fooyin::ButtonSurface& buttonSurface(const QStyleOptionToolButton& option, const Appearance& appearance)
{
    const bool enabled = option.state & QStyle::State_Enabled;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool pressed = option.state & QStyle::State_Sunken;
    if(enabled && !hovered && !pressed && (option.state & QStyle::State_On)) {
        return appearance.checked;
    }
    return Fooyin::Style::stateAppearance(appearance, enabled, hovered, pressed);
}

void drawPanel(const QStyleOption& option, QPainter& painter, double inset, double cornerRadius,
               const Fooyin::ButtonSurface& surface, bool background, bool border, const QWidget* widget,
               bool menu = false)
{
    const auto group  = Fooyin::Style::colourGroup(option);
    const QRectF rect = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(rect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const double radius = std::min(cornerRadius, std::min(rect.width(), rect.height()) / 2.0);
    using namespace Fooyin::Style;
    const auto backgroundChannel = menu ? TransitionChannel::MenuBackground : TransitionChannel::Background;
    const auto tintChannel       = menu ? TransitionChannel::MenuTint : TransitionChannel::Tint;
    const auto borderChannel     = menu ? TransitionChannel::MenuBorder : TransitionChannel::Border;
    const auto backgroundBrush   = background && surface.backgroundVisible
                                     ? Fooyin::resolveBrush(surface.backgroundColour, option.palette, group, rect,
                                                            Qt::Horizontal, option.direction == Qt::RightToLeft)
                                     : QBrush{Qt::transparent};
    painter.setBrush(animatedBrush(option, widget, backgroundChannel, backgroundBrush));
    painter.drawRoundedRect(rect, radius, radius);

    const auto tint = opacityBrush(Fooyin::resolveBrush(surface.tintColour, option.palette, group),
                                   surface.backgroundVisible ? surface.tintOpacity / 100.0 : 0.0);
    painter.setBrush(animatedBrush(option, widget, tintChannel, tint));
    painter.drawRoundedRect(rect, radius, radius);
    const auto borderBrush = animatedBrush(option, widget, borderChannel,
                                           Fooyin::resolveBrush(surface.borderColour, option.palette, group));
    if(border && surface.borderVisible && surface.borderWidth > 0) {
        painter.setPen(QPen{borderBrush, surface.borderWidth});
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect, radius, radius);
    }

    painter.restore();
}

void drawFocus(const QStyleOption& option, QPainter& painter, double inset, double cornerRadius, double borderWidth,
               const Fooyin::ButtonFocus& focus)
{
    if(!(option.state & QStyle::State_Enabled) || !(option.state & QStyle::State_HasFocus) || focus.borderWidth <= 0) {
        return;
    }

    const QRectF rect = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(rect.isEmpty()) {
        return;
    }

    const double radius      = std::min(cornerRadius, std::min(rect.width(), rect.height()) / 2.0);
    const double expansion   = (borderWidth / 2.0) + focus.spacing + (focus.borderWidth / 2.0);
    const double focusRadius = radius > 0 ? radius + expansion : 0;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen{Fooyin::resolveBrush(focus.borderColour, option.palette, Fooyin::Style::colourGroup(option)),
                        focus.borderWidth});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect.adjusted(-expansion, -expansion, expansion, expansion), focusRadius, focusRadius);
    painter.restore();
}

int menuIndicatorWidth(const Fooyin::ToolButtonAppearance& appearance)
{
    return std::max(appearance.menu.indicatorWidth, (buttonPadding(appearance).width() * 2) + 8);
}

QStyleOptionToolButton partOption(const QStyleOptionToolButton& option, QStyle::SubControl part)
{
    auto resolved{option};
    if((option.features & QStyleOptionToolButton::MenuButtonPopup) && !(option.activeSubControls & part)) {
        resolved.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken);
    }
    return resolved;
}
} // namespace

namespace Fooyin::Style::Button {
void drawPushButton(const QStyle& style, const QStyleOptionButton& option, QPainter& painter, const QWidget* widget,
                    const PushButtonAppearance& appearance)
{
    drawPushButtonPanel(style, option, painter, widget, appearance);

    auto label{option};
    label.rect = style.subElementRect(QStyle::SE_PushButtonContents, &option, widget);
    label.palette.setCurrentColorGroup(colourGroup(option));
    style.drawControl(QStyle::CE_PushButtonLabel, &label, &painter, widget);
}

void drawPushButtonPanel(const QStyle& style, const QStyleOptionButton& option, QPainter& painter,
                         const QWidget* widget, const PushButtonAppearance& appearance)
{
    const bool flat = option.features & QStyleOptionButton::Flat;
    const bool border
        = !flat || (option.state & QStyle::State_On) || (option.features & QStyleOptionButton::DefaultButton);
    const auto& surface = buttonSurface(option, appearance);
    const double inset  = panelInset(appearance);
    drawPanel(option, painter, inset, appearance.radius, surface, !flat, border, widget);
    drawFocus(option, painter, inset, appearance.radius, border ? surfaceBorderWidth(surface) : 0, appearance.focus);

    if(option.features & QStyleOptionButton::HasMenu) {
        const QRect contents = style.subElementRect(QStyle::SE_PushButtonContents, &option, widget);
        const int width
            = std::min(contents.width(), style.pixelMetric(QStyle::PM_MenuButtonIndicator, &option, widget));
        const QRect arrow{contents.right() - width + 1, contents.y(), width, contents.height()};
        drawChevron(option, QStyle::visualRect(option.direction, contents, arrow), painter, false,
                    resolveBrush(surface.foregroundColour, option.palette, colourGroup(option)));
    }
}

void drawPushButtonLabel(const QCommonStyle& style, const QStyleOptionButton& option, QPainter& painter,
                         const QWidget* widget, const PushButtonAppearance& appearance)
{
    const auto& surface = buttonSurface(option, appearance);
    const auto group    = colourGroup(option);

    auto label{option};
    label.palette.setCurrentColorGroup(group);
    label.palette.setBrush(group, QPalette::ButtonText,
                           animatedBrush(option, widget, TransitionChannel::Foreground,
                                         resolveBrush(surface.foregroundColour, option.palette, group)));

    const auto& icon = iconSurface(surface, appearance.checked, option.state & QStyle::State_On);
    if(icon.tintIcon && !label.icon.isNull()) {
        const auto mode = !(option.state & QStyle::State_Enabled) ? QIcon::Disabled
                        : option.state & QStyle::State_HasFocus   ? QIcon::Active
                                                                  : QIcon::Normal;
        label.icon      = tintedIcon(
            label.icon, label.iconSize, painter.device()->devicePixelRatioF(), mode,
            option.state & QStyle::State_On ? QIcon::On : QIcon::Off,
            animatedBrush(option, widget, TransitionChannel::Icon, resolveBrush(icon.iconColour, option.palette, group))
                .color());
    }

    style.QCommonStyle::drawControl(QStyle::CE_PushButtonLabel, &label, &painter, widget);
}

void drawToolButton(const QStyle& style, const QStyleOptionToolButton& option, QPainter& painter, const QWidget* widget,
                    const ToolButtonAppearance& appearance)
{
    // Dock title buttons paint their panel separately and request only the label here
    if(option.subControls == QStyle::SC_None) {
        style.drawControl(QStyle::CE_ToolButtonLabel, &option, &painter, widget);
        return;
    }

    const bool split      = option.features & QStyleOptionToolButton::MenuButtonPopup;
    const bool drawButton = option.subControls & QStyle::SC_ToolButton;
    const bool drawMenu   = split && (option.subControls & QStyle::SC_ToolButtonMenu);
    if(!drawButton && !drawMenu) {
        return;
    }

    painter.save();

    const QRect menu   = style.subControlRect(QStyle::CC_ToolButton, &option, QStyle::SC_ToolButtonMenu, widget);
    const QRect button = style.subControlRect(QStyle::CC_ToolButton, &option, QStyle::SC_ToolButton, widget);

    if(split && !(drawButton && drawMenu)) {
        painter.setClipRect(drawButton ? button : menu, Qt::IntersectClip);
    }

    const bool flat       = option.state & QStyle::State_AutoRaise;
    const bool background = !flat || appearance.flatBackground;
    const bool border     = !flat || appearance.flatBorder || (option.state & QStyle::State_On);
    const double inset    = panelInset(appearance);
    const auto mainOption = partOption(option, QStyle::SC_ToolButton);
    const auto menuOption = partOption(option, QStyle::SC_ToolButtonMenu);

    if(split) {
        // Paint each region once so translucent surfaces have the same opacity on both sides
        if(drawButton) {
            painter.save();
            painter.setClipRect(button, Qt::IntersectClip);
            drawPanel(mainOption, painter, inset, appearance.radius, buttonSurface(mainOption, appearance), background,
                      border, widget);
            painter.restore();
        }
        if(drawMenu) {
            painter.save();
            painter.setClipRect(menu, Qt::IntersectClip);
            drawPanel(menuOption, painter, inset, appearance.radius, buttonSurface(menuOption, appearance.menu),
                      background, border, widget, true);
            painter.restore();
        }
        if(appearance.menu.separatorVisible && appearance.menu.separatorWidth > 0
           && (border || (option.state & QStyle::State_HasFocus)
               || ((option.state & QStyle::State_Enabled) && (option.state & QStyle::State_MouseOver)))) {
            const double top    = option.rect.top() + appearance.menu.separatorInset;
            const double bottom = option.rect.bottom() - appearance.menu.separatorInset;
            if(bottom > top) {
                painter.save();
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen{resolveBrush(appearance.menu.separatorColour, option.palette, colourGroup(option)),
                                    appearance.menu.separatorWidth});
                const double x = option.direction == Qt::RightToLeft ? menu.right() : menu.left();
                painter.drawLine(QPointF{x, top}, QPointF{x, bottom});
                painter.restore();
            }
        }
    }
    else {
        drawPanel(option, painter, inset, appearance.radius, buttonSurface(option, appearance), background, border,
                  widget);
    }
    const double focusBorder = border
                                 ? std::max(surfaceBorderWidth(buttonSurface(mainOption, appearance)),
                                            split ? surfaceBorderWidth(buttonSurface(menuOption, appearance.menu)) : 0)
                                 : 0;
    drawFocus(option, painter, inset, appearance.radius, focusBorder, appearance.focus);

    if(drawButton) {
        auto label{mainOption};
        const auto padding = buttonPadding(appearance);
        label.rect         = button.adjusted(padding.width(), padding.height(), -padding.width(), -padding.height());
        if(!label.rect.isEmpty()) {
            style.drawControl(QStyle::CE_ToolButtonLabel, &label, &painter, widget);
        }
    }
    const auto& surface = split ? buttonSurface(menuOption, appearance.menu) : buttonSurface(option, appearance);
    if(drawMenu) {
        painter.save();
        painter.setClipRect(menu, Qt::IntersectClip);
        drawChevron(menuOption, menu, painter, false,
                    resolveBrush(surface.foregroundColour, option.palette, colourGroup(menuOption)));
        painter.restore();
    }
    else if(!split && (option.features & QStyleOptionToolButton::HasMenu)) {
        const QRect arrow{option.rect.right() - 11, option.rect.bottom() - 10, 7, 4};
        drawChevron(option, QStyle::visualRect(option.direction, option.rect, arrow), painter, false,
                    resolveBrush(surface.foregroundColour, option.palette, colourGroup(option)));
    }

    painter.restore();
}

void drawToolButtonLabel(const QCommonStyle& style, const QStyleOptionToolButton& option, QPainter& painter,
                         const QWidget* widget, const ToolButtonAppearance& appearance)
{
    const auto& surface = buttonSurface(option, appearance);
    const auto group    = colourGroup(option);

    auto label{option};
    label.palette.setCurrentColorGroup(group);
    label.palette.setBrush(group, QPalette::ButtonText,
                           animatedBrush(option, widget, TransitionChannel::Foreground,
                                         resolveBrush(surface.foregroundColour, option.palette, group)));

    const auto& icon = iconSurface(surface, appearance.checked, option.state & QStyle::State_On);
    if(icon.tintIcon && !label.icon.isNull()) {
        const auto mode = !(option.state & QStyle::State_Enabled) ? QIcon::Disabled
                        : (option.state & QStyle::State_MouseOver) && (option.state & QStyle::State_AutoRaise)
                            ? QIcon::Active
                            : QIcon::Normal;
        label.icon      = tintedIcon(
            label.icon, label.iconSize, painter.device()->devicePixelRatioF(), mode,
            option.state & QStyle::State_On ? QIcon::On : QIcon::Off,
            animatedBrush(option, widget, TransitionChannel::Icon, resolveBrush(icon.iconColour, option.palette, group))
                .color());
    }

    style.QCommonStyle::drawControl(QStyle::CE_ToolButtonLabel, &label, &painter, widget);
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const QStyleOption* option,
                               const ToolButtonAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_ButtonDefaultIndicator:
        case QStyle::PM_ButtonShiftHorizontal:
        case QStyle::PM_ButtonShiftVertical:
            return 0;
        case QStyle::PM_MenuButtonIndicator:
            return qstyleoption_cast<const QStyleOptionToolButton*>(option) ? menuIndicatorWidth(appearance)
                                                                            : MenuIndicatorWidth;
        default:
            return {};
    }
}

QSize pushButtonSize(const QStyleOptionButton& option, const QSize& contentsSize,
                     const PushButtonAppearance& appearance)
{
    QSize size = contentsSize + buttonPadding(appearance) * 2;
    if(!option.text.isEmpty()) {
        size.setWidth(std::max(appearance.minimumWidth, size.width()));
    }
    size.setHeight(std::max(appearance.minimumHeight, size.height()));
    return size;
}

QSize toolButtonSize(const QSize& contentsSize, const ToolButtonAppearance& appearance)
{
    return (contentsSize + buttonPadding(appearance) * 2)
        .expandedTo(QSize{appearance.minimumWidth, appearance.minimumHeight});
}

QRect pushButtonContentsRect(const QStyleOptionButton& option, const PushButtonAppearance& appearance)
{
    const auto padding = buttonPadding(appearance);
    return option.rect.adjusted(padding.width(), padding.height(), -padding.width(), -padding.height());
}

QRect toolButtonSubControlRect(const QStyle& style, const QStyleOptionToolButton& option, QStyle::SubControl subControl,
                               const QWidget* widget)
{
    if(subControl != QStyle::SC_ToolButton && subControl != QStyle::SC_ToolButtonMenu) {
        return {};
    }
    if(!(option.features & QStyleOptionToolButton::MenuButtonPopup)) {
        return subControl == QStyle::SC_ToolButton ? option.rect : QRect{};
    }

    const int width = std::clamp(style.pixelMetric(QStyle::PM_MenuButtonIndicator, &option, widget), 0,
                                 std::max(0, option.rect.width()));

    QRect rect{option.rect};
    if(subControl == QStyle::SC_ToolButtonMenu) {
        rect.setLeft(rect.right() - width + 1);
    }
    else {
        rect.setWidth(rect.width() - width);
    }
    return QStyle::visualRect(option.direction, option.rect, rect);
}
} // namespace Fooyin::Style::Button
