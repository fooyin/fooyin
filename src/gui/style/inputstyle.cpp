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

#include "inputstyle.h"

#include "styleutils.h"
#include "theme/inputappearance.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QStyleOptionComboBox>
#include <QStyleOptionFrame>
#include <QStyleOptionSpinBox>
#include <QTextEdit>

#include <array>
#include <cmath>

namespace {
using Fooyin::InputAppearance;

int frameWidth(const InputAppearance& appearance)
{
    return static_cast<int>(std::ceil(
        std::max({appearance.normal.borderWidth, appearance.hovered.borderWidth, appearance.focused.borderWidth,
                  appearance.readOnly.borderWidth, appearance.disabled.borderWidth})));
}

int contentPadding(int padding, const InputAppearance& appearance)
{
    const int focus = appearance.focus.borderWidth > 0
                        ? static_cast<int>(std::ceil(appearance.focus.borderWidth + appearance.focus.spacing))
                        : 0;
    return std::max({padding, frameWidth(appearance), focus});
}

int actionExtent(const InputAppearance& appearance, bool vertical = false)
{
    const auto& action  = appearance.action;
    const double border = std::max({appearance.actionNormal.borderWidth, appearance.actionHovered.borderWidth,
                                    appearance.actionPressed.borderWidth, appearance.actionDisabled.borderWidth});
    const double scale
        = vertical && action.indicatorDesign == InputAppearance::Action::IndicatorDesign::PairedArrows ? 1.5 : 1.0;
    return static_cast<int>(std::ceil((action.glyphSize * scale) + action.glyphWidth + (action.inset * 2) + border));
}

int actionWidth(const InputAppearance& appearance)
{
    return std::max(appearance.action.width, actionExtent(appearance));
}

bool embeddedEditor(const QWidget* widget)
{
    const auto* edit = qobject_cast<const QLineEdit*>(widget);
    return edit && !edit->hasFrame()
        && (qobject_cast<const QComboBox*>(edit->parentWidget())
            || qobject_cast<const QAbstractSpinBox*>(edit->parentWidget()));
}

bool lineEditFrame(const QStyleOptionFrame& option, const QWidget* widget)
{
    if(const auto* edit = qobject_cast<const QLineEdit*>(widget)) {
        return edit->hasFrame();
    }
    return option.lineWidth > 0;
}

QStyleOption inputOption(const QStyleOption& option, const QWidget* widget)
{
    QStyleOption resolved{option};
    if(const auto* spin = qobject_cast<const QAbstractSpinBox*>(widget)) {
        resolved.state.setFlag(QStyle::State_ReadOnly, spin->isReadOnly());
    }
    else if(const auto* combo = qobject_cast<const QComboBox*>(widget); combo && combo->isEditable()) {
        resolved.state.setFlag(QStyle::State_ReadOnly, combo->lineEdit() && combo->lineEdit()->isReadOnly());
    }
    return resolved;
}

const InputAppearance::Surface& surfaceFor(const QStyleOption& option, const InputAppearance& appearance)
{
    if(!(option.state & QStyle::State_Enabled)) {
        return appearance.disabled;
    }
    if(option.state & QStyle::State_ReadOnly) {
        return appearance.readOnly;
    }
    if(option.state & QStyle::State_HasFocus) {
        return appearance.focused;
    }
    return option.state & QStyle::State_MouseOver ? appearance.hovered : appearance.normal;
}

const InputAppearance::ActionState& actionFor(const QStyleOption& option, bool active,
                                              const InputAppearance& appearance)
{
    if(!(option.state & QStyle::State_Enabled)) {
        return appearance.actionDisabled;
    }
    if(active && (option.state & (QStyle::State_Sunken | QStyle::State_On))) {
        return appearance.actionPressed;
    }
    return active && (option.state & QStyle::State_MouseOver) ? appearance.actionHovered : appearance.actionNormal;
}

QRect innerRect(const QRect& rect, bool frame, const InputAppearance& appearance)
{
    const int inset = frame ? frameWidth(appearance) : 0;
    return rect.adjusted(inset, inset, -inset, -inset);
}

QRect editFieldRect(const QRect& rect, bool frame, int actionColumns, const InputAppearance& appearance)
{
    const int horizontal = frame ? contentPadding(appearance.horizontalPadding, appearance) : 0;

    QRect field = innerRect(rect, frame, appearance);
    field.setLeft(rect.left() + horizontal);
    field.setRight(rect.right() - horizontal);

    if(actionColumns > 0) {
        const QRect inner = innerRect(rect, frame, appearance);
        const int width   = std::clamp(inner.width(), 0, actionWidth(appearance) * actionColumns);
        field.setRight(inner.right() - width - (frame ? frameWidth(appearance) : 0));
    }

    return field.isEmpty() ? QRect{} : field;
}

QRect actionRect(const QRect& rect, bool frame, int columns, const InputAppearance& appearance)
{
    QRect action = innerRect(rect, frame, appearance);
    action.setLeft(action.right() - std::clamp(action.width(), 0, actionWidth(appearance) * columns) + 1);
    return action.isEmpty() ? QRect{} : action;
}

void drawSurface(const QStyleOption& option, QPainter& painter, bool frame, bool fill, bool editable,
                 const InputAppearance& appearance, const QWidget* widget)
{
    const auto& surface = surfaceFor(option, appearance);
    const auto group    = Fooyin::Style::colourGroup(option);
    const auto& colour  = editable ? surface.editableBackgroundColour : surface.backgroundColour;
    const auto background
        = fill ? Fooyin::Style::animatedBrush(option, widget, Fooyin::Style::TransitionChannel::Background,
                                              Fooyin::resolveBrush(colour, option.palette, group))
               : Fooyin::resolveBrush(colour, option.palette, group);

    if(!frame) {
        if(fill && surface.backgroundVisible) {
            painter.fillRect(option.rect, background);
        }
        return;
    }

    const double halfBorder = surface.borderWidth / 2.0;
    const QRectF rect       = QRectF{option.rect}.adjusted(halfBorder, halfBorder, -halfBorder, -halfBorder);
    if(rect.isEmpty()) {
        return;
    }

    const double radius = std::min(appearance.radius, std::min(rect.width(), rect.height()) / 2.0);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(
        surface.borderWidth > 0
            ? QPen{Fooyin::Style::animatedBrush(option, widget, Fooyin::Style::TransitionChannel::Border,
                                                Fooyin::resolveBrush(surface.borderColour, option.palette, group)),
                   surface.borderWidth}
            : QPen{Qt::NoPen});
    painter.setBrush(fill && surface.backgroundVisible ? background : Qt::NoBrush);
    painter.drawRoundedRect(rect, radius, radius);

    if((option.state & QStyle::State_Enabled) && (option.state & QStyle::State_HasFocus)
       && appearance.focus.borderWidth > 0) {
        const double inset = appearance.focus.spacing + (appearance.focus.borderWidth / 2.0);
        const QRectF focus = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
        if(!focus.isEmpty()) {
            const double focusRadius = std::clamp(appearance.radius - std::max(0.0, inset - halfBorder), 0.0,
                                                  std::min(focus.width(), focus.height()) / 2.0);
            painter.setPen(QPen{Fooyin::resolveBrush(appearance.focus.borderColour, option.palette, group),
                                appearance.focus.borderWidth});
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(focus, focusRadius, focusRadius);
        }
    }

    painter.restore();
}

void drawAction(const QStyleOption& option, const QRect& rect, QPainter& painter,
                const InputAppearance::ActionState& state, const InputAppearance& appearance)
{
    const double inset = appearance.action.inset + (state.borderWidth / 2.0);
    const QRectF body  = QRectF{rect}.adjusted(inset, inset, -inset, -inset);
    if(body.isEmpty()) {
        return;
    }

    const auto group    = Fooyin::Style::colourGroup(option);
    const double radius = std::min(appearance.action.radius, std::min(body.width(), body.height()) / 2.0);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    if(state.backgroundOpacity > 0) {
        const double opacity = painter.opacity();
        painter.setOpacity(opacity * state.backgroundOpacity / 100.0);
        painter.setBrush(Fooyin::resolveBrush(state.backgroundColour, option.palette, group));
        painter.drawRoundedRect(body, radius, radius);
        painter.setOpacity(opacity);
    }
    if(state.borderWidth > 0) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{Fooyin::resolveBrush(state.borderColour, option.palette, group), state.borderWidth});
        painter.drawRoundedRect(body, radius, radius);
    }

    painter.restore();
}

void drawGlyph(const QStyleOption& option, const QRect& rect, QPainter& painter, bool up, bool plusMinus, bool editable,
               const InputAppearance::ActionState& state, const InputAppearance& appearance, bool paired = false)
{
    const bool filledArrow = appearance.action.filledArrows && !plusMinus;
    const double penWidth  = appearance.action.glyphWidth;
    const double inset     = appearance.action.inset + state.borderWidth;
    const QRectF body      = QRectF{rect}.adjusted(inset, inset, -inset, -inset);
    const double half      = std::min({appearance.action.glyphSize, body.width() - penWidth,
                                       (body.height() - penWidth) / (paired ? 1.5 : 1.0)})
                           / 2.0;
    if(half <= 0 || (!filledArrow && penWidth <= 0)) {
        return;
    }

    const auto& colour   = editable ? state.editableForegroundColour : state.foregroundColour;
    const auto brush     = Fooyin::resolveBrush(colour, option.palette, Fooyin::Style::colourGroup(option));
    const QPointF centre = QRectF{rect}.center();

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(penWidth > 0 ? QPen{brush, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin} : QPen{Qt::NoPen});

    if(plusMinus) {
        painter.drawLine(centre + QPointF{-half, 0}, centre + QPointF{half, 0});
        if(up) {
            painter.drawLine(centre + QPointF{0, -half}, centre + QPointF{0, half});
        }
    }
    else {
        const auto drawArrow = [&](const QPointF& position, bool upwards) {
            const double direction = upwards ? -1.0 : 1.0;
            const std::array points{position + QPointF{-half, -half / 2.0 * direction},
                                    position + QPointF{0, half / 2.0 * direction},
                                    position + QPointF{half, -half / 2.0 * direction}};
            if(filledArrow) {
                painter.setBrush(brush);
                painter.drawPolygon(points.data(), points.size());
            }
            else {
                painter.drawPolyline(points.data(), points.size());
            }
        };

        if(paired) {
            drawArrow(centre - QPointF{0, half}, true);
            drawArrow(centre + QPointF{0, half}, false);
        }
        else {
            drawArrow(centre, up);
        }
    }

    painter.restore();
}

QSize inputSize(const QStyleOption& option, const QSize& contentsSize, bool frame, int extraWidth, int actionRows,
                const InputAppearance& appearance)
{
    QSize size{contentsSize};
    size.rwidth() += extraWidth;

    const int buttonHeight = actionRows ? actionExtent(appearance, true) * actionRows : 0;
    if(frame) {
        size.rwidth() += contentPadding(appearance.horizontalPadding, appearance) * 2;
        // Line edits include their internal text margins in contentsSize; reserve the same height for all inputs
        const int contentHeight = std::max(contentsSize.height(), option.fontMetrics.height() + 4);
        size.setHeight(
            std::max({appearance.minimumHeight, contentHeight + (frameWidth(appearance) * 2),
                      option.fontMetrics.height() + (contentPadding(appearance.verticalPadding, appearance) * 2),
                      buttonHeight + (frameWidth(appearance) * 2)}));
    }
    else {
        size.setHeight(std::max(size.height(), buttonHeight));
    }

    return size;
}
} // namespace

namespace Fooyin::Style::Input {
QSize lineEditSize(const QStyleOptionFrame& option, const QSize& contentsSize, const InputAppearance& appearance,
                   const QWidget* widget)
{
    return inputSize(option, contentsSize, lineEditFrame(option, widget), 0, 0, appearance);
}

QRect lineEditContentsRect(const QStyleOptionFrame& option, const InputAppearance& appearance, const QWidget* widget)
{
    return editFieldRect(option.rect, lineEditFrame(option, widget), 0, appearance);
}

void drawLineEdit(const QStyleOptionFrame& option, QPainter& painter, bool fill, const InputAppearance& appearance,
                  const QWidget* widget)
{
    if(embeddedEditor(widget)) {
        return;
    }
    drawSurface(option, painter, lineEditFrame(option, widget), fill, false, appearance, widget);
}

QSize comboBoxSize(const QStyleOptionComboBox& option, const QSize& contentsSize, const InputAppearance& appearance)
{
    return inputSize(option, contentsSize, option.frame, actionWidth(appearance) + 4, 1, appearance);
}

QRect comboBoxSubControlRect(const QStyleOptionComboBox& option, QStyle::SubControl subControl,
                             const InputAppearance& appearance)
{
    QRect rect;
    switch(subControl) {
        case QStyle::SC_ComboBoxFrame:
        case QStyle::SC_ComboBoxListBoxPopup:
            return option.rect;
        case QStyle::SC_ComboBoxEditField:
            rect = editFieldRect(option.rect, option.frame, 1, appearance);
            break;
        case QStyle::SC_ComboBoxArrow:
            rect = actionRect(option.rect, option.frame, 1, appearance);
            break;
        default:
            return {};
    }
    return QStyle::visualRect(option.direction, option.rect, rect);
}

void drawComboBox(const QStyle& style, const QStyleOptionComboBox& option, QPainter& painter, const QWidget* widget,
                  const InputAppearance& appearance)
{
    const auto surface = inputOption(option, widget);
    if(option.subControls & QStyle::SC_ComboBoxFrame) {
        drawSurface(surface, painter, option.frame, true, option.editable, appearance, widget);
    }
    if(option.subControls & QStyle::SC_ComboBoxArrow) {
        const QRect arrow = style.subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxArrow, widget);
        const auto& state = actionFor(
            option, (option.activeSubControls & QStyle::SC_ComboBoxArrow) || (option.state & QStyle::State_On),
            appearance);
        drawAction(option, arrow, painter, state, appearance);
        drawGlyph(option, arrow, painter, false, false, option.editable, state, appearance,
                  appearance.action.indicatorDesign == InputAppearance::Action::IndicatorDesign::PairedArrows);
    }
}

void drawComboBoxLabel(const QStyle& style, const QStyleOptionComboBox& option, QPainter& painter,
                       const QWidget* widget)
{
    auto rect = style.subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, widget);

    auto palette{option.palette};
    palette.setCurrentColorGroup(colourGroup(option));

    painter.save();
    painter.setClipRect(rect, Qt::IntersectClip);

    if(!option.currentIcon.isNull()) {
        const auto mode   = option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled;
        const auto pixmap = option.currentIcon.pixmap(option.iconSize, painter.device()->devicePixelRatioF(), mode);
        const int width   = option.iconSize.width() + 4;

        const auto iconRect
            = QStyle::alignedRect(option.direction, Qt::AlignLeading | Qt::AlignVCenter, {width, rect.height()}, rect);
        style.drawItemPixmap(&painter, iconRect, Qt::AlignCenter, pixmap);

        if(option.direction == Qt::RightToLeft) {
            rect.adjust(0, 0, -width, 0);
        }
        else {
            rect.adjust(width, 0, 0, 0);
        }
    }
    if(!option.editable) {
        style.drawItemText(&painter, rect.adjusted(1, 0, -1, 0),
                           QStyle::visualAlignment(option.direction, option.textAlignment), palette,
                           option.state & QStyle::State_Enabled, option.currentText, QPalette::ButtonText);
    }

    painter.restore();
}

QSize spinBoxSize(const QStyleOptionSpinBox& option, const QSize& contentsSize, const InputAppearance& appearance)
{
    const bool buttons    = option.buttonSymbols != QAbstractSpinBox::NoButtons;
    const bool sideBySide = appearance.action.layout == InputAppearance::Action::Layout::SideBySide;
    const int columns     = sideBySide ? 2 : 1;
    const int rows        = sideBySide ? 1 : 2;
    return inputSize(option, contentsSize, option.frame, buttons ? actionWidth(appearance) * columns : 0,
                     buttons ? rows : 0, appearance);
}

QRect spinBoxSubControlRect(const QStyleOptionSpinBox& option, QStyle::SubControl subControl,
                            const InputAppearance& appearance)
{
    const bool buttons    = option.buttonSymbols != QAbstractSpinBox::NoButtons;
    const bool sideBySide = appearance.action.layout == InputAppearance::Action::Layout::SideBySide;
    const int columns     = sideBySide ? 2 : 1;

    QRect rect;
    switch(subControl) {
        case QStyle::SC_SpinBoxFrame:
            return option.rect;
        case QStyle::SC_SpinBoxEditField:
            rect = editFieldRect(option.rect, option.frame, buttons ? columns : 0, appearance);
            break;
        case QStyle::SC_SpinBoxUp:
        case QStyle::SC_SpinBoxDown:
            if(!buttons) {
                return {};
            }
            rect = actionRect(option.rect, option.frame, columns, appearance);
            if(sideBySide) {
                const int middle = rect.left() + ((rect.width() + 1) / 2);
                if(subControl == QStyle::SC_SpinBoxUp) {
                    rect.setLeft(middle);
                }
                else {
                    rect.setRight(middle - 1);
                }
            }
            else if(subControl == QStyle::SC_SpinBoxUp) {
                rect.setHeight((rect.height() + 1) / 2);
            }
            else {
                rect.setTop(rect.top() + ((rect.height() + 1) / 2));
            }
            break;
        default:
            return {};
    }

    return QStyle::visualRect(option.direction, option.rect, rect);
}

void drawSpinBox(const QStyle& style, const QStyleOptionSpinBox& option, QPainter& painter, const QWidget* widget,
                 const InputAppearance& appearance)
{
    const auto surface = inputOption(option, widget);
    if(option.subControls & QStyle::SC_SpinBoxFrame) {
        drawSurface(surface, painter, option.frame, true, false, appearance, widget);
    }

    if(option.buttonSymbols == QAbstractSpinBox::NoButtons) {
        return;
    }

    for(const auto control : {QStyle::SC_SpinBoxUp, QStyle::SC_SpinBoxDown}) {
        if(!(option.subControls & control)) {
            continue;
        }

        const bool up    = control == QStyle::SC_SpinBoxUp;
        const auto step  = up ? QAbstractSpinBox::StepUpEnabled : QAbstractSpinBox::StepDownEnabled;
        const QRect rect = style.subControlRect(QStyle::CC_SpinBox, &option, control, widget);

        auto action{surface};
        if(!(option.stepEnabled & step) || (surface.state & QStyle::State_ReadOnly)) {
            action.state &= ~QStyle::State_Enabled;
        }

        const auto& state = actionFor(action, option.activeSubControls & control, appearance);
        drawAction(action, rect, painter, state, appearance);
        drawGlyph(action, rect, painter, up, option.buttonSymbols == QAbstractSpinBox::PlusMinus, false, state,
                  appearance);
    }
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const QWidget* widget, const InputAppearance& lineEdit,
                               const InputAppearance& comboBox, const InputAppearance& spinBox)
{
    switch(metric) {
        case QStyle::PM_ComboBoxFrameWidth:
            return frameWidth(comboBox);
        case QStyle::PM_SpinBoxFrameWidth:
            return frameWidth(spinBox);
        case QStyle::PM_DefaultFrameWidth:
            if(qobject_cast<const QLineEdit*>(widget) || qobject_cast<const QTextEdit*>(widget)
               || qobject_cast<const QPlainTextEdit*>(widget)) {
                return frameWidth(lineEdit);
            }
            return {};
        default:
            return {};
    }
}
} // namespace Fooyin::Style::Input
