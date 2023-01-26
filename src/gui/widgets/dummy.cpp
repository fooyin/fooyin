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

#include "dummy.h"

namespace Gui::Widgets {
Dummy::Dummy(QWidget* parent)
    : FyWidget{parent}
    , m_layout{new QHBoxLayout(this)}
    , m_label{new QLabel(this)}
{
    setObjectName(Dummy::name());

    m_layout->setContentsMargins(0, 0, 0, 0);

    m_label->setText("Right-Click to add a new widget.");
    m_label->setAutoFillBackground(true);

    QPalette palette = m_label->palette();
    palette.setColor(m_label->backgroundRole(), palette.base().color());
    m_label->setPalette(palette);

    m_label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_label->setAlignment(Qt::AlignCenter);

    m_layout->addWidget(m_label);
}

QString Dummy::name() const
{
    return "Dummy";
}
} // namespace Gui::Widgets
