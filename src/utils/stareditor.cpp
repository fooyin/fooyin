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

#include <utils/stareditor.h>

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QPainter>

namespace Fooyin {
StarEditor::StarEditor(QWidget* parent)
    : QWidget{parent}
    , m_originalRating{0}
{
    setMouseTracking(true);
    setAutoFillBackground(true);
}

StarRating StarEditor::rating() const
{
    return m_rating;
}

void StarEditor::setRating(const StarRating& rating)
{
    m_rating         = rating;
    m_originalRating = rating.rating();
}

float StarEditor::ratingAtPosition(int x) const
{
    const float maxStars  = static_cast<float>(m_rating.maxStarCount());
    const float starWidth = static_cast<float>(m_rating.sizeHint().width()) / maxStars;
    const float starIndex = static_cast<float>(x) / starWidth;
    return std::clamp(starIndex, 0.0F, maxStars) / maxStars;
}

float StarEditor::ratingAtPosition(const QPoint& pos, const QRect& rect, const StarRating& rating)
{
    const float maxStars  = static_cast<float>(rating.maxStarCount());
    const float starWidth = static_cast<float>(rating.sizeHint().width()) / maxStars;
    const float x         = pos.x() - (rect.x() + (rect.width() - rating.sizeHint().width()) / 2);

    if(x < starWidth / 2) {
        return 0.0F;
    }

    const float starIndex = x / starWidth;
    return std::clamp(starIndex, 0.0F, maxStars) / maxStars;
}

QSize StarEditor::sizeHint() const
{
    return m_rating.sizeHint();
}

void StarEditor::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    m_rating.paint(&painter, rect(), palette(), StarRating::EditMode::Editable);
}

void StarEditor::contextMenuEvent(QContextMenuEvent* event)
{
    // Prevent showing parent widget context menu
    event->accept();
}

void StarEditor::mouseMoveEvent(QMouseEvent* event)
{
    const auto star = ratingAtPosition(event->position().toPoint().x());

    if(star != m_rating.rating() && star != -1) {
        m_rating.setRating(star);
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void StarEditor::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::RightButton) {
        m_rating.setRating(m_originalRating);
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void StarEditor::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    emit editingFinished();
}

void StarEditor::keyPressEvent(QKeyEvent* event)
{
    const float rating = m_rating.rating();
    const int key      = event->key();

    if(key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Escape) {
        emit editingFinished();
    }
    else if(event->key() == Qt::Key_Left) {
        if(rating > 0) {
            m_rating.setRating(rating - 1);
            update();
        }
    }
    else if(event->key() == Qt::Key_Right) {
        if(rating < static_cast<float>(m_rating.maxStarCount())) {
            m_rating.setRating(0);
            update();
        }
    }
    else {
        QWidget::keyPressEvent(event);
    }
}
} // namespace Fooyin
