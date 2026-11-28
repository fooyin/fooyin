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

#include "scrollbarstyle.h"

#include "styleutils.h"
#include "theme/scrollbarappearance.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionSlider>
#include <QTimerEvent>

#include <ranges>

namespace {
QAbstractScrollArea* scrollArea(const QWidget* widget)
{
    if(!qobject_cast<const QScrollBar*>(widget)) {
        return nullptr;
    }
    for(auto* parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if(auto* area = qobject_cast<QAbstractScrollArea*>(parent);
           area && (area->horizontalScrollBar() == widget || area->verticalScrollBar() == widget)) {
            return area;
        }
    }
    return nullptr;
}

bool canAutoHide(const QScrollBar* bar, const QAbstractScrollArea* area)
{
    const auto policy
        = bar->orientation() == Qt::Horizontal ? area->horizontalScrollBarPolicy() : area->verticalScrollBarPolicy();
    if(policy != Qt::ScrollBarAsNeeded || !bar->parentWidget() || bar->parentWidget() == area) {
        return false;
    }
    return std::ranges::all_of(bar->parentWidget()->children(),
                               [bar](auto* child) { return child == bar || !qobject_cast<QWidget*>(child); });
}
} // namespace

namespace Fooyin::Style::ScrollBar {
bool isTransient(const ScrollBarAppearance& appearance, const QWidget* widget)
{
    if(!appearance.autoHide.enabled) {
        return false;
    }
    if(!widget) {
        return true;
    }
    const auto* area = scrollArea(widget);
    return area && canAutoHide(static_cast<const QScrollBar*>(widget), area);
}

void draw(const QStyle& style, const ScrollBarAppearance& appearance, const QStyleOptionSlider& option,
          QPainter& painter, const QWidget* widget)
{
    const bool enabled = (option.state & QStyle::State_Enabled) && option.maximum > option.minimum;
    const bool active  = option.activeSubControls & QStyle::SC_ScrollBarSlider;
    const bool hovered = enabled && active && (option.state & QStyle::State_MouseOver);
    const bool pressed = enabled && active && (option.state & QStyle::State_Sunken);
    const auto group   = enabled ? colourGroup(option) : QPalette::Disabled;
    const auto& track  = stateAppearance(appearance.track, enabled, option.state & QStyle::State_MouseOver,
                                         option.state & QStyle::State_Sunken);

    if(track.visible && (option.subControls & QStyle::SC_ScrollBarGroove)) {
        painter.fillRect(option.rect, resolveBrush(track.backgroundColour, option.palette, group));
    }
    if(!(option.subControls & QStyle::SC_ScrollBarSlider)) {
        return;
    }

    const auto& state = stateAppearance(appearance, enabled, hovered, pressed);

    const QRectF handle = style.subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, widget);
    QRectF thumb{handle};
    if(option.orientation == Qt::Horizontal) {
        thumb.adjust(state.endInset, state.sideInset, -state.endInset, -state.sideInset);
    }
    else {
        thumb.adjust(state.sideInset, state.endInset, -state.sideInset, -state.endInset);
    }
    if(thumb.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const double opacity = painter.opacity();
    painter.setOpacity(opacity * state.opacity / 100.0);
    if(state.borderWidth > 0.0) {
        painter.setPen(QPen{resolveBrush(state.borderColour, option.palette, group), state.borderWidth});
    }
    else {
        painter.setPen(Qt::NoPen);
    }
    painter.setBrush(resolveBrush(state.backgroundColour, option.palette, group));
    const double radius = std::min(thumb.width(), thumb.height()) * state.cornerRadius / 200.0;
    painter.drawRoundedRect(thumb, radius, radius);

    if(enabled && (option.state & QStyle::State_HasFocus) && appearance.focus.borderWidth > 0.0) {
        painter.setOpacity(opacity);
        painter.setPen(
            QPen{resolveBrush(appearance.focus.borderColour, option.palette, group), appearance.focus.borderWidth});
        painter.setBrush(Qt::NoBrush);
        const double inset = appearance.focus.borderWidth / 2.0;
        painter.drawRoundedRect(handle.adjusted(inset, inset, -inset, -inset), appearance.focus.radius,
                                appearance.focus.radius);
    }
    painter.restore();
}

void drawCorner(const ScrollBarAppearance& appearance, const QStyleOption& option, QPainter& painter)
{
    const auto& track = option.state & QStyle::State_Enabled ? appearance.track.normal : appearance.track.disabled;
    if(track.visible) {
        painter.fillRect(option.rect, resolveBrush(track.backgroundColour, option.palette, colourGroup(option)));
    }
}

std::optional<int> pixelMetric(QStyle::PixelMetric metric, const ScrollBarAppearance& appearance)
{
    switch(metric) {
        case QStyle::PM_ScrollBarExtent:
            return appearance.width;
        case QStyle::PM_ScrollBarSliderMin:
            return appearance.minimumThumbLength;
        default:
            return {};
    }
}

std::optional<int> styleHint(QStyle::StyleHint hint, const ScrollBarAppearance& appearance, const QWidget* widget)
{
    if(hint == QStyle::SH_ScrollBar_Transient) {
        return isTransient(appearance, widget);
    }
    return {};
}

QRect subControlRect(const QStyle& style, const QStyleOptionSlider& option, QStyle::SubControl subControl,
                     const QWidget* widget)
{
    const QRect rect{option.rect};
    if(rect.isEmpty()) {
        return {};
    }
    if(subControl == QStyle::SC_ScrollBarGroove) {
        return rect;
    }
    if(subControl != QStyle::SC_ScrollBarSlider && subControl != QStyle::SC_ScrollBarSubPage
       && subControl != QStyle::SC_ScrollBarAddPage) {
        return {};
    }

    const bool horizontal = option.orientation == Qt::Horizontal;
    const int length      = horizontal ? rect.width() : rect.height();
    const qint64 range    = static_cast<qint64>(option.maximum) - option.minimum;

    int thumbLength{length};
    if(range > 0) {
        const qint64 pageStep        = std::max(0, option.pageStep);
        const int proportionalLength = static_cast<int>(pageStep * length / (range + pageStep));
        const int minimumLength = std::min(length, style.pixelMetric(QStyle::PM_ScrollBarSliderMin, &option, widget));
        thumbLength             = std::clamp(proportionalLength, minimumLength, length);
    }

    const int position = QStyle::sliderPositionFromValue(option.minimum, option.maximum, option.sliderPosition,
                                                         length - thumbLength, option.upsideDown);

    int start{position};
    int size{thumbLength};
    if(subControl == QStyle::SC_ScrollBarSubPage || subControl == QStyle::SC_ScrollBarAddPage) {
        const bool beforeThumb = (subControl == QStyle::SC_ScrollBarSubPage) != option.upsideDown;
        start                  = beforeThumb ? 0 : position + thumbLength;
        size                   = beforeThumb ? position : length - start;
    }

    const QRect subRect = horizontal ? QRect{rect.x() + start, rect.y(), size, rect.height()}
                                     : QRect{rect.x(), rect.y() + start, rect.width(), size};
    return QStyle::visualRect(option.direction, rect, subRect);
}

Animation::Animation(QStyle* style, const ScrollBarAppearance& appearance)
    : QObject{style}
    , m_settings{appearance.autoHide}
{
    m_elapsed.start();
}

Animation::~Animation()
{
    while(!m_entries.empty()) {
        remove(m_entries.back().bar);
    }
}

void Animation::polish(QWidget* widget)
{
    if(!m_settings.enabled) {
        return;
    }

    auto* bar  = qobject_cast<QScrollBar*>(widget);
    auto* area = scrollArea(widget);
    if(!bar || !area || !canAutoHide(bar, area)) {
        return;
    }

    auto* viewport      = area->viewport();
    const auto existing = std::ranges::find_if(m_entries, [bar](const auto& entry) { return entry.bar == bar; });
    if(existing != m_entries.end()) {
        if(existing->viewport == viewport && existing->container == bar->parentWidget()) {
            return;
        }
        remove(bar);
    }

    m_entries.push_back({.bar                  = bar,
                         .area                 = area,
                         .viewport             = viewport,
                         .container            = bar->parentWidget(),
                         .lastActivity         = m_elapsed.elapsed(),
                         .opacity              = 1.0,
                         .hovered              = bar->underMouse(),
                         .atEdge               = false,
                         .hoverAttribute       = bar->testAttribute(Qt::WA_Hover),
                         .opaqueAttribute      = bar->testAttribute(Qt::WA_OpaquePaintEvent),
                         .transparentAttribute = bar->testAttribute(Qt::WA_TransparentForMouseEvents),
                         .autoFillBackground   = bar->autoFillBackground(),
                         .containerTransparent = bar->parentWidget()->testAttribute(Qt::WA_TransparentForMouseEvents)});

    bar->setAttribute(Qt::WA_Hover, true);
    bar->setAttribute(Qt::WA_OpaquePaintEvent, false);
    bar->setAutoFillBackground(false);

    if(m_entries.size() == 1) {
        qApp->installEventFilter(this);
    }

    QObject::connect(bar, &QScrollBar::valueChanged, this, [this, area] { reveal(area); });
    QObject::connect(bar, &QScrollBar::rangeChanged, this, [this, area] { reveal(area); });
    QObject::connect(bar, &QScrollBar::sliderPressed, this, [this, area] { reveal(area); });
    QObject::connect(bar, &QScrollBar::sliderReleased, this, [this, area] { reveal(area); });
    QObject::connect(bar, &QObject::destroyed, this, [this, bar] { remove(bar, false); });

    reveal(area);
}

void Animation::setOpacity(Entry& entry, double opacity)
{
    if(!entry.bar) {
        return;
    }

    entry.bar->setAttribute(Qt::WA_TransparentForMouseEvents, entry.transparentAttribute || opacity <= 0.0);
    if(entry.container) {
        entry.container->setAttribute(Qt::WA_TransparentForMouseEvents, entry.containerTransparent || opacity <= 0.0);
    }
    if(entry.opacity != opacity) {
        entry.opacity = opacity;
        entry.bar->update();
    }
}

void Animation::reveal(const QWidget* area)
{
    for(auto& entry : m_entries) {
        if(entry.area == area) {
            entry.lastActivity = m_elapsed.elapsed();
            setOpacity(entry, 1.0);
        }
    }
    if(!m_timer.isActive()) {
        m_timer.start(33, this);
    }
}

double Animation::opacity(const QStyleOptionSlider& option, const QWidget* widget)
{
    if(!m_settings.enabled) {
        return 1.0;
    }

    auto* bar = const_cast<QScrollBar*>(qobject_cast<const QScrollBar*>(widget));
    if(!bar) {
        bar = qobject_cast<QScrollBar*>(option.styleObject);
    }

    const auto* area = scrollArea(bar);
    if(!bar || !area || !canAutoHide(bar, area)) {
        remove(bar);
        return 1.0;
    }
    // A pre-polished scroll bar can be installed into a scroll area later
    polish(bar);
    const auto entry = std::ranges::find_if(m_entries, [bar](const auto& tracked) { return tracked.bar == bar; });
    return entry == m_entries.end() ? 1.0 : entry->opacity;
}

void Animation::remove(const QWidget* widget, bool restore)
{
    std::erase_if(m_entries, [this, widget, restore](const auto& entry) {
        if(entry.bar && entry.bar != widget) {
            return false;
        }
        if(entry.container) {
            entry.container->setAttribute(Qt::WA_TransparentForMouseEvents, entry.containerTransparent);
        }
        if(entry.bar && restore) {
            QObject::disconnect(entry.bar, nullptr, this, nullptr);
            entry.bar->setAttribute(Qt::WA_Hover, entry.hoverAttribute);
            entry.bar->setAttribute(Qt::WA_OpaquePaintEvent, entry.opaqueAttribute);
            entry.bar->setAttribute(Qt::WA_TransparentForMouseEvents, entry.transparentAttribute);
            entry.bar->setAutoFillBackground(entry.autoFillBackground);
        }
        return true;
    });

    if(m_entries.empty()) {
        m_timer.stop();
        qApp->removeEventFilter(this);
    }
}

bool Animation::eventFilter(QObject* watched, QEvent* event)
{
    if(auto* bar = qobject_cast<QScrollBar*>(watched)) {
        const auto entry = std::ranges::find_if(m_entries, [bar](const auto& tracked) { return tracked.bar == bar; });
        if(entry == m_entries.cend()) {
            return QObject::eventFilter(watched, event);
        }

        switch(event->type()) {
            case QEvent::Enter:
                entry->hovered = true;
                reveal(entry->area);
                break;
            case QEvent::Leave:
                entry->hovered = false;
                entry->atEdge  = false;
                reveal(entry->area);
                break;
            case QEvent::Show:
            case QEvent::FocusIn:
            case QEvent::FocusOut:
            case QEvent::Wheel:
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonRelease:
                reveal(entry->area);
                break;
            case QEvent::Hide:
                entry->hovered = false;
                entry->atEdge  = false;
                setOpacity(*entry, 0.0);
                break;
            case QEvent::ParentChange:
                remove(bar);
                break;
            default:
                break;
        }
    }

    if(event->type() == QEvent::MouseMove || event->type() == QEvent::HoverMove || event->type() == QEvent::HoverEnter
       || event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
        const auto* widget = qobject_cast<QWidget*>(watched);
        if(!widget) {
            return QObject::eventFilter(watched, event);
        }

        const bool leave    = event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave;
        const QPoint global = leave ? QPoint{}
                            : event->type() == QEvent::MouseMove
                                ? static_cast<QMouseEvent*>(event)->globalPosition().toPoint()
                                : static_cast<QHoverEvent*>(event)->globalPosition().toPoint();

        for(auto& entry : m_entries) {
            const auto* bar = qobject_cast<QScrollBar*>(entry.bar);
            if(!bar || !entry.viewport) {
                continue;
            }
            if(leave && widget != entry.viewport && widget != entry.area && widget != bar->window()) {
                continue;
            }

            const bool inView = widget == entry.viewport || entry.viewport->isAncestorOf(widget) || widget == bar
                             || widget == entry.container;

            QRect activation{bar->rect()};
            if(bar->orientation() == Qt::Vertical) {
                activation.adjust(-6, 0, 6, 0);
            }
            else {
                activation.adjust(0, -6, 0, 6);
            }

            const bool wasAtEdge{entry.atEdge};
            entry.atEdge = !leave && inView && bar->isVisible() && activation.contains(bar->mapFromGlobal(global));
            if(entry.atEdge || wasAtEdge) {
                reveal(entry.area);
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

void Animation::timerEvent(QTimerEvent* event)
{
    if(event->timerId() != m_timer.timerId()) {
        QObject::timerEvent(event);
        return;
    }

    bool animating{false};
    const qint64 now = m_elapsed.elapsed();
    for(auto& entry : m_entries) {
        const auto* bar = qobject_cast<QScrollBar*>(entry.bar);
        if(!bar || !bar->isVisible()) {
            continue;
        }
        if(entry.hovered || entry.atEdge || bar->isSliderDown() || bar->hasFocus()) {
            entry.lastActivity = now;
            setOpacity(entry, 1.0);
            continue;
        }

        const qint64 elapsed = now - entry.lastActivity - m_settings.hideDelay;
        const double opacity = elapsed <= 0 ? 1.0
                             : m_settings.fadeDuration == 0
                                 ? 0.0
                                 : std::max(0.0, 1.0 - (static_cast<double>(elapsed) / m_settings.fadeDuration));
        setOpacity(entry, opacity);
        animating |= opacity > 0.0;
    }
    if(!animating) {
        m_timer.stop();
    }
}
} // namespace Fooyin::Style::ScrollBar
