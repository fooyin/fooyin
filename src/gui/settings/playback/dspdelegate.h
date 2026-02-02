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

#include <gui/widgets/actiondelegate.h>

namespace Fooyin {
class DspDelegate : public ActionDelegate
{
    Q_OBJECT

public:
    enum Button : uint8_t
    {
        Remove,
        Configure
    };

    explicit DspDelegate(QAbstractItemView* view, QObject* parent = nullptr);

signals:
    void removeClicked(const QModelIndex& index);
    void configureClicked(const QModelIndex& index);

protected:
    [[nodiscard]] std::vector<ActionButton> buttons(const QModelIndex& index) const override;

private:
    void buttonClicked(const QModelIndex& index, int buttonId);
};
} // namespace Fooyin
