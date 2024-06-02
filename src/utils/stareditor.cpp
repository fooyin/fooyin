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

#include <QKeyEvent>
#include <QPainter>

namespace Fooyin {
StarEditor::StarEditor(QWidget* parent)
    : QWidget{parent}
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
    m_rating = rating;
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

void StarEditor::mouseMoveEvent(QMouseEvent* event)
{
    const int star = starAtPosition(event->position().toPoint().x());

    if(star != m_rating.starCount() && star != -1) {
        m_rating.setStarCount(star);
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void StarEditor::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    emit editingFinished();
}

void StarEditor::keyPressEvent(QKeyEvent* event)
{
    const int count = m_rating.starCount();
    const auto key  = event->key();

    if(key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Escape) {
        emit editingFinished();
    }
    else if(event->key() == Qt::Key_Left) {
        if(count > 0) {
            m_rating.setStarCount(count - 1);
            update();
        }
    }
    else if(event->key() == Qt::Key_Right) {
        if(count < m_rating.maxStarCount()) {
            m_rating.setStarCount(count + 1);
            update();
        }
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

int StarEditor::starAtPosition(int x) const
{
    const int star = (x / (m_rating.sizeHint().width() / m_rating.maxStarCount())) + 1;

    if(star <= 0 || star > m_rating.maxStarCount()) {
        return -1;
    }

    return star;
}
} // namespace Fooyin
