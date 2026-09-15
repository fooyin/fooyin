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

#include <core/library/libraryfilter.h>
#include <gui/widgets/extendabletableview.h>

#include <optional>
#include <vector>

namespace Fooyin::Filters {
class FYGUI_EXPORT LibraryFilterModel : public ExtendableTableModel
{
    Q_OBJECT

public:
    explicit LibraryFilterModel(QObject* parent = nullptr);

    void setPresets(std::vector<LibraryFilter> presets);
    [[nodiscard]] const std::vector<LibraryFilter>& presets() const;
    [[nodiscard]] QString validationError() const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    void addPendingRow() override;
    void removePendingRow() override;
    void moveRowsUp(const QModelIndexList& indexes) override;
    void moveRowsDown(const QModelIndexList& indexes) override;
    bool removeRows(int row, int count, const QModelIndex& parent) override;

private:
    std::vector<LibraryFilter> m_presets;
    std::optional<int> m_pendingRow;
};
} // namespace Fooyin::Filters
