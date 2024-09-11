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

#include <gui/widgets/colourbutton.h>

#include <QColorDialog>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace Fooyin {
ColourButton::ColourButton(QWidget* parent)
    : ColourButton{{}, parent}
{ }

ColourButton::ColourButton(const QColor& colour, QWidget* parent)
    : QWidget{parent}
    , m_colour{colour}
    , m_changed{false}
{
    setMinimumSize(20, 20);

    QObject::connect(this, &ColourButton::clicked, this, &ColourButton::pickColour);
}

QColor ColourButton::colour() const
{
    return m_colour;
}

bool ColourButton::colourChanged() const
{
    return m_changed;
}

void ColourButton::setColour(const QColor& colour)
{
    m_colour = colour;
    update();
}

void ColourButton::mousePressEvent(QMouseEvent* event)
{
    QWidget::mousePressEvent(event);

    if(event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

void ColourButton::pickColour()
{
    const QColor chosenColour
        = QColorDialog::getColor(m_colour, this, tr("Select Colour"), QColorDialog::ShowAlphaChannel);
    if(chosenColour.isValid() && chosenColour != m_colour) {
        setColour(chosenColour);
        m_changed = true;
    }
}

void ColourButton::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter{this};
    painter.fillRect(rect(), m_colour);

    painter.setPen({palette().text().color(), 2});
    painter.drawRect(rect());
}
} // namespace Fooyin

#include "gui/widgets/moc_colourbutton.cpp"
