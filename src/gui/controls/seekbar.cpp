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

#include "internalguisettings.h"

#include <core/player/playercontroller.h>
#include <core/track.h>
#include <utils/clickablelabel.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>
#include <utils/widgets/tooltip.h>

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QSlider>
#include <QStyleOptionSlider>

constexpr auto SeekDelta = 5000;

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

    updateToolTip();
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
        updateSeekPosition(event->position());
        updateToolTip();
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
        updateToolTip();
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
        seekPoint.setX(seekPoint.x() - m_toolTip->width());
        seekPoint.setX(std::clamp(seekPoint.x(), 0, width() - (2 * m_toolTip->width())));
        seekPoint.setY(rect().y() - m_toolTip->height() / 4);

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

    const auto seekPos = valueFromPosition(static_cast<int>(m_seekPos.x()));

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

struct SeekBar::Private
{
    SeekBar* self;

    PlayerController* playerController;
    SettingsManager* settings;

    TrackSlider* slider;

    ClickableLabel* elapsed;
    ClickableLabel* total;

    uint64_t max{0};
    bool elapsedTotal;

    Private(SeekBar* self_, PlayerController* playerController_, SettingsManager* settings_)
        : self{self_}
        , playerController{playerController_}
        , settings{settings_}
        , slider{new TrackSlider(self)}
        , elapsed{new ClickableLabel(self)}
        , total{new ClickableLabel(self)}
    {
        toggleElapsedTotal(settings->value<Settings::Gui::Internal::SeekBarElapsedTotal>());
        toggleLabels(settings->value<Settings::Gui::Internal::SeekBarLabels>());

        settings->subscribe<Settings::Gui::Internal::SeekBarLabels>(self,
                                                                    [this](bool enabled) { toggleLabels(enabled); });
        settings->subscribe<Settings::Gui::Internal::SeekBarElapsedTotal>(
            self, [this](bool enabled) { toggleElapsedTotal(enabled); });
    }

    void reset()
    {
        elapsed->setText(QStringLiteral("00:00"));
        total->setText(elapsedTotal ? QStringLiteral("-00:00") : QStringLiteral("00:00"));
        slider->setValue(0);
        slider->updateMaximum(0);
        max = 0;
    }

    void trackChanged(const Track& track)
    {
        reset();

        if(track.isValid()) {
            max = track.duration();
            slider->updateMaximum(max);
            const QString totalText = elapsedTotal ? QStringLiteral("-") : QStringLiteral("") + Utils::msToString(max);
            total->setText(totalText);
        }
    }

    void setCurrentPosition(uint64_t pos) const
    {
        slider->updateCurrentValue(pos);
        updateLabels(pos);
    }

    void updateLabels(uint64_t time) const
    {
        elapsed->setText(Utils::msToString(time));

        if(elapsedTotal) {
            const int remaining = static_cast<int>(max - time);
            total->setText(QStringLiteral("-") + Utils::msToString(remaining < 0 ? 0 : remaining));
        }
        else {
            if(!slider->isEnabled()) {
                total->setText(Utils::msToString(max));
            }
        }
    }

    void stateChanged(PlayState state)
    {
        switch(state) {
            case(PlayState::Stopped): {
                reset();
                slider->setEnabled(false);
                break;
            }
            case(PlayState::Playing): {
                slider->setEnabled(true);
                break;
            }
            case(PlayState::Paused): {
                return;
            }
        }
    }

    void toggleLabels(bool enabled) const
    {
        elapsed->setHidden(!enabled);
        total->setHidden(!enabled);
    }

    void toggleElapsedTotal(bool enabled)
    {
        elapsedTotal = enabled;

        if(!elapsedTotal) {
            total->setText(Utils::msToString(max));
        }
    }
};

SeekBar::SeekBar(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , p{std::make_unique<Private>(this, playerController, settings)}
{
    setMouseTracking(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);

    layout->addWidget(p->elapsed, 0, Qt::AlignVCenter | Qt::AlignLeft);
    layout->addWidget(p->slider, 0, Qt::AlignVCenter);
    layout->addWidget(p->total, 0, Qt::AlignVCenter | Qt::AlignLeft);

    p->slider->setEnabled(p->playerController->currentTrack().isValid());

    QObject::connect(p->total, &ClickableLabel::clicked, this,
                     [this]() { p->settings->set<Settings::Gui::Internal::SeekBarElapsedTotal>(!p->elapsedTotal); });
    QObject::connect(p->slider, &TrackSlider::sliderDropped, playerController, &PlayerController::seek);
    QObject::connect(p->slider, &TrackSlider::seekForward, this,
                     [this]() { p->playerController->seekForward(SeekDelta); });
    QObject::connect(p->slider, &TrackSlider::seekBackward, this,
                     [this]() { p->playerController->seekBackward(SeekDelta); });

    QObject::connect(p->playerController, &PlayerController::playStateChanged, this,
                     [this](PlayState state) { p->stateChanged(state); });
    QObject::connect(p->playerController, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { p->trackChanged(track); });
    QObject::connect(p->playerController, &PlayerController::positionChanged, this,
                     [this](uint64_t pos) { p->setCurrentPosition(pos); });
    QObject::connect(p->playerController, &PlayerController::positionMoved, this,
                     [this](uint64_t pos) { p->setCurrentPosition(pos); });

    p->trackChanged(p->playerController->currentTrack());
}

Fooyin::SeekBar::~SeekBar() = default;

QString SeekBar::name() const
{
    return QStringLiteral("SeekBar");
}

void SeekBar::contextMenuEvent(QContextMenuEvent* event)
{
    if(p->slider->isSeeking()) {
        p->slider->stopSeeking();
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showLabels = new QAction(tr("Show Labels"), this);
    showLabels->setCheckable(true);
    showLabels->setChecked(p->settings->value<Settings::Gui::Internal::SeekBarLabels>());
    QObject::connect(showLabels, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::Internal::SeekBarLabels>(checked); });
    menu->addAction(showLabels);

    auto* showElapsed = new QAction(tr("Show Elapsed Total"), this);
    showElapsed->setCheckable(true);
    showElapsed->setChecked(p->settings->value<Settings::Gui::Internal::SeekBarElapsedTotal>());
    QObject::connect(showElapsed, &QAction::triggered, this,
                     [this](bool checked) { p->settings->set<Settings::Gui::Internal::SeekBarElapsedTotal>(checked); });
    menu->addAction(showElapsed);

    menu->popup(event->globalPos());
}
} // namespace Fooyin

#include "moc_seekbar.cpp"
#include "seekbar.moc"
