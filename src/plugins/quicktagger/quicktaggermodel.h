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

#include "quicktagger.h"

#include <gui/widgets/extendabletableview.h>

#include <optional>

namespace Fooyin::QuickTagger {
class QuickTaggerModel : public ExtendableTableModel
{
    Q_OBJECT

public:
    explicit QuickTaggerModel(QObject* parent = nullptr);

    void setTags(std::vector<QuickTag> tags);
    [[nodiscard]] std::vector<QuickTag> tags() const;
    [[nodiscard]] QString validationError() const;

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void addPendingRow() override;
    void removePendingRow() override;
    void moveRowsUp(const QModelIndexList& indexes) override;
    void moveRowsDown(const QModelIndexList& indexes) override;
    bool removeRows(int row, int count, const QModelIndex& parent) override;

private:
    [[nodiscard]] static bool isBlank(const QuickTag& tag);
    [[nodiscard]] static bool isComplete(const QuickTag& tag);

    std::vector<QuickTag> m_tags;
    std::optional<int> m_pendingRow;
};
} // namespace Fooyin::QuickTagger
