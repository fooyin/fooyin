/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <utils/modelutils.h>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QSet>
#include <QTreeView>

using namespace Qt::StringLiterals;

namespace Fooyin::Utils {
namespace {
void saveTreeViewExpansionState(const QTreeView* view, const ModelIndexKey& keyForIndex, QStringList* expandedIndexes,
                                const QModelIndex& parent = {})
{
    const auto* model = view->model();
    if(!model) {
        return;
    }

    const int rows = model->rowCount(parent);
    for(int row{0}; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(view->isExpanded(index)) {
            const QString key = keyForIndex(index);
            if(!key.isEmpty()) {
                expandedIndexes->append(key);
            }
        }
        saveTreeViewExpansionState(view, keyForIndex, expandedIndexes, index);
    }
}

void updateTreeViewExpansionState(const QTreeView* view, const ModelIndexKey& keyForIndex, QStringList* expandedIndexes,
                                  const QModelIndex& parent = {})
{
    const auto* model = view->model();
    if(!model) {
        return;
    }

    const int rows = model->rowCount(parent);
    for(int row{0}; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        const QString key       = keyForIndex(index);
        if(!key.isEmpty()) {
            expandedIndexes->removeOne(key);
            if(view->isExpanded(index)) {
                expandedIndexes->append(key);
            }
        }
        updateTreeViewExpansionState(view, keyForIndex, expandedIndexes, index);
    }
}

void restoreTreeViewExpansionState(QTreeView* view, const QStringList& expandedIndexes,
                                   const ModelIndexKey& keyForIndex, const QModelIndex& parent = {})
{
    const auto* model = view->model();
    if(!model) {
        return;
    }

    const int rows = model->rowCount(parent);
    for(int row{0}; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(expandedIndexes.contains(keyForIndex(index))) {
            view->setExpanded(index, true);
        }
        restoreTreeViewExpansionState(view, expandedIndexes, keyForIndex, index);
    }
}

void updateTreeViewCollapsedExpansionState(const QTreeView* view, const ModelIndexKey& keyForIndex,
                                           QStringList* collapsedIndexes, const QModelIndex& parent = {})
{
    const auto* model = view->model();
    if(!model) {
        return;
    }

    const int rows = model->rowCount(parent);
    for(int row{0}; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(!model->hasChildren(index)) {
            continue;
        }

        const QString key = keyForIndex(index);
        if(!key.isEmpty()) {
            collapsedIndexes->removeOne(key);
            if(!view->isExpanded(index)) {
                collapsedIndexes->append(key);
                continue;
            }
        }

        updateTreeViewCollapsedExpansionState(view, keyForIndex, collapsedIndexes, index);
    }
}

void restoreTreeViewCollapsedExpansionState(QTreeView* view, const QStringList& collapsedIndexes,
                                            const ModelIndexKey& keyForIndex, const QModelIndex& parent = {})
{
    const auto* model = view->model();
    if(!model) {
        return;
    }

    const int rows = model->rowCount(parent);
    for(int row{0}; row < rows; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(collapsedIndexes.contains(keyForIndex(index))) {
            view->setExpanded(index, false);
        }
        restoreTreeViewCollapsedExpansionState(view, collapsedIndexes, keyForIndex, index);
    }
}
} // namespace

IndexRangeList getIndexRanges(const QModelIndexList& indexes)
{
    IndexRangeList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, sortModelIndexes);

    auto startOfSequence = sortedIndexes.cbegin();
    while(startOfSequence != sortedIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, sortedIndexes.cend(), [](const auto& lhs, const auto& rhs) {
                  return lhs.parent() != rhs.parent() || rhs.row() != lhs.row() + 1;
              });
        if(endOfSequence != sortedIndexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        indexGroups.emplace_back(startOfSequence, endOfSequence);

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}

bool sortModelIndexes(const QModelIndex& index1, const QModelIndex& index2)
{
    QModelIndex item1{index1};
    QModelIndex item2{index2};

    QModelIndexList item1Parents;
    QModelIndexList item2Parents;
    const QModelIndex root;

    while(item1.parent() != item2.parent()) {
        if(item1.parent() != root) {
            item1Parents.push_back(item1);
            item1 = item1.parent();
        }
        if(item2.parent() != root) {
            item2Parents.push_back(item2);
            item2 = item2.parent();
        }
    }
    if(item1.row() == item2.row()) {
        return item1Parents.size() < item2Parents.size();
    }
    return item1.row() < item2.row();
}

void recursiveDataChanged(QAbstractItemModel* model, const QModelIndex& parent, const QList<int>& roles)
{
    const int rowCount    = model->rowCount(parent);
    const int columnCount = model->columnCount(parent);

    if(rowCount > 0 && columnCount > 0) {
        const QModelIndex topLeft     = model->index(0, 0, parent);
        const QModelIndex bottomRight = model->index(rowCount - 1, columnCount - 1, parent);
        Q_EMIT model->dataChanged(topLeft, bottomRight, roles);
    }

    for(int row{0}; row < rowCount; ++row) {
        const QModelIndex index = model->index(row, 0, parent);
        if(model->hasChildren(index)) {
            recursiveDataChanged(model, index, roles);
        }
    }
}

QString modelIndexPath(const QModelIndex& index)
{
    if(!index.isValid()) {
        return {};
    }

    QStringList pathParts;
    for(QModelIndex current = index; current.isValid(); current = current.parent()) {
        pathParts.prepend(QString::number(current.row()));
    }

    return pathParts.join('/'_L1);
}

QStringList saveExpansionState(const QTreeView* view)
{
    return saveExpansionState(view, modelIndexPath);
}

QStringList saveExpansionState(const QTreeView* view, const ModelIndexKey& keyForIndex)
{
    if(!view) {
        return {};
    }

    QStringList expandedIndexes;
    saveTreeViewExpansionState(view, keyForIndex, &expandedIndexes);
    expandedIndexes.removeDuplicates();
    return expandedIndexes;
}

QStringList updateExpansionState(const QTreeView* view, const QStringList& currentState)
{
    return updateExpansionState(view, currentState, modelIndexPath);
}

QStringList updateExpansionState(const QTreeView* view, const QStringList& currentState,
                                 const ModelIndexKey& keyForIndex)
{
    if(!view) {
        return currentState;
    }

    QStringList updatedState{currentState};
    updateTreeViewExpansionState(view, keyForIndex, &updatedState);
    updatedState.sort();
    return updatedState;
}

void restoreExpansionState(QTreeView* view, const QStringList& expandedIndexes)
{
    restoreTreeViewExpansionState(view, expandedIndexes, modelIndexPath);
}

void restoreExpansionState(QTreeView* view, const QStringList& expandedIndexes, const ModelIndexKey& keyForIndex)
{
    if(!view) {
        return;
    }

    const bool updatesEnabled = view->updatesEnabled();
    view->setUpdatesEnabled(false);
    view->collapseAll();
    restoreTreeViewExpansionState(view, expandedIndexes, keyForIndex);
    view->setUpdatesEnabled(updatesEnabled);
}

QStringList saveCollapsedExpansionState(const QTreeView* view)
{
    return saveCollapsedExpansionState(view, modelIndexPath);
}

QStringList saveCollapsedExpansionState(const QTreeView* view, const ModelIndexKey& keyForIndex)
{
    return updateCollapsedExpansionState(view, {}, keyForIndex);
}

QStringList updateCollapsedExpansionState(const QTreeView* view, const QStringList& currentState)
{
    return updateCollapsedExpansionState(view, currentState, modelIndexPath);
}

QStringList updateCollapsedExpansionState(const QTreeView* view, const QStringList& currentState,
                                          const ModelIndexKey& keyForIndex)
{
    if(!view) {
        return currentState;
    }

    QStringList updatedState{currentState};

    updateTreeViewCollapsedExpansionState(view, keyForIndex, &updatedState);
    updatedState.removeDuplicates();
    updatedState.sort();

    return updatedState;
}

void restoreCollapsedExpansionState(QTreeView* view, const QStringList& collapsedIndexes)
{
    restoreCollapsedExpansionState(view, collapsedIndexes, modelIndexPath);
}

void restoreCollapsedExpansionState(QTreeView* view, const QStringList& collapsedIndexes,
                                    const ModelIndexKey& keyForIndex)
{
    if(!view) {
        return;
    }

    const bool updatesEnabled = view->updatesEnabled();
    view->setUpdatesEnabled(false);
    view->expandAll();
    restoreTreeViewCollapsedExpansionState(view, collapsedIndexes, keyForIndex);
    view->setUpdatesEnabled(updatesEnabled);
}
} // namespace Fooyin::Utils
