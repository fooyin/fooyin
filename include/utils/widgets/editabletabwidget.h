/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#pragma once

#include "fyutils_export.h"

#include <QTabWidget>

namespace Fooyin {
class EditableTabBar;

class FYUTILS_EXPORT EditableTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit EditableTabWidget(QWidget* parent = nullptr);

    [[nodiscard]] EditableTabBar* editableTabBar() const;

private:
    EditableTabBar* m_tabBar;
};
} // namespace Fooyin
