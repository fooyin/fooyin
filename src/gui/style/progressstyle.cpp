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

#include "progressstyle.h"

#include "styleutils.h"

#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QStyleOptionProgressBar>
#include <QTimerEvent>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace {
bool isBusy(const QStyleOptionProgressBar& option)
{
    return option.minimum == 0 && option.maximum == 0;
}

bool canAnimate(const QProgressBar* bar)
{
    return bar && bar->isVisible() && bar->isEnabled() && !bar->window()->isMinimized() && bar->minimum() == 0
        && bar->maximum() == 0;
}

bool textVisible(const QStyleOptionProgressBar& option, const Fooyin::ProgressBarAppearance& appearance)
{
    return option.textVisible && appearance.text.visible;
}

bool textInside(const QStyleOptionProgressBar& option, const Fooyin::ProgressBarAppearance& appearance)
{
    return textVisible(option, appearance)
        && appearance.text.position == Fooyin::ProgressBarAppearance::Text::Position::Inside;
}

QSizeF outsideTextSize(const QStyleOptionProgressBar& option, const Fooyin::ProgressBarAppearance& appearance)
{
    int width = option.fontMetrics.horizontalAdvance(option.text);
    if(const auto* bar = qobject_cast<const QProgressBar*>(option.styleObject)) {
        auto locale{bar->locale()};
        locale.setNumberOptions(locale.numberOptions() | QLocale::OmitGroupSeparator);

        auto format{bar->format()};
        format.replace("%m"_L1, locale.toString(static_cast<qint64>(option.maximum) - option.minimum));
        format.replace("%p"_L1, locale.toString(100));

        for(const int value : {option.minimum, option.maximum}) {
            auto text{format};
            text.replace("%v"_L1, locale.toString(value));
            width = std::max(width, option.fontMetrics.horizontalAdvance(text));
        }
    }

    QSizeF size{static_cast<double>(width + (appearance.text.padding * 2)),
                static_cast<double>(option.fontMetrics.height())};
    if(!(option.state & QStyle::State_Horizontal)) {
        size.transpose();
    }
    return size;
}

struct TextLayout
{
    QRectF bar;
    QRectF text;
};

TextLayout textLayout(const QStyleOptionProgressBar& option, const Fooyin::ProgressBarAppearance& appearance)
{
    TextLayout layout{.bar = option.rect, .text = option.rect};
    if(!textVisible(option, appearance) || textInside(option, appearance)) {
        return layout;
    }

    const double width   = std::min(outsideTextSize(option, appearance).width(), layout.bar.width());
    const double spacing = std::min<double>(appearance.text.spacing, layout.bar.width() - width);
    if(appearance.text.position == Fooyin::ProgressBarAppearance::Text::Position::Left) {
        layout.text.setWidth(width);
        layout.bar.setLeft(layout.bar.left() + width + spacing);
    }
    else {
        layout.text.setLeft(layout.text.right() - width);
        layout.bar.setRight(layout.bar.right() - width - spacing);
    }

    return layout;
}

QRectF grooveRect(const QStyleOptionProgressBar& option, const Fooyin::ProgressBarAppearance& appearance,
                  const QRectF& area)
{
    const double border
        = textInside(option, appearance) ? appearance.track.borderWidth : appearance.track.compactBorderWidth;
    const double inset = std::max(appearance.track.inset, border / 2.0);
    QRectF rect        = area.adjusted(inset, inset, -inset, -inset);

    if(!textInside(option, appearance)) {
        if(option.state & QStyle::State_Horizontal) {
            const double height = std::min(static_cast<double>(appearance.track.compactThickness), rect.height());
            rect.setTop(rect.center().y() - (height / 2.0));
            rect.setHeight(height);
        }
        else {
            const double width = std::min(static_cast<double>(appearance.track.compactThickness), rect.width());
            rect.setLeft(rect.center().x() - (width / 2.0));
            rect.setWidth(width);
        }
    }

    return rect;
}

QPainterPath roundedPath(const QRectF& rect, double cornerRadius)
{
    QPainterPath path;
    if(!rect.isEmpty()) {
        const double radius = std::min({cornerRadius, rect.width() / 2.0, rect.height() / 2.0});
        path.addRoundedRect(rect, radius, radius);
    }
    return path;
}

bool reverseFill(const QStyleOptionProgressBar& option)
{
    const bool horizontal = option.state & QStyle::State_Horizontal;
    return option.invertedAppearance ^ (horizontal ? option.direction == Qt::RightToLeft : true);
}

QRectF fillRect(const QStyleOptionProgressBar& option, const QRectF& groove, double phase, int segmentSize)
{
    const bool horizontal = option.state & QStyle::State_Horizontal;
    const double length   = horizontal ? groove.width() : groove.height();

    double start{0.0};
    double extent{0.0};
    if(isBusy(option)) {
        extent                = length * segmentSize / 100.0;
        const double position = (1.0 - std::cos(phase * 2.0 * std::numbers::pi)) / 2.0;
        start                 = position * (length - extent);
    }
    else if(option.progress >= option.minimum && option.maximum >= option.minimum) {
        const auto range    = static_cast<qint64>(option.maximum) - option.minimum;
        const auto progress = static_cast<qint64>(option.progress) - option.minimum;
        const double fraction
            = range == 0 ? 1.0 : std::clamp(static_cast<double>(progress) / static_cast<double>(range), 0.0, 1.0);
        extent = length * fraction;
    }

    if(reverseFill(option)) {
        start = length - start - extent;
    }

    return horizontal ? QRectF{groove.left() + start, groove.top(), extent, groove.height()}
                      : QRectF{groove.left(), groove.top() + start, groove.width(), extent};
}

QPainterPath fillPath(const QStyleOptionProgressBar& option, const QRectF& area, double phase,
                      const Fooyin::ProgressBarAppearance& appearance)
{
    if(area.isEmpty()) {
        return {};
    }

    const auto filled   = fillRect(option, area, phase, appearance.busy.segmentSize);
    const double radius = textInside(option, appearance) ? appearance.fill.radius : appearance.fill.compactRadius;
    if(!appearance.fill.segmented || filled.isEmpty()) {
        return roundedPath(filled, radius);
    }

    const bool horizontal = option.state & QStyle::State_Horizontal;
    const bool reverse    = reverseFill(option);
    const double length   = horizontal ? area.width() : area.height();
    const int pitch       = appearance.fill.segmentLength + appearance.fill.segmentSpacing;

    QPainterPath segments;
    for(int offset{0}; offset < length; offset += pitch) {
        const double extent  = std::min<double>(appearance.fill.segmentLength, length - offset);
        const double start   = reverse ? length - offset - extent : offset;
        const QRectF segment = horizontal ? QRectF{area.left() + start, area.top(), extent, area.height()}
                                          : QRectF{area.left(), area.top() + start, area.width(), extent};
        if(segment.intersects(filled)) {
            segments.addPath(roundedPath(segment, radius));
        }
    }

    QPainterPath clip;
    clip.addRect(filled);
    return segments.intersected(clip);
}

void drawLabel(const QStyleOptionProgressBar& option, QPainter& painter, const QPainterPath& fill,
               const Fooyin::ProgressBarAppearance& appearance, const QRectF& area)
{
    if(!textVisible(option, appearance) || option.text.isEmpty()) {
        return;
    }

    const auto group    = Fooyin::Style::colourGroup(option);
    const auto& colours = option.state & QStyle::State_Enabled ? appearance.normal : appearance.disabled;

    const auto text = [&]() {
        painter.save();

        QRectF rect{area};
        if(!(option.state & QStyle::State_Horizontal)) {
            painter.translate(rect.center());
            painter.rotate(option.bottomToTop ? -90.0 : 90.0);
            rect = QRectF{-rect.height() / 2.0, -rect.width() / 2.0, rect.height(), rect.width()};
        }
        rect.adjust(appearance.text.padding, 0.0, -appearance.text.padding, 0.0);

        Qt::Alignment alignment = option.textAlignment & Qt::AlignHorizontal_Mask;
        using Alignment         = Fooyin::ProgressBarAppearance::Text::Alignment;
        switch(appearance.text.alignment) {
            case Alignment::Widget:
                break;
            case Alignment::Leading:
                alignment = Qt::AlignLeading;
                break;
            case Alignment::Centre:
                alignment = Qt::AlignHCenter;
                break;
            case Alignment::Trailing:
                alignment = Qt::AlignTrailing;
                break;
        }

        const auto flags = QStyle::visualAlignment(option.direction, alignment) | Qt::AlignVCenter | Qt::TextSingleLine;
        painter.drawText(rect, flags, option.text);
        painter.restore();
    };

    painter.setPen(QPen{Fooyin::resolveBrush(colours.textColour, option.palette, group), 1.0});
    text();

    if(textInside(option, appearance) && !fill.isEmpty()) {
        painter.save();
        painter.setClipPath(fill, Qt::IntersectClip);
        painter.setPen(QPen{Fooyin::resolveBrush(colours.filledTextColour, option.palette, group), 1.0});
        text();
        painter.restore();
    }
}
} // namespace

namespace Fooyin::Style::Progress {
void draw(QStyle::ControlElement element, const QStyleOptionProgressBar& option, QPainter& painter, double phase,
          const ProgressBarAppearance& appearance)
{
    const auto layout   = textLayout(option, appearance);
    const auto groove   = grooveRect(option, appearance, layout.bar);
    const bool labelled = textInside(option, appearance);

    const auto& colours      = option.state & QStyle::State_Enabled ? appearance.normal : appearance.disabled;
    const double borderWidth = labelled ? appearance.track.borderWidth : appearance.track.compactBorderWidth;

    const auto track = roundedPath(groove, labelled ? appearance.track.radius : appearance.track.compactRadius);
    const auto fillArea
        = groove.adjusted(appearance.fill.inset, appearance.fill.inset, -appearance.fill.inset, -appearance.fill.inset);
    const auto fill = fillPath(option, fillArea, phase, appearance).intersected(track);

    const auto group       = colourGroup(option);
    const bool horizontal  = option.state & QStyle::State_Horizontal;
    const auto orientation = horizontal ? Qt::Horizontal : Qt::Vertical;
    const bool reverse     = reverseFill(option);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    if(element == QStyle::CE_ProgressBar || element == QStyle::CE_ProgressBarGroove) {
        painter.setPen(element == QStyle::CE_ProgressBarGroove && borderWidth > 0
                           ? QPen{resolveBrush(colours.borderColour, option.palette, group), borderWidth}
                           : QPen{Qt::NoPen});
        painter.setBrush(resolveBrush(labelled ? colours.trackColour : colours.compactTrackColour, option.palette,
                                      group, groove, orientation, reverse));
        painter.drawPath(track);
    }
    if(element == QStyle::CE_ProgressBar || element == QStyle::CE_ProgressBarContents) {
        // Anchor the fill gradient to the full range, so colours stay in place as progress changes
        painter.fillPath(fill, resolveBrush(colours.fillColour, option.palette, group, fillArea, orientation, reverse));
    }
    if(element == QStyle::CE_ProgressBar && borderWidth > 0) {
        painter.setPen(QPen{resolveBrush(colours.borderColour, option.palette, group), borderWidth});
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(track);
    }
    if(element == QStyle::CE_ProgressBar || element == QStyle::CE_ProgressBarLabel) {
        drawLabel(option, painter, fill, appearance, layout.text);
    }

    painter.restore();
}

QRect subElementRect(QStyle::SubElement element, const QStyleOptionProgressBar& option,
                     const ProgressBarAppearance& appearance)
{
    const auto layout = textLayout(option, appearance);
    if(element == QStyle::SE_ProgressBarLabel) {
        return textVisible(option, appearance) ? layout.text.toAlignedRect() : QRect{};
    }
    auto rect = grooveRect(option, appearance, layout.bar);
    if(element == QStyle::SE_ProgressBarContents) {
        rect.adjust(appearance.fill.inset, appearance.fill.inset, -appearance.fill.inset, -appearance.fill.inset);
    }
    return rect.isEmpty() ? QRect{} : rect.toAlignedRect();
}

QSize sizeFromContents(const QStyleOptionProgressBar& option, const QSize& contentsSize,
                       const ProgressBarAppearance& appearance)
{
    const bool labelled         = textInside(option, appearance);
    const double border         = labelled ? appearance.track.borderWidth : appearance.track.compactBorderWidth;
    const double inset          = std::max(appearance.track.inset, border / 2.0);
    const double innerThickness = labelled ? option.fontMetrics.height() + 4.0 : appearance.track.compactThickness;

    const int thickness  = std::max({appearance.minimumThickness, option.fontMetrics.height() + 14,
                                     static_cast<int>(std::ceil(innerThickness + (inset * 2.0) + border))});
    const int textLength = labelled ? option.fontMetrics.horizontalAdvance(option.text) + (appearance.text.padding * 2)
                                          + static_cast<int>(std::ceil(inset * 2.0))
                                    : 0;

    QSize size{contentsSize};
    if(option.state & QStyle::State_Horizontal) {
        size.setHeight(std::max(size.height(), thickness));
        size.setWidth(std::max(size.width(), textLength));
    }
    else {
        size.setWidth(std::max(size.width(), thickness));
        size.setHeight(std::max(size.height(), textLength));
    }
    if(textVisible(option, appearance) && !labelled) {
        const auto text = outsideTextSize(option, appearance);
        size.rwidth() += static_cast<int>(std::ceil(text.width())) + appearance.text.spacing;
        size.setHeight(std::max(size.height(), static_cast<int>(std::ceil(text.height()))));
    }
    return size;
}

Animation::Animation(QStyle* style)
    : QObject{style}
{ }

double Animation::phase(const QStyleOptionProgressBar& option, const QWidget* widget,
                        const ProgressBarAppearance& appearance)
{
    auto* bar = const_cast<QProgressBar*>(qobject_cast<const QProgressBar*>(widget));
    if(!bar) {
        bar = qobject_cast<QProgressBar*>(option.styleObject);
    }

    if(!appearance.busy.animated || !isBusy(option) || !(option.state & QStyle::State_Enabled) || !canAnimate(bar)
       || !bar->style()->styleHint(QStyle::SH_Widget_Animate, &option, bar)) {
        remove(bar);
        return 0.25;
    }

    if(std::ranges::none_of(m_bars, [bar](const auto& tracked) { return tracked == bar; })) {
        m_bars.emplace_back(bar);
    }

    if(!m_timer.isActive()) {
        m_elapsed.start();
        m_timer.start(33, this);
    }
    return static_cast<double>(m_elapsed.elapsed() % appearance.busy.duration) / appearance.busy.duration;
}

void Animation::remove(const QWidget* widget)
{
    std::erase_if(m_bars, [widget](const auto& bar) { return !bar || bar == widget; });
    if(m_bars.empty()) {
        m_timer.stop();
    }
}

void Animation::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != m_timer.timerId()) {
        QObject::timerEvent(event);
        return;
    }

    std::erase_if(m_bars, [](const auto& bar) { return !canAnimate(bar); });
    if(m_bars.empty()) {
        m_timer.stop();
        return;
    }
    for(const auto& bar : m_bars) {
        bar->update();
    }
}
} // namespace Fooyin::Style::Progress
