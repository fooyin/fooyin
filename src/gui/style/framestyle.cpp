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

#include "framestyle.h"

#include "styleutils.h"
#include "theme/groupboxappearance.h"

#include <QApplication>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QRubberBand>
#include <QStyleOptionButton>
#include <QStyleOptionFrame>
#include <QStyleOptionGroupBox>
#include <QStyleOptionRubberBand>
#include <QVariant>

#include <cmath>

using namespace Qt::StringLiterals;

constexpr auto SeparatorMinimumProperty       = "_fooyin_separator_minimum";
constexpr auto SeparatorStyledMinimumProperty = "_fooyin_separator_styled_minimum";

namespace {
int contentPadding(int padding, const Fooyin::GroupBoxAppearance& appearance)
{
    return std::max(padding, static_cast<int>(std::ceil(appearance.borderWidth)));
}

int titleInset(const Fooyin::GroupBoxAppearance& appearance)
{
    return std::max(appearance.title.inset, static_cast<int>(std::ceil(appearance.borderWidth)));
}

QMargins frameMargins(const Fooyin::GroupBoxAppearance& appearance, bool hasTitle)
{
    const int overhang = hasTitle ? std::max(0, -appearance.title.horizontalMargin) : 0;
    return {appearance.title.alignment == "trailing"_L1 ? 0 : overhang, 0,
            appearance.title.alignment == "leading"_L1 ? 0 : overhang, 0};
}

QColor groupColour(const QStyleOption& option, const Fooyin::ColourSource& colour,
                   const Fooyin::ColourSource& tintColour, double opacity)
{
    const auto group   = Fooyin::Style::colourGroup(option);
    const auto base    = Fooyin::resolveBrush(colour, option.palette, group).color();
    const auto tint    = Fooyin::resolveBrush(tintColour, option.palette, group).color();
    const float amount = static_cast<float>(opacity / 100.0) * tint.alphaF();
    return QColor::fromRgbF(std::lerp(base.redF(), tint.redF(), amount),
                            std::lerp(base.greenF(), tint.greenF(), amount),
                            std::lerp(base.blueF(), tint.blueF(), amount), base.alphaF());
}

QFont titleFont(const QStyleOptionGroupBox& option, const QWidget* widget, const Fooyin::GroupBoxAppearance& appearance)
{
    if(!widget) {
        widget = qobject_cast<const QWidget*>(option.styleObject);
    }
    return appearance.title.font.resolve(widget ? widget->font() : QApplication::font("QGroupBox"));
}

QSize titleSize(const QStyle& style, const QStyleOptionGroupBox& option, const QWidget* widget,
                const Fooyin::GroupBoxAppearance& appearance)
{
    const auto metrics = appearance.title.font.resolveMask() == 0
                           ? option.fontMetrics
                           : QFontMetrics{titleFont(option, widget, appearance), widget};
    QSize size         = option.text.isEmpty() ? QSize{0, 0} : metrics.size(Qt::TextShowMnemonic, option.text);

    if(option.subControls & QStyle::SC_GroupBoxCheckBox) {
        size.rwidth() += style.pixelMetric(QStyle::PM_IndicatorWidth, &option, widget);

        if(!option.text.isEmpty()) {
            size.rwidth() += style.pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &option, widget);
        }
        size.setHeight(std::max(size.height(), style.pixelMetric(QStyle::PM_IndicatorHeight, &option, widget)));
    }

    return size;
}
} // namespace

namespace Fooyin::Style::Frame {
void draw(const QStyleOptionFrame& option, QPainter& painter, const FrameAppearance& appearance)
{
    const double inset = appearance.borderWidth / 2.0;
    const QRectF rect  = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(rect.isEmpty() || option.lineWidth <= 0 || appearance.borderWidth <= 0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(painter.opacity() * appearance.borderOpacity / 100.0);
    painter.setPen(
        QPen{resolveBrush(appearance.borderColour, option.palette, colourGroup(option)), appearance.borderWidth});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect, appearance.radius, appearance.radius);
    painter.restore();
}

void drawFocus(const QStyleOption& option, QPainter& painter)
{
    if(!(option.state & QStyle::State_Enabled) || !(option.state & QStyle::State_KeyboardFocusChange)) {
        return;
    }

    const auto rect = QRectF{option.rect}.adjusted(0.75, 0.75, -0.75, -0.75);
    if(rect.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const auto colour = option.palette.color(colourGroup(option), QPalette::Highlight);
    painter.setPen(QPen{colour, 1.5});
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect, 4.0, 4.0);
    painter.restore();
}

void drawRubberBand(const QStyleOptionRubberBand& option, QPainter& painter)
{
    if(option.rect.isEmpty()) {
        return;
    }

    const auto accent = option.palette.color(colourGroup(option), QPalette::Highlight);

    painter.save();
    painter.setClipRect(option.rect, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing);

    if(option.shape == QRubberBand::Line) {
        auto fill{accent};
        fill.setAlphaF(fill.alphaF() * 0.8F);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        const double radius = std::min({2.0, option.rect.width() / 2.0, option.rect.height() / 2.0});
        painter.drawRoundedRect(QRectF{option.rect}, radius, radius);
    }
    else {
        auto fill{accent};
        fill.setAlphaF(fill.alphaF() * 0.2F);
        painter.setPen(QPen{accent, 1.0});
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF{option.rect}.adjusted(0.5, 0.5, -0.5, -0.5), 3.0, 3.0);
    }

    painter.restore();
}

void drawSeparator(const QStyleOptionFrame& option, QPainter& painter, const FrameAppearance::Separator& appearance)
{
    if(option.rect.isEmpty() || option.lineWidth <= 0 || appearance.width <= 0) {
        return;
    }

    const QRectF rect{option.rect};
    const bool horizontal = option.frameShape == QFrame::HLine;
    const double inset    = appearance.inset + (appearance.width / 2.0);
    if((horizontal ? rect.width() : rect.height()) <= 2 * inset) {
        return;
    }

    painter.save();
    painter.setOpacity(painter.opacity() * appearance.opacity / 100.0);
    painter.setPen(QPen{resolveBrush(appearance.colour, option.palette, colourGroup(option)), appearance.width});

    if(horizontal) {
        painter.drawLine(QPointF{rect.left() + inset, rect.center().y()},
                         QPointF{rect.right() - inset, rect.center().y()});
    }
    else {
        painter.drawLine(QPointF{rect.center().x(), rect.top() + inset},
                         QPointF{rect.center().x(), rect.bottom() - inset});
    }

    painter.restore();
}

void polish(QWidget* widget, const FrameAppearance::Separator& appearance)
{
    const int width = static_cast<int>(std::ceil(appearance.width));
    if(width <= 3) {
        return;
    }

    const auto* frame = qobject_cast<QFrame*>(widget);
    if(!frame || frame->lineWidth() <= 0
       || (frame->frameShape() != QFrame::HLine && frame->frameShape() != QFrame::VLine)
       || widget->property(SeparatorMinimumProperty).isValid()) {
        return;
    }

    const QSize previous = widget->minimumSize();

    QSize minimum{previous};
    if(frame->frameShape() == QFrame::HLine) {
        minimum.setHeight(std::max(previous.height(), std::min(width, widget->maximumHeight())));
    }
    else {
        minimum.setWidth(std::max(previous.width(), std::min(width, widget->maximumWidth())));
    }
    if(minimum != previous) {
        widget->setProperty(SeparatorMinimumProperty, previous);
        widget->setProperty(SeparatorStyledMinimumProperty, minimum);
        widget->setMinimumSize(minimum);
    }
}

void unpolish(QWidget* widget)
{
    if(const auto previous = widget->property(SeparatorMinimumProperty); previous.isValid()) {
        const auto original{previous.toSize()};
        const auto styled = widget->property(SeparatorStyledMinimumProperty).toSize();

        auto minimum = widget->minimumSize();
        if(minimum.width() == styled.width()) {
            minimum.setWidth(original.width());
        }
        if(minimum.height() == styled.height()) {
            minimum.setHeight(original.height());
        }
        widget->setMinimumSize(minimum);

        widget->setProperty(SeparatorMinimumProperty, {});
        widget->setProperty(SeparatorStyledMinimumProperty, {});
    }
}

void drawGroupFrame(const QStyleOptionFrame& option, QPainter& painter, const GroupBoxAppearance& appearance)
{
    const double border = option.lineWidth > 0 ? appearance.borderWidth : 0;
    const double inset  = border / 2.0;
    const QRectF rect   = QRectF{option.rect}.adjusted(inset, inset, -inset, -inset);
    if(rect.isEmpty()) {
        return;
    }

    const auto& colours = option.state & QStyle::State_Enabled ? appearance.normal : appearance.disabled;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    if(option.features & QStyleOptionFrame::Flat) {
        path.moveTo(rect.topLeft());
        path.lineTo(rect.topRight());
    }
    else {
        const double radius = std::min({appearance.radius, rect.width() / 2.0, rect.height() / 2.0});
        path.addRoundedRect(rect, radius, radius);
        if(colours.backgroundVisible) {
            painter.fillPath(path, groupColour(option, colours.backgroundColour, colours.backgroundTintColour,
                                               colours.backgroundTintOpacity));
        }
    }

    drawRelief(option, painter, path, border, appearance.reliefOpacity);
    if(border > 0) {
        painter.strokePath(
            path, QPen{groupColour(option, colours.borderColour, colours.borderTintColour, colours.borderTintOpacity),
                       border});
    }

    painter.restore();
}

void drawGroupBox(const QStyle& style, const QStyleOptionGroupBox& option, QPainter& painter, const QWidget* widget,
                  const GroupBoxAppearance& appearance)
{
    const QRect label = style.subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxLabel, widget);
    const QRect check = style.subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxCheckBox, widget);

    if(option.subControls & QStyle::SC_GroupBoxFrame) {
        QStyleOptionFrame frame;
        frame.QStyleOption::operator=(option);
        frame.rect      = style.subControlRect(QStyle::CC_GroupBox, &option, QStyle::SC_GroupBoxFrame, widget);
        frame.features  = option.features;
        frame.lineWidth = option.lineWidth;

        painter.save();

        const QRect title = label.united(check);
        QRegion region{option.rect};
        if((option.features & QStyleOptionFrame::Flat) && appearance.title.placement != "above"_L1
           && !title.isEmpty()) {
            region -= title.adjusted(-appearance.title.gap, 0, appearance.title.gap, 0);
        }

        painter.setClipRegion(region, Qt::IntersectClip);
        style.drawPrimitive(QStyle::PE_FrameGroupBox, &frame, &painter, widget);
        painter.restore();
    }
    if((option.subControls & QStyle::SC_GroupBoxLabel) && !label.isEmpty()) {
        painter.save();
        if(appearance.title.font.resolveMask() != 0) {
            painter.setFont(titleFont(option, widget, appearance));
        }
        auto palette{option.palette};
        palette.setCurrentColorGroup(colourGroup(option));
        const auto& colours   = option.state & QStyle::State_Enabled ? appearance.normal : appearance.disabled;
        const auto* role      = std::get_if<QPalette::ColorRole>(&colours.titleColour);
        const auto titleBrush = option.textColor.isValid() && role && *role == QPalette::WindowText
                                  ? QBrush{option.textColor}
                                  : resolveBrush(colours.titleColour, option.palette, colourGroup(option));
        palette.setBrush(palette.currentColorGroup(), QPalette::WindowText, titleBrush);

        int flags = QStyle::visualAlignment(option.direction, Qt::AlignLeft) | Qt::AlignVCenter | Qt::TextShowMnemonic;
        if(!style.styleHint(QStyle::SH_UnderlineShortcut, &option, widget)) {
            flags |= Qt::TextHideMnemonic;
        }

        style.drawItemText(&painter, label, flags, palette, option.state & QStyle::State_Enabled, option.text,
                           QPalette::WindowText);
        painter.restore();
    }
    if((option.subControls & QStyle::SC_GroupBoxCheckBox) && !check.isEmpty()) {
        QStyleOptionButton indicator;
        indicator.QStyleOption::operator=(option);
        indicator.rect = check;
        style.drawPrimitive(QStyle::PE_IndicatorCheckBox, &indicator, &painter, widget);
    }
}

QRect groupBoxSubControlRect(const QStyle& style, const QStyleOptionGroupBox& option, QStyle::SubControl subControl,
                             const QWidget* widget, const GroupBoxAppearance& appearance)
{
    const QSize size   = titleSize(style, option, widget, appearance);
    const bool above   = appearance.title.placement == "above"_L1 && size.height() > 0;
    const int frameTop = above ? titleInset(appearance) + size.height() + appearance.title.gap : 0;
    const QRect frame  = QStyle::visualRect(option.direction, option.rect,
                                            option.rect.marginsRemoved(frameMargins(appearance, size.height() > 0)));

    if(subControl == QStyle::SC_GroupBoxFrame) {
        if(above) {
            return frame.adjusted(0, frameTop, 0, 0);
        }
        return option.features & QStyleOptionFrame::Flat
                 ? frame.adjusted(0, titleInset(appearance) + (size.height() / 2), 0, 0)
                 : frame;
    }
    if(subControl == QStyle::SC_GroupBoxContents) {
        const int horizontal = contentPadding(appearance.horizontalPadding, appearance);
        const int vertical   = contentPadding(appearance.verticalPadding, appearance);
        if(above) {
            return frame.adjusted(horizontal, frameTop + vertical, -horizontal, -vertical);
        }
        return frame.adjusted(
            horizontal,
            size.height() + (size.height() > 0 ? (titleInset(appearance) * 2) + appearance.title.gap : vertical),
            -horizontal, -vertical);
    }
    if(subControl != QStyle::SC_GroupBoxCheckBox && subControl != QStyle::SC_GroupBoxLabel) {
        return {};
    }

    const int margin = std::max(0, appearance.title.horizontalMargin);
    QRect area       = option.rect.adjusted(margin, 0, -margin, 0);
    area.translate(0, titleInset(appearance));
    area.setHeight(size.height());
    if(area.isEmpty()) {
        return {};
    }

    const bool checkable = option.subControls & QStyle::SC_GroupBoxCheckBox;
    const int checkWidth = checkable ? style.pixelMetric(QStyle::PM_IndicatorWidth, &option, widget) : 0;
    const auto alignment = appearance.title.alignment == "leading"_L1  ? Qt::AlignLeft
                         : appearance.title.alignment == "trailing"_L1 ? Qt::AlignRight
                                                                       : Qt::AlignHCenter;
    QRect title          = QStyle::alignedRect(Qt::LeftToRight, alignment | Qt::AlignVCenter,
                                               QSize{std::min(size.width(), area.width()), size.height()}, area);

    if(subControl == QStyle::SC_GroupBoxCheckBox) {
        if(!checkable) {
            return {};
        }
        const int height = style.pixelMetric(QStyle::PM_IndicatorHeight, &option, widget);
        title            = QRect{title.left(), title.top() + ((title.height() - height) / 2), checkWidth, height};
    }
    else {
        if(option.text.isEmpty()) {
            return {};
        }
        if(checkable) {
            title.setLeft(title.left() + checkWidth
                          + style.pixelMetric(QStyle::PM_CheckBoxLabelSpacing, &option, widget));
        }
    }

    return title.isEmpty() ? QRect{} : QStyle::visualRect(option.direction, option.rect, title);
}

QSize groupBoxSize(const QStyle& style, const QStyleOptionGroupBox& option, const QSize& contentsSize,
                   const QWidget* widget, const GroupBoxAppearance& appearance)
{
    const QSize title      = titleSize(style, option, widget, appearance);
    const int titlePadding = title.height() == 0                      ? 0
                           : appearance.title.placement == "above"_L1 ? titleInset(appearance) + appearance.title.gap
                                                                      : titleInset(appearance) * 2;
    const auto margins     = frameMargins(appearance, title.height() > 0);
    return contentsSize.expandedTo(title)
         + QSize{contentPadding(std::max(appearance.title.horizontalMargin, appearance.horizontalPadding), appearance)
                         * 2
                     + margins.left() + margins.right(),
                 (contentPadding(appearance.verticalPadding, appearance) * 2) + titlePadding};
}
} // namespace Fooyin::Style::Frame
