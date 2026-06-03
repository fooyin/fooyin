/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "fygui_export.h"

#include <QComboBox>

namespace Fooyin {
class FYGUI_EXPORT ExpandingComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit ExpandingComboBox(QWidget* parent = nullptr);

    [[nodiscard]] bool resizeToCurrentEnabled() const;
    void setResizeToCurrentEnabled(bool enabled);
    void resizeToFitCurrent();
    void resizeDropDown();

private:
    bool m_resizeToCurrentEnabled;
};
} // namespace Fooyin
