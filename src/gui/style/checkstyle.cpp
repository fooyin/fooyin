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

#include "checkstyle.h"

#include "styleutils.h"
#include "theme/checkappearance.h"

#include <QPainter>
#include <QStyleOptionButton>

#include <array>

namespace Fooyin::Style::Check {
void drawIndicator(const QStyleOption& option, QPainter& painter, bool radio, const CheckAppearance& appearance,
                   const QWidget* widget)
{
    const auto group = colourGroup(option);
    const int extent = std::min({appearance.indicatorExtent, option.rect.width(), option.rect.height()});

    const QRectF bounds = QStyle::alignedRect(option.direction, Qt::AlignCenter, QSize{extent, extent}, option.rect);
    if(bounds.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(bounds.topLeft());
    const double scale = static_cast<double>(extent) / appearance.indicatorExtent;
    painter.scale(scale, scale);

    const double borderWidth = std::max({appearance.normal.borderWidth, appearance.hovered.borderWidth,
                                         appearance.pressed.borderWidth, appearance.disabled.borderWidth});
    const double focusSpace
        = appearance.focus.borderWidth > 0 ? appearance.focus.borderWidth + appearance.focus.spacing : 0;
    const double inset = std::max(appearance.inset, (borderWidth / 2.0) + focusSpace);
    const QRectF indicator{0.0, 0.0, static_cast<double>(appearance.indicatorExtent),
                           static_cast<double>(appearance.indicatorExtent)};
    const QRectF body   = indicator.adjusted(inset, inset, -inset, -inset);
    const double radius = std::min(appearance.radius, body.width() / 2.0);

    const auto shape = [&](const QRectF& rect, double cornerRadius) {
        if(radio) {
            painter.drawEllipse(rect);
        }
        else {
            painter.drawRoundedRect(rect, cornerRadius, cornerRadius);
        }
    };

    painter.setPen(Qt::NoPen);

    const bool enabled = option.state & QStyle::State_Enabled;
    const bool mixed   = !radio && (option.state & QStyle::State_NoChange);
    const bool checked = mixed || (option.state & QStyle::State_On);
    const bool hovered = enabled && (option.state & QStyle::State_MouseOver);
    const bool pressed = enabled && (option.state & QStyle::State_Sunken);

    const auto& state = stateAppearance(appearance, enabled, hovered, pressed);
    const auto mark   = resolveBrush(state.markColour, option.palette, group);

    const auto halo = opacityBrush(resolveBrush(state.haloColour, option.palette, group), state.haloOpacity / 100.0);
    painter.setBrush(animatedBrush(option, widget, TransitionChannel::Halo, halo));
    shape(indicator.adjusted(0.75, 0.75, -0.75, -0.75), radius + inset - 0.75);

    painter.setBrush(animatedBrush(
        option, widget, TransitionChannel::Background,
        resolveBrush(checked ? state.checkedBackgroundColour : state.backgroundColour, option.palette, group)));
    shape(body, radius);

    const auto tint
        = opacityBrush(resolveBrush(checked ? state.checkedTintColour : state.tintColour, option.palette, group),
                       state.tintOpacity / 100.0);
    painter.setBrush(animatedBrush(option, widget, TransitionChannel::Tint, tint));
    shape(body, radius);

    if(state.borderWidth > 0) {
        const auto& colour = (option.state & QStyle::State_Selected) ? state.markColour
                           : checked                                 ? state.checkedBorderColour
                                                                     : state.borderColour;
        painter.setPen(
            QPen{animatedBrush(option, widget, TransitionChannel::Border, resolveBrush(colour, option.palette, group)),
                 state.borderWidth});
        painter.setBrush(Qt::NoBrush);
        shape(body, radius);
    }

    if(checked && appearance.markSize > 0) {
        const auto centre = body.center();
        if(radio) {
            const double size = std::min(appearance.markSize, std::max(0.0, body.width() - state.borderWidth));
            painter.setPen(Qt::NoPen);
            painter.setBrush(mark);
            painter.drawEllipse(centre, size / 2.0, size / 2.0);
        }
        else if(appearance.markWidth > 0) {
            const double width = std::min(appearance.markWidth, body.width() / 2.0);
            const double size  = std::min(appearance.markSize, std::max(0.0, body.width() - state.borderWidth - width));
            painter.setPen(QPen{mark, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
            if(mixed) {
                painter.drawLine(centre + QPointF{-size * 0.4375, 0.0}, centre + QPointF{size * 0.4375, 0.0});
            }
            else {
                const std::array points{centre + QPointF{-size * 0.475, 0.0},
                                        centre + QPointF{-size * 0.125, size * 0.35},
                                        centre + QPointF{size * 0.5, -size * 0.375}};
                painter.drawPolyline(points.data(), static_cast<int>(points.size()));
            }
        }
    }
    if(enabled && (option.state & QStyle::State_HasFocus) && appearance.focus.borderWidth > 0) {
        const double expansion
            = (state.borderWidth / 2.0) + appearance.focus.spacing + (appearance.focus.borderWidth / 2.0);
        painter.setPen(
            QPen{resolveBrush(appearance.focus.borderColour, option.palette, group), appearance.focus.borderWidth});
        painter.setBrush(Qt::NoBrush);
        shape(body.adjusted(-expansion, -expansion, expansion, expansion), radius + expansion);
    }

    painter.restore();
}

void draw(const QStyle& style, const QStyleOptionButton& option, QPainter& painter, const QWidget* widget, bool radio)
{
    auto indicator{option};
    indicator.rect
        = style.subElementRect(radio ? QStyle::SE_RadioButtonIndicator : QStyle::SE_CheckBoxIndicator, &option, widget);
    style.drawPrimitive(radio ? QStyle::PE_IndicatorRadioButton : QStyle::PE_IndicatorCheckBox, &indicator, &painter,
                        widget);

    auto label{option};
    label.rect
        = style.subElementRect(radio ? QStyle::SE_RadioButtonContents : QStyle::SE_CheckBoxContents, &option, widget);
    label.palette.setCurrentColorGroup(colourGroup(option));
    style.drawControl(radio ? QStyle::CE_RadioButtonLabel : QStyle::CE_CheckBoxLabel, &label, &painter, widget);
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const CheckAppearance& checkBox,
                               const CheckAppearance& radioButton)
{
    switch(metric) {
        case QStyle::PM_IndicatorWidth:
        case QStyle::PM_IndicatorHeight:
            return checkBox.indicatorExtent;
        case QStyle::PM_ExclusiveIndicatorWidth:
        case QStyle::PM_ExclusiveIndicatorHeight:
            return radioButton.indicatorExtent;
        case QStyle::PM_CheckBoxLabelSpacing:
            return checkBox.labelSpacing;
        case QStyle::PM_RadioButtonLabelSpacing:
            return radioButton.labelSpacing;
        default:
            return {};
    }
}
} // namespace Fooyin::Style::Check
