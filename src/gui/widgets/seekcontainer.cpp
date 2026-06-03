/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/seekcontainer.h>

#include <core/player/playercontroller.h>
#include <gui/widgets/clickablelabel.h>
#include <utils/stringutils.h>

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>

using namespace Qt::StringLiterals;

namespace {
QString widestDigitsText(QString text)
{
    for(QChar& ch : text) {
        if(ch.isDigit()) {
            ch = u'8'; // Safer upper bound
        }
    }

    return text;
}
} // namespace

namespace Fooyin {
class SeekContainerPrivate
{
public:
    SeekContainerPrivate(SeekContainer* self, PlayerController* playerController);

    void reset();
    [[nodiscard]] QString elapsedWidthText() const;
    [[nodiscard]] QString totalWidthText() const;
    void updateLabelWidth(ClickableLabel* label, const QString& text) const;
    void updateLabelWidths() const;
    [[nodiscard]] bool hasLiveDuration() const;

    void trackChanged(const Track& track);
    void stateChanged(Player::PlayState state);
    void updateLabels(uint64_t time) const;

    SeekContainer* m_self;

    PlayerController* m_playerController;

    QHBoxLayout* m_layout;
    ClickableLabel* m_elapsed;
    ClickableLabel* m_total;
    uint64_t m_max{0};
    bool m_showRemainingTime{false};
    QString m_liveText{SeekContainer::tr("Live")};
};

SeekContainerPrivate::SeekContainerPrivate(SeekContainer* self, PlayerController* playerController)
    : m_self{self}
    , m_playerController{playerController}
    , m_layout{new QHBoxLayout(m_self)}
    , m_elapsed{new ClickableLabel(Utils::msToString(0), m_self)}
    , m_total{new ClickableLabel(Utils::msToString(0), m_self)}
{
    m_layout->setContentsMargins({});

    m_layout->addWidget(m_elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
    m_layout->addWidget(m_total, 0, Qt::AlignVCenter | Qt::AlignLeft);

    trackChanged(m_playerController->currentTrack());
}

void SeekContainerPrivate::reset()
{
    m_max = 0;
    updateLabels(m_max);
    updateLabelWidths();
}

QString SeekContainerPrivate::elapsedWidthText() const
{
    return widestDigitsText(Utils::msToString(m_max));
}

QString SeekContainerPrivate::totalWidthText() const
{
    if(hasLiveDuration()) {
        return m_liveText;
    }

    const QString totalText         = widestDigitsText(Utils::msToString(m_max));
    const QString remainingTimeText = u"-"_s + totalText;
    return m_showRemainingTime ? remainingTimeText : totalText;
}

void SeekContainerPrivate::updateLabelWidth(ClickableLabel* label, const QString& text) const
{
    if(!label) {
        return;
    }

    const QFontMetrics fm{label->fontMetrics()};
    const auto margins = label->contentsMargins();
    const int width    = fm.horizontalAdvance(text) + margins.left() + margins.right() + (2 * label->margin()) + 2;
    label->setFixedWidth(width);
}

void SeekContainerPrivate::updateLabelWidths() const
{
    updateLabelWidth(m_elapsed, elapsedWidthText());
    updateLabelWidth(m_total, totalWidthText());
}

bool SeekContainerPrivate::hasLiveDuration() const
{
    const Track track = m_playerController->currentTrack();
    return track.isValid() && track.isRemote() && !m_playerController->currentTrackSeekable() && m_max == 0;
}

void SeekContainerPrivate::trackChanged(const Track& track)
{
    if(track.isValid()) {
        m_max = track.duration();
        updateLabels(0);
        updateLabelWidths();
    }
}

void SeekContainerPrivate::stateChanged(Player::PlayState state)
{
    switch(state) {
        case(Player::PlayState::Paused):
            break;
        case(Player::PlayState::Stopped):
            reset();
            break;
        case(Player::PlayState::Playing): {
            if(m_max == 0) {
                trackChanged(m_playerController->currentTrack());
            }
            break;
        }
    }
}

void SeekContainerPrivate::updateLabels(uint64_t time) const
{
    const auto elapsed = Utils::msToString(time);
    m_elapsed->setText(elapsed);

    if(hasLiveDuration()) {
        m_total->setText(m_liveText);
        return;
    }

    QString total;
    if(m_showRemainingTime) {
        const int remaining = std::max(0, static_cast<int>(m_max - time));
        total               = u"-"_s + Utils::msToString(remaining);
    }
    else {
        total = Utils::msToString(m_max);
    }
    m_total->setText(total);
}

SeekContainer::SeekContainer(PlayerController* playerController, QWidget* parent)
    : QWidget{parent}
    , p{std::make_unique<SeekContainerPrivate>(this, playerController)}
{
    QObject::connect(p->m_elapsed, &ClickableLabel::clicked, this, &SeekContainer::elapsedClicked);
    QObject::connect(p->m_total, &ClickableLabel::clicked, this, &SeekContainer::totalClicked);

    QObject::connect(p->m_playerController, &PlayerController::playStateChanged, this,
                     [this](Player::PlayState state) { p->stateChanged(state); });
    QObject::connect(p->m_playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { p->trackChanged(track); });
    QObject::connect(p->m_playerController, &PlayerController::currentTrackSeekableChanged, this, [this]() {
        p->updateLabels(p->m_playerController->currentPosition());
        p->updateLabelWidths();
    });
    QObject::connect(p->m_playerController, &PlayerController::positionChanged, this,
                     [this](uint64_t pos) { p->updateLabels(pos); });

    QObject::connect(this, &SeekContainer::totalClicked, this,
                     [this]() { setShowRemainingTime(!showRemainingTime()); });
}

SeekContainer::~SeekContainer() = default;

void SeekContainer::insertWidget(int index, QWidget* widget)
{
    p->m_layout->insertWidget(index, widget, Qt::AlignVCenter);
}

bool SeekContainer::labelsEnabled() const
{
    return !p->m_elapsed->isHidden() && !p->m_total->isHidden();
}

bool SeekContainer::showRemainingTime() const
{
    return p->m_showRemainingTime;
}

void SeekContainer::setLabelsEnabled(bool enabled)
{
    p->m_elapsed->setHidden(!enabled);
    p->m_total->setHidden(!enabled);
}

void SeekContainer::setShowRemainingTime(bool enabled)
{
    p->m_showRemainingTime = enabled;
    p->updateLabels(p->m_playerController->currentPosition());
    p->updateLabelWidths();
}

void SeekContainer::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    switch(event->type()) {
        case QEvent::FontChange:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
            p->updateLabelWidths();
            break;
        default:
            break;
    }
}
} // namespace Fooyin
