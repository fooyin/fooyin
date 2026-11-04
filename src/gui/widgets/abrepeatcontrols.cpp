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

#include <gui/widgets/abrepeatcontrols.h>

#include <core/player/playercontroller.h>
#include <core/track.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/tooltip.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/actions/proxyaction.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>

#include <QCoreApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QWidget>

using namespace Qt::StringLiterals;

namespace Fooyin {
class ABRepeatControlsPrivate
{
public:
    ABRepeatControlsPrivate(PlayerController* playerController, SettingsManager* settings, QWidget* widget);

    void refreshPlaybackState();
    void refreshColours();

    [[nodiscard]] bool canEdit() const;

    [[nodiscard]] double coordinate(uint64_t position) const;
    [[nodiscard]] uint64_t positionAt(const QPointF& point) const;
    void dragMarker(const auto* mouse);
    void finishDragging();
    void stopDragging();

    PlayerController* m_playerController;
    QWidget* m_widget;
    Player::ABRepeatRange m_range;
    QColor m_customColourA;
    QColor m_customColourB;
    QColor m_colourA;
    QColor m_colourB;
    QColor m_fillColour;
    double m_markerWidth;
    bool m_seekToStartOnChange;
    bool m_displayingCurrentTrack{false};
    bool m_seekable{false};
    bool m_middleClick{false};
    Track m_track;
    QRectF m_area;
    Qt::Orientation m_orientation{Qt::Horizontal};
    bool m_inverted{false};
    std::optional<bool> m_dragStart;
    std::optional<uint64_t> m_previewStart;
    QPointer<ToolTip> m_toolTip;
};

ABRepeatControlsPrivate::ABRepeatControlsPrivate(PlayerController* playerController, SettingsManager* settings,
                                                 QWidget* widget)
    : m_playerController{playerController}
    , m_widget{widget}
    , m_customColourA{settings->value<Settings::Gui::ABRepeatMarkerColourA>().value<QColor>()}
    , m_customColourB{settings->value<Settings::Gui::ABRepeatMarkerColourB>().value<QColor>()}
    , m_markerWidth{std::clamp(settings->value<Settings::Gui::ABRepeatMarkerWidth>(), 0.5, 10.0)}
    , m_seekToStartOnChange{settings->value<Settings::Gui::ABRepeatSeekOnAChange>()}
{
    refreshPlaybackState();
}

void ABRepeatControlsPrivate::refreshPlaybackState()
{
    m_range                  = m_playerController->abRepeatRange();
    m_displayingCurrentTrack = m_track.isValid() && m_track.duration() > 0
                            && m_track.sameIdentityAs(m_playerController->currentTrack())
                            && m_playerController->playState() != Player::PlayState::Stopped;
    m_seekable               = m_playerController->currentTrackSeekable();

    if(!canEdit() || (m_dragStart && !(*m_dragStart ? m_range.start : m_range.end))) {
        stopDragging();
    }

    refreshColours();
}

void ABRepeatControlsPrivate::refreshColours()
{
    const QColor defaultColour
        = m_widget->palette().color(m_range.enabled ? QPalette::Highlight : QPalette::WindowText);
    m_colourA    = m_customColourA.isValid() ? m_customColourA : ABRepeatControls::defaultColourA(m_widget->palette());
    m_colourB    = m_customColourB.isValid() ? m_customColourB : ABRepeatControls::defaultColourB(m_widget->palette());
    m_fillColour = defaultColour;
    m_fillColour.setAlpha(defaultColour.alpha() * (m_range.enabled ? 35 : 15) / 255);
    if(m_toolTip) {
        m_toolTip->setPalette(m_widget->palette());
    }
    m_widget->update();
}

bool ABRepeatControlsPrivate::canEdit() const
{
    return m_displayingCurrentTrack && m_track.duration() >= 100 && m_widget->isEnabled() && m_seekable;
}

double ABRepeatControlsPrivate::coordinate(uint64_t position) const
{
    double ratio = std::clamp(static_cast<double>(position) / static_cast<double>(m_track.duration()), 0.0, 1.0);
    if(m_inverted) {
        ratio = 1.0 - ratio;
    }
    return m_orientation == Qt::Horizontal ? m_area.left() + (ratio * m_area.width())
                                           : m_area.top() + (ratio * m_area.height());
}

uint64_t ABRepeatControlsPrivate::positionAt(const QPointF& point) const
{
    const bool horizontal = m_orientation == Qt::Horizontal;
    const double span     = horizontal ? m_area.width() : m_area.height();
    if(span <= 0.0) {
        return 0;
    }

    double ratio = std::clamp(
        ((horizontal ? point.x() : point.y()) - (horizontal ? m_area.left() : m_area.top())) / span, 0.0, 1.0);
    if(m_inverted) {
        ratio = 1.0 - ratio;
    }
    return std::llround(ratio * static_cast<double>(m_track.duration()));
}

void ABRepeatControlsPrivate::dragMarker(const auto* mouse)
{
    if(!m_dragStart.has_value()) {
        return;
    }

    const bool draggingStart = *m_dragStart;
    uint64_t position        = positionAt(mouse->position());

    if(draggingStart) {
        position = std::min(position, m_range.end.value_or(m_track.duration()) - 1);
        if(m_previewStart) {
            m_previewStart = position;
            m_widget->update();
        }
        else {
            m_playerController->setABRepeatStart(position);
        }
    }
    else {
        position = std::max(position, m_range.start.value_or(0) + 1);
        m_playerController->setABRepeatEnd(position);
    }

    if(!m_toolTip) {
        m_toolTip = new ToolTip(m_widget);
        m_toolTip->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        m_toolTip->setAttribute(Qt::WA_ShowWithoutActivating);
        m_toolTip->setPalette(m_widget->palette());
    }

    m_toolTip->setText((draggingStart ? u"A: "_s : u"B: "_s) + Utils::msToString(position));
    m_toolTip->setPosition(mouse->globalPosition().toPoint() + QPoint{12, 24});
    m_toolTip->show();
}

void ABRepeatControlsPrivate::finishDragging()
{
    const auto previewStart{m_previewStart};
    stopDragging();
    if(previewStart && canEdit()) {
        m_playerController->setABRepeatStart(*previewStart, m_seekToStartOnChange);
    }
}

void ABRepeatControlsPrivate::stopDragging()
{
    if(m_dragStart) {
        if(m_toolTip) {
            m_toolTip->hide();
            m_toolTip->deleteLater();
            m_toolTip = nullptr;
        }
        m_dragStart.reset();
        m_previewStart.reset();
        m_widget->update();
    }
}

ABRepeatControls::ABRepeatControls(PlayerController* playerController, SettingsManager* settings, QWidget* widget)
    : QObject{widget}
    , p{std::make_unique<ABRepeatControlsPrivate>(playerController, settings, widget)}
{
    widget->installEventFilter(this);

    const auto refresh = [this]() {
        p->refreshPlaybackState();
    };
    QObject::connect(playerController, &PlayerController::abRepeatRangeChanged, this, refresh);
    QObject::connect(playerController, &PlayerController::currentTrackChanged, this, refresh);
    QObject::connect(playerController, &PlayerController::currentTrackUpdated, this, refresh);
    QObject::connect(playerController, &PlayerController::playStateChanged, this, refresh);
    QObject::connect(playerController, &PlayerController::currentTrackSeekableChanged, this, refresh);

    settings->subscribe<Settings::Gui::ABRepeatMarkerWidth>(this, [this](double width) {
        p->m_markerWidth = std::clamp(width, 0.5, 10.0);
        p->m_widget->update();
    });
    settings->subscribe<Settings::Gui::ABRepeatMarkerColourA>(this, [this](const QVariant& value) {
        p->m_customColourA = value.value<QColor>();
        p->refreshColours();
    });
    settings->subscribe<Settings::Gui::ABRepeatMarkerColourB>(this, [this](const QVariant& value) {
        p->m_customColourB = value.value<QColor>();
        p->refreshColours();
    });
    settings->subscribe<Settings::Gui::ResolvedAppStyle>(this, [this]() { p->refreshColours(); });
    settings->subscribe<Settings::Gui::ABRepeatSeekOnAChange>(
        this, [this](bool enabled) { p->m_seekToStartOnChange = enabled; });
}

ABRepeatControls::~ABRepeatControls() = default;

QColor ABRepeatControls::defaultColourA(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightness() < 128 ? QColor{0x62, 0xc9, 0x8d} : QColor{0x23, 0x83, 0x4b};
}

QColor ABRepeatControls::defaultColourB(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightness() < 128 ? QColor{0xf0, 0xb3, 0x5f} : QColor{0xb5, 0x6a, 0x16};
}

void ABRepeatControls::setTrack(const Track& track)
{
    if(!p->m_track.sameIdentityAs(track)) {
        p->stopDragging();
    }
    p->m_track = track;
    p->refreshPlaybackState();
}

void ABRepeatControls::paint(QPainter& painter, const QRectF& area, Qt::Orientation orientation, bool inverted)
{
    p->m_area        = area;
    p->m_orientation = orientation;
    p->m_inverted    = inverted;

    if(!p->m_displayingCurrentTrack || area.isEmpty()) {
        return;
    }

    auto range{p->m_range};
    if(p->m_previewStart) {
        range.start = p->m_previewStart;
    }
    const bool horizontal = orientation == Qt::Horizontal;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setClipRect(p->m_widget->rect(), Qt::IntersectClip);

    const QTransform toDevice   = painter.deviceTransform();
    const QTransform fromDevice = toDevice.inverted();

    const auto markerCoord = [&](uint64_t position) {
        const double pos      = p->coordinate(position);
        const QPointF device  = toDevice.map(horizontal ? QPointF{pos, 0.0} : QPointF{0.0, pos});
        const QPointF aligned = fromDevice.map(QPointF{std::round(device.x()), std::round(device.y())});
        return horizontal ? aligned.x() : aligned.y();
    };

    if(range.start && range.end) {
        const double start = markerCoord(*range.start);
        const double end   = markerCoord(*range.end);

        painter.fillRect(horizontal ? QRectF{std::min(start, end), area.top(), std::abs(end - start), area.height()}
                                    : QRectF{area.left(), std::min(start, end), area.width(), std::abs(end - start)},
                         p->m_fillColour);
    }

    QPen pen{p->m_colourA, p->m_markerWidth, Qt::SolidLine, Qt::FlatCap};
    pen.setCosmetic(true);
    if(!range.enabled) {
        painter.setOpacity(painter.opacity() * 0.6);
    }

    const auto drawMarker = [&](std::optional<uint64_t> position, const QColor& colour) {
        if(!position) {
            return;
        }

        pen.setColor(colour);
        painter.setPen(pen);

        const double pos = markerCoord(*position);
        if(horizontal) {
            painter.drawLine(QPointF{pos, area.top()}, QPointF{pos, area.bottom()});
        }
        else {
            painter.drawLine(QPointF{area.left(), pos}, QPointF{area.right(), pos});
        }
    };

    drawMarker(range.start, p->m_colourA);
    drawMarker(range.end, p->m_colourB);
    painter.restore();
}

void ABRepeatControls::populateMenu(QMenu* menu, ActionManager* actionManager, std::optional<QPoint> position)
{
    auto* repeatMenu = menu->addMenu(tr("A–B repeat"));

    const auto addAction = [repeatMenu](QAction* registeredAction, const QString& text, const QString& mouseHint) {
        auto* action = new ProxyAction(repeatMenu);
        action->initialise(registeredAction);

        const QString shortcut = registeredAction->shortcut().toString(QKeySequence::NativeText);
        const QString hint     = shortcut.isEmpty() ? mouseHint : shortcut + u" | "_s + mouseHint;
        action->setText(text + u"\t"_s + hint);

        repeatMenu->addAction(action);
        return action;
    };

    const auto middleClickHint = [](Qt::Modifiers modifiers) {
        const QString keys = QKeySequence(modifiers).toString(QKeySequence::NativeText);
        //: %1 is a native modifier sequence, including its trailing separator e.g. Ctrl+Shift+
        return tr("%1Middle-click").arg(keys);
    };

    const auto range    = p->m_range;
    const bool clicked  = position && p->m_area.contains(*position);
    const uint64_t time = clicked ? p->positionAt(*position) : p->m_playerController->currentPosition();

    auto* startAction = actionManager->command(Constants::Actions::ABRepeatSetStart)->action();
    auto* start       = addAction(startAction, clicked ? tr("Set A here") : tr("Set A"), tr("Middle-click"));
    start->setEnabled(p->canEdit() && time < p->m_track.duration());
    QObject::connect(start, &QAction::triggered, this, [this, time]() {
        if(p->canEdit() && p->m_track.sameIdentityAs(p->m_playerController->currentTrack())) {
            p->m_playerController->setABRepeatStart(time, p->m_seekToStartOnChange);
        }
    });

    auto* endAction = actionManager->command(Constants::Actions::ABRepeatSetEnd)->action();
    auto* end       = addAction(endAction, clicked ? tr("Set B here") : tr("Set B"), middleClickHint(Qt::SHIFT));
    end->setEnabled(p->canEdit() && range.start && time > *range.start);
    QObject::connect(end, &QAction::triggered, this, [this, time]() {
        if(p->canEdit() && p->m_track.sameIdentityAs(p->m_playerController->currentTrack())) {
            p->m_playerController->setABRepeatEnd(time);
        }
    });

    auto* enabledAction = actionManager->command(Constants::Actions::ABRepeatEnable)->action();
    auto* enabled       = addAction(enabledAction, enabledAction->text(), middleClickHint(Qt::SHIFT | Qt::CTRL));
    enabled->setAction(enabledAction);

    const auto updateEnabled = [this, enabled, enabledAction]() {
        enabled->setEnabled(p->canEdit() && enabledAction->isEnabled());
    };
    QObject::connect(enabledAction, &QAction::changed, enabled, updateEnabled);
    updateEnabled();

    auto* clearAction = actionManager->command(Constants::Actions::ABRepeatClear)->action();
    auto* clear       = addAction(clearAction, clearAction->text(), middleClickHint(Qt::CTRL));
    clear->setAction(clearAction);
}

bool ABRepeatControls::eventFilter(QObject* watched, QEvent* event)
{
    if(event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        p->refreshColours();
    }

    if(event->type() == QEvent::Hide || event->type() == QEvent::UngrabMouse
       || event->type() == QEvent::WindowDeactivate || event->type() == QEvent::Resize) {
        p->stopDragging();
        p->m_middleClick = false;
    }

    if(p->m_dragStart && event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        p->stopDragging();
        event->accept();
        return true;
    }

    if(event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick) {
        const auto* mouse         = static_cast<QMouseEvent*>(event);
        const auto modifiers      = mouse->modifiers();
        const bool middleClick    = mouse->button() == Qt::MiddleButton && mouse->buttons() == Qt::MiddleButton;
        const bool validModifiers = modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier
                                 || modifiers == Qt::ControlModifier
                                 || modifiers == (Qt::ShiftModifier | Qt::ControlModifier);

        if(middleClick && validModifiers && p->canEdit() && p->m_area.contains(mouse->position())) {
            p->stopDragging();
            p->m_middleClick = true;

            const uint64_t position = p->positionAt(mouse->position());

            if(modifiers == (Qt::ShiftModifier | Qt::ControlModifier)) {
                if(p->m_range.start && p->m_range.end) {
                    p->m_playerController->setABRepeatEnabled(!p->m_range.enabled);
                }
            }
            else if(modifiers == Qt::ControlModifier) {
                p->m_playerController->clearABRepeat();
            }
            else if(modifiers == Qt::ShiftModifier) {
                p->m_playerController->setABRepeatEnd(position);
            }
            else {
                p->m_playerController->setABRepeatStart(position, p->m_seekToStartOnChange);
            }

            event->accept();
            return true;
        }
    }

    if(event->type() == QEvent::MouseButtonRelease && p->m_middleClick
       && static_cast<QMouseEvent*>(event)->button() == Qt::MiddleButton) {
        p->m_middleClick = false;
        event->accept();
        return true;
    }

    if(event->type() == QEvent::MouseButtonPress) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if(mouse->button() == Qt::LeftButton && mouse->buttons() == Qt::LeftButton && p->canEdit()
           && p->m_area.contains(mouse->position())) {
            const auto range = p->m_range;
            const double pos = p->m_orientation == Qt::Horizontal ? mouse->position().x() : mouse->position().y();

            const double startDistance = range.start ? std::abs(pos - p->coordinate(*range.start)) : 1000;
            const double endDistance   = range.end ? std::abs(pos - p->coordinate(*range.end)) : 1000;
            if(std::min(startDistance, endDistance) <= 6) {
                p->m_dragStart = startDistance <= endDistance;
                if(*p->m_dragStart && p->m_seekToStartOnChange && range.enabled) {
                    p->m_previewStart = range.start;
                }
                event->accept();
                return true;
            }
        }

        if(p->m_dragStart) {
            p->stopDragging();
            event->accept();
            return true;
        }
    }

    if(p->m_dragStart && (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonRelease)) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if(!p->canEdit() || (event->type() == QEvent::MouseMove && !mouse->buttons().testFlag(Qt::LeftButton))) {
            p->stopDragging();
            return true;
        }

        p->dragMarker(mouse);

        if(event->type() == QEvent::MouseButtonRelease) {
            p->finishDragging();
        }

        event->accept();
        return true;
    }

    return QObject::eventFilter(watched, event);
}
} // namespace Fooyin

#include "gui/widgets/moc_abrepeatcontrols.cpp"
