/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/widgets/overlaywidget.h>

#include <QApplication>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fooyin {
OverlayWidget::OverlayWidget(QWidget* parent)
    : OverlayWidget{None, parent}
{ }

OverlayWidget::OverlayWidget(const Options& options, QWidget* parent)
    : QWidget{parent}
    , m_options{options}
{
    setAttribute(Qt::WA_NoSystemBackground);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);

    if(m_options & Option::Label) {
        m_text = new QLabel(this);
        layout->addWidget(m_text);
    }

    if(m_options & Option::Button) {
        m_button = new QPushButton(this);
        QObject::connect(m_button, &QPushButton::pressed, this, &OverlayWidget::buttonClicked);
        layout->addWidget(m_button);
    }

    resetColour();
    hide();
}

void OverlayWidget::setText(const QString& text)
{
    if(m_text) {
        m_text->setText(text);
    }
}

void OverlayWidget::setButtonText(const QString& text)
{
    if(m_button) {
        m_button->setText(text);
    }
}

void OverlayWidget::setColour(const QColor& colour)
{
    m_colour = colour;
    update();
}

void OverlayWidget::resetColour()
{
    static QColor colour = QApplication::palette().color(QPalette::Highlight);
    colour.setAlpha(80);
    m_colour = colour;
}

void OverlayWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};

    painter.fillRect(rect(), m_colour);
}
} // namespace Fooyin

#include "utils/widgets/moc_overlaywidget.cpp"
