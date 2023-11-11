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

#include <utils/recursiveselectionmodel.h>

namespace {
void selectChildren(QAbstractItemModel* model, const QModelIndex& parentIndex, QItemSelection& selection)
{
    if(model->hasChildren(parentIndex)) {
        const int rowCount = model->rowCount(parentIndex);

        const QModelIndex firstChild = model->index(0, 0, parentIndex);
        const QModelIndex lastChild  = model->index(rowCount - 1, 0, parentIndex);
        selection.append({firstChild, lastChild});

        for(int row{0}; row < rowCount; ++row) {
            selectChildren(model, model->index(row, 0, parentIndex), selection);
        }
    }
}
} // namespace

namespace Fy::Utils {
RecursiveSelectionModel::RecursiveSelectionModel(QAbstractItemModel* model, QObject* parent)
    : QItemSelectionModel{model, parent}
{ }

void RecursiveSelectionModel::select(const QItemSelection& selection, SelectionFlags command)
{
    QItemSelection newSelection;
    newSelection.reserve(selection.size());

    QAbstractItemModel* model = this->model();

    for(const auto& range : selection) {
        for(int row = range.top(); row <= range.bottom(); ++row) {
            const QModelIndex index = model->index(row, 0, range.parent());
            if(model->hasChildren(index)) {
                selectChildren(model, index, newSelection);
            }
        }
        newSelection.append(range);
    }

    QItemSelectionModel::select(newSelection, command);
}
} // namespace Fy::Utils
