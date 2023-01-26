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

#include "splitter.h"

#include "gui/guisettings.h"
#include "splitterhandle.h"

#include <QBoxLayout>
#include <QMenu>

namespace Gui::Widgets {
Splitter::Splitter(Qt::Orientation type, Core::SettingsManager* settings, QWidget* parent)
    : QSplitter{type, parent}
    , m_settings{settings}
{
    setObjectName("Splitter");
    setChildrenCollapsible(false);
}

QSplitterHandle* Splitter::createHandle()
{
    auto* handle = new SplitterHandle(orientation(), this);
    m_settings->subscribe<Settings::SplitterHandles>(handle, &SplitterHandle::showHandle);
    return handle;
}
} // namespace Gui::Widgets
