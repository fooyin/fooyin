/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "seekbar.h"

#include <core/player/playercontroller.h>
#include <core/track.h>
#include <gui/guisettings.h>
#include <gui/widgets/seekcontainer.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>
#include <utils/widgets/tooltip.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMenu>
#include <QPointer>
#include <QSlider>
#include <QStyleOptionSlider>

constexpr auto ToolTipDelay = 5;

namespace Fooyin {
class TrackSlider : public QSlider
{
    Q_OBJECT

public:
    explicit TrackSlider(QWidget* parent = nullptr);

    [[nodiscard]] int positionFromValue(uint64_t value) const;
    uint64_t valueFromPosition(int pos);

    void updateMaximum(uint64_t max);
    void updateCurrentValue(uint64_t value);

    [[nodiscard]] bool isSeeking() const;
    void stopSeeking();

signals:
    void sliderDropped(uint64_t pos);
    void seekForward();
    void seekBackward();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateSeekPosition(const QPointF& pos);
    void updateToolTip();

    QPointer<ToolTip> m_toolTip;
    uint64_t m_max{0};
    uint64_t m_currentPos{0};
    QPoint m_pressPos;
    QPoint m_seekPos;
};

TrackSlider::TrackSlider(QWidget* parent)
    : QSlider{Qt::Horizontal, parent}
{ }

uint64_t TrackSlider::valueFromPosition(int pos)
{
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

    const int handleWidth = handle.width();
    const int sliderPos   = pos - (handleWidth / 2) - groove.x() + 1;
    const int span        = groove.right() - handleWidth - groove.x() + 1;

    return static_cast<uint64_t>(QStyle::sliderValueFromPosition(0, maximum(), sliderPos, span, opt.upsideDown));
}

void TrackSlider::updateMaximum(uint64_t max)
{
    m_max = max;
    setMaximum(static_cast<int>(max));
}

void TrackSlider::updateCurrentValue(uint64_t value)
{
    m_currentPos = value;

    if(!isSeeking()) {
        setValue(static_cast<int>(value));
    }

    if(m_toolTip) {
        updateToolTip();
    }
}

bool TrackSlider::isSeeking() const
{
    return !m_seekPos.isNull();
}

void TrackSlider::stopSeeking()
{
    if(m_toolTip) {
        m_toolTip->deleteLater();
    }

    m_seekPos = {};
}

void TrackSlider::mousePressEvent(QMouseEvent* event)
{
    if(m_max == 0) {
        return;
    }

    Qt::MouseButton button = event->button();
    if(button == Qt::LeftButton) {
        const int absolute = style()->styleHint(QStyle::SH_Slider_AbsoluteSetButtons);
        if(Qt::LeftButton & absolute) {
            button = Qt::LeftButton;
        }
        else if(Qt::MiddleButton & absolute) {
            button = Qt::MiddleButton;
        }
        else if(Qt::RightButton & absolute) {
            button = Qt::RightButton;
        }
    }

    QMouseEvent modifiedEvent{event->type(), event->position(), event->globalPosition(), button,
                              button,        event->modifiers()};
    QSlider::mousePressEvent(&modifiedEvent);

    if(event->button() == Qt::LeftButton) {
        m_pressPos = event->position().toPoint();
        updateSeekPosition(event->position());
    }
}

void TrackSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if(m_max == 0) {
        return;
    }

    QSlider::mouseReleaseEvent(event);

    if(event->button() != Qt::LeftButton || !isSeeking()) {
        return;
    }

    stopSeeking();
    m_pressPos = {};

    const auto pos = valueFromPosition(static_cast<int>(event->position().x()));
    emit sliderDropped(pos);
}

void TrackSlider::mouseMoveEvent(QMouseEvent* event)
{
    if(m_max == 0) {
        return;
    }

    QSlider::mouseMoveEvent(event);

    if(isSeeking() && event->buttons() & Qt::LeftButton) {
        updateSeekPosition(event->position());
        if(!m_pressPos.isNull() && std::abs(m_pressPos.x() - event->position().x()) > ToolTipDelay) {
            updateToolTip();
        }
    }
}

void TrackSlider::keyPressEvent(QKeyEvent* event)
{
    const auto key = event->key();

    if(key == Qt::Key_Right || key == Qt::Key_Up) {
        emit seekForward();
        event->accept();
    }
    else if(key == Qt::Key_Left || key == Qt::Key_Down) {
        emit seekBackward();
        event->accept();
    }
    else {
        QSlider::keyPressEvent(event);
    }
}

void TrackSlider::wheelEvent(QWheelEvent* event)
{
    if(event->angleDelta().y() < 0) {
        emit seekBackward();
    }
    else {
        emit seekForward();
    }

    event->accept();
}

void TrackSlider::updateSeekPosition(const QPointF& pos)
{
    m_seekPos = pos.toPoint();

    QPoint seekPoint = pos.toPoint();

    if(m_toolTip) {
        const int yPosToWindow  = mapToGlobal(QPoint{0, 0}).y();
        const bool displayAbove = (yPosToWindow - (height() + m_toolTip->height())) > 0;

        seekPoint.setX(seekPoint.x() - m_toolTip->width());
        seekPoint.setX(std::clamp(seekPoint.x(), 0, width() - (2 * m_toolTip->width())));

        if(displayAbove) {
            seekPoint.setY(rect().y() - m_toolTip->height() / 4);
        }
        else {
            seekPoint.setY(rect().bottom() + (height() + m_toolTip->height()));
        }

        m_toolTip->setPosition(mapTo(window(), seekPoint));
    }

    updateToolTip();
}

void TrackSlider::updateToolTip()
{
    if(!m_toolTip) {
        m_toolTip = new ToolTip(window());
        m_toolTip->raise();
        m_toolTip->show();
    }

    const auto seekPos = valueFromPosition(m_seekPos.x());

    const uint64_t seekDelta = std::max(m_currentPos, seekPos) - std::min(m_currentPos, seekPos);

    QString deltaText;

    if(seekPos > m_currentPos) {
        deltaText = QStringLiteral("+") + Utils::msToString(seekDelta);
    }
    else {
        deltaText = QStringLiteral("-") + Utils::msToString(seekDelta);
    }

    m_toolTip->setText(Utils::msToString(seekPos));
    m_toolTip->setSubtext(deltaText);
}

SeekBar::SeekBar(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_container{new SeekContainer(playerController, this)}
    , m_slider{new TrackSlider(this)}
{
    setMouseTracking(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});

    layout->addWidget(m_container);
    m_container->insertWidget(1, m_slider);

    m_slider->setEnabled(playerController->currentTrack().isValid());
    trackChanged(playerController->currentTrack());

    QObject::connect(m_slider, &TrackSlider::sliderDropped, playerController, &PlayerController::seek);
    QObject::connect(m_slider, &TrackSlider::seekForward, this,
                     [this]() { m_playerController->seekForward(m_settings->value<Settings::Gui::SeekStep>()); });
    QObject::connect(m_slider, &TrackSlider::seekBackward, this,
                     [this]() { m_playerController->seekBackward(m_settings->value<Settings::Gui::SeekStep>()); });

    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &SeekBar::stateChanged);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &SeekBar::trackChanged);
    QObject::connect(m_playerController, &PlayerController::positionChanged, this, &SeekBar::setCurrentPosition);
    QObject::connect(m_playerController, &PlayerController::positionMoved, this, &SeekBar::setCurrentPosition);
}

QString SeekBar::name() const
{
    return tr("Seekbar");
}

QString SeekBar::layoutName() const
{
    return QStringLiteral("SeekBar");
}

void SeekBar::saveLayoutData(QJsonObject& layout)
{
    layout[QStringLiteral("ShowLabels")]   = m_container->labelsEnabled();
    layout[QStringLiteral("ElapsedTotal")] = m_container->elapsedTotal();
}

void SeekBar::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(QStringLiteral("ShowLabels"))) {
        const bool showLabels = layout.value(QStringLiteral("ShowLabels")).toBool();
        m_container->setLabelsEnabled(showLabels);
    }
    if(layout.contains(QStringLiteral("ElapsedTotal"))) {
        const bool elapsedTotal = layout.value(QStringLiteral("ElapsedTotal")).toBool();
        m_container->setElapsedTotal(elapsedTotal);
    }
}

void SeekBar::contextMenuEvent(QContextMenuEvent* event)
{
    if(m_slider->isSeeking()) {
        m_slider->stopSeeking();
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showLabels = new QAction(tr("Show labels"), this);
    showLabels->setCheckable(true);
    showLabels->setChecked(m_container->labelsEnabled());
    QObject::connect(showLabels, &QAction::triggered, this,
                     [this](bool checked) { m_container->setLabelsEnabled(checked); });
    menu->addAction(showLabels);

    auto* showElapsed = new QAction(tr("Show elapsed total"), this);
    showElapsed->setCheckable(true);
    showElapsed->setChecked(m_container->elapsedTotal());
    QObject::connect(showElapsed, &QAction::triggered, this,
                     [this](bool checked) { m_container->setElapsedTotal(checked); });
    menu->addAction(showElapsed);

    menu->popup(event->globalPos());
}

void SeekBar::reset()
{
    m_max = 0;
    m_slider->setValue(0);
    m_slider->updateMaximum(m_max);
}

void SeekBar::trackChanged(const Track& track)
{
    if(track.isValid()) {
        m_max = track.duration();
        m_slider->updateMaximum(m_max);
    }
}

void SeekBar::setCurrentPosition(uint64_t pos) const
{
    m_slider->updateCurrentValue(pos);
}

void SeekBar::stateChanged(Player::PlayState state)
{
    switch(state) {
        case(Player::PlayState::Paused):
            break;
        case(Player::PlayState::Stopped):
            reset();
            m_slider->setEnabled(false);
            break;
        case(Player::PlayState::Playing): {
            if(m_max == 0) {
                trackChanged(m_playerController->currentTrack());
            }
            m_slider->setEnabled(true);
            break;
        }
    }
}
} // namespace Fooyin

#include "moc_seekbar.cpp"
#include "seekbar.moc"
