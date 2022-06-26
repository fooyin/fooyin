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

#include "splitterhandle.h"

#include "utils/settings.h"

SplitterHandle::SplitterHandle(Qt::Orientation type, QSplitter* parent)
    : QSplitterHandle(type, parent)
    , m_settings(Settings::instance())
    , m_showHandles(m_settings->value(Settings::Setting::SplitterHandles).toBool())
{
    connect(m_settings, &Settings::splitterHandlesChanged, this, [this]() {
        m_showHandles = !m_showHandles;
        update();
    });
}

SplitterHandle::~SplitterHandle() = default;

void SplitterHandle::paintEvent(QPaintEvent* e)
{
    if(m_showHandles) {
        return QSplitterHandle::paintEvent(e);
    }
}
