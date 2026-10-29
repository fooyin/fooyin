/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <gui/widgets/actiondelegate.h>

namespace Fooyin {
class FYGUI_EXPORT DspDelegate : public ActionDelegate
{
    Q_OBJECT

public:
    enum Button
    {
        Remove = Qt::UserRole + 100,
        Configure,
        Add,
    };

    enum class Mode : uint8_t
    {
        Active,
        Available,
    };

    explicit DspDelegate(QAbstractItemView* view, QObject* parent = nullptr, Mode mode = Mode::Active);

Q_SIGNALS:
    void removeClicked(const QModelIndex& index);
    void configureClicked(const QModelIndex& index);
    void addClicked(const QModelIndex& index);

protected:
    [[nodiscard]] std::vector<ActionButton> buttons(const QModelIndex& index) const override;

private:
    void buttonWasClicked(const QModelIndex& index, int buttonId);

    Mode m_mode;
};
} // namespace Fooyin
