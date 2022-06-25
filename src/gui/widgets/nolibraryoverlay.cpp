/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "nolibraryoverlay.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidgetAction>

NoLibraryOverlay::NoLibraryOverlay(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
    , m_text(new QLabel("No tracks to show - add a library first", this))
    , m_addLibrary(new QPushButton("Library Settings", this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    setAutoFillBackground(true);
    m_layout->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    m_layout->addWidget(m_text);
    m_layout->addWidget(m_addLibrary);

    hide();
}

NoLibraryOverlay::~NoLibraryOverlay() = default;
