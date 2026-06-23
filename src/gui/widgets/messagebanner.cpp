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

#include <gui/widgets/messagebanner.h>

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

constexpr auto BannerMargin  = 8;
constexpr auto MaxWidthRatio = 0.9;

namespace {
QColor blend(const QColor& first, const QColor& second, const double firstWeight)
{
    const double weight = std::clamp(firstWeight, 0.0, 1.0);
    return QColor::fromRgbF((first.redF() * weight) + (second.redF() * (1.0 - weight)),
                            (first.greenF() * weight) + (second.greenF() * (1.0 - weight)),
                            (first.blueF() * weight) + (second.blueF() * (1.0 - weight)));
}
} // namespace

namespace Fooyin {
MessageBanner::MessageBanner(QWidget* parent)
    : QWidget{parent}
    , m_label{new QLabel(this)}
    , m_closeButton{new QToolButton(this)}
    , m_updatingPosition{false}
{
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

    m_label->setWordWrap(true);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    m_closeButton->setAutoRaise(true);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_DialogCloseButton));
    m_closeButton->setToolTip(tr("Dismiss"));

    QObject::connect(m_closeButton, &QToolButton::clicked, this, [this]() {
        clear();
        Q_EMIT closed();
    });

    static constexpr auto HorizontalPadding = 12;
    static constexpr auto VerticalPadding   = 8;
    static constexpr auto CloseEdgePadding  = 8;

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(HorizontalPadding, VerticalPadding, VerticalPadding, CloseEdgePadding);
    layout->addWidget(m_label, 1);
    layout->addWidget(m_closeButton, 0, Qt::AlignTop);

    if(parent) {
        parent->installEventFilter(this);
    }

    hide();
}

void MessageBanner::setText(const QString& text)
{
    m_label->setText(text);
    setVisible(!text.isEmpty());
    updatePosition();
}

void MessageBanner::clear()
{
    setText({});
}

QString MessageBanner::text() const
{
    return m_label->text();
}

bool MessageBanner::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == parent()) {
        switch(event->type()) {
            case QEvent::Paint:
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::UpdateRequest:
            case QEvent::Wheel:
                scheduleUpdatePosition();
                break;
            default:
                break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool MessageBanner::event(QEvent* event)
{
    if(event->type() == QEvent::Move && !m_updatingPosition) {
        scheduleUpdatePosition();
    }

    return QWidget::event(event);
}

void MessageBanner::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor base = palette().color(QPalette::Base);
    const QColor text = palette().color(QPalette::Text);

    QColor background = blend(base, text, base.lightnessF() < 0.5 ? 0.78 : 0.9);
    background.setAlpha(246);

    QColor border = palette().color(QPalette::Highlight);
    border.setAlpha(190);

    painter.setPen(QPen{border, 1});
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF{rect()}.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
}

void MessageBanner::updatePosition()
{
    auto* parent = parentWidget();
    if(!parent || !isVisible()) {
        return;
    }

    m_updatingPosition = true;

    const int maxWidth      = std::max(1, static_cast<int>(parent->width() * MaxWidthRatio));
    const QMargins margins  = layout()->contentsMargins();
    const int labelMaxWidth = std::max(1, maxWidth - margins.left() - margins.right() - layout()->spacing()
                                              - m_closeButton->sizeHint().width());
    m_label->setMaximumWidth(labelMaxWidth);

    setMaximumWidth(maxWidth);
    adjustSize();

    const int width  = std::min(sizeHint().width(), maxWidth);
    const int height = sizeHint().height();
    resize(width, height);

    move((parent->width() - width) / 2, std::max(BannerMargin, parent->height() - height - BannerMargin));
    raise();

    m_updatingPosition = false;
}

void MessageBanner::scheduleUpdatePosition()
{
    if(!isVisible() || m_updatingPosition) {
        return;
    }

    QTimer::singleShot(0, this, &MessageBanner::updatePosition);
}
} // namespace Fooyin

#include "gui/widgets/moc_messagebanner.cpp"
