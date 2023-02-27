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

#include "overlaywidget.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fy::Utils {
OverlayWidget::OverlayWidget(bool button, QWidget* parent)
    : QWidget{parent}
    , m_layout{new QVBoxLayout(this)}
    , m_text{new QLabel(this)}
{
    if(button) {
        m_button = new QPushButton(this);
        QObject::connect(m_button, &QPushButton::pressed, this, &OverlayWidget::settingsClicked);
    }
    m_layout->setContentsMargins(0, 0, 0, 0);
    setAutoFillBackground(true);
    m_layout->setAlignment(Qt::AlignCenter);
    m_layout->addWidget(m_text);
    m_layout->addWidget(m_button);

    hide();
}

void OverlayWidget::setText(const QString& text)
{
    m_text->setText(text);
}

void OverlayWidget::setButtonText(const QString& text)
{
    if(m_button) {
        m_button->setText(text);
    }
}
} // namespace Fy::Utils
