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

#pragma once

#include "fyutils_export.h"

#include <QModelIndexList>
#include <QString>
#include <QStringList>

#include <functional>

class QTreeView;

namespace Fooyin::Utils {
using IndexRangeList = std::vector<QModelIndexList>;

FYUTILS_EXPORT IndexRangeList getIndexRanges(const QModelIndexList& indexes);
FYUTILS_EXPORT bool sortModelIndexes(const QModelIndex& index1, const QModelIndex& index2);

FYUTILS_EXPORT void recursiveDataChanged(QAbstractItemModel* model, const QModelIndex& parent,
                                         const QList<int>& roles = {});

using ModelIndexKey = std::function<QString(const QModelIndex&)>;

FYUTILS_EXPORT QString modelIndexPath(const QModelIndex& index);
FYUTILS_EXPORT QStringList saveExpansionState(const QTreeView* view);
FYUTILS_EXPORT QStringList saveExpansionState(const QTreeView* view, const ModelIndexKey& keyForIndex);
FYUTILS_EXPORT QStringList updateExpansionState(const QTreeView* view, const QStringList& currentState);
FYUTILS_EXPORT QStringList updateExpansionState(const QTreeView* view, const QStringList& currentState,
                                                const ModelIndexKey& keyForIndex);
FYUTILS_EXPORT void restoreExpansionState(QTreeView* view, const QStringList& expandedIndexes);
FYUTILS_EXPORT void restoreExpansionState(QTreeView* view, const QStringList& expandedIndexes,
                                          const ModelIndexKey& keyForIndex);

FYUTILS_EXPORT QStringList saveCollapsedExpansionState(const QTreeView* view);
FYUTILS_EXPORT QStringList saveCollapsedExpansionState(const QTreeView* view, const ModelIndexKey& keyForIndex);
FYUTILS_EXPORT QStringList updateCollapsedExpansionState(const QTreeView* view, const QStringList& currentState);
FYUTILS_EXPORT QStringList updateCollapsedExpansionState(const QTreeView* view, const QStringList& currentState,
                                                         const ModelIndexKey& keyForIndex);
FYUTILS_EXPORT void restoreCollapsedExpansionState(QTreeView* view, const QStringList& collapsedIndexes);
FYUTILS_EXPORT void restoreCollapsedExpansionState(QTreeView* view, const QStringList& collapsedIndexes,
                                                   const ModelIndexKey& keyForIndex);
} // namespace Fooyin::Utils
