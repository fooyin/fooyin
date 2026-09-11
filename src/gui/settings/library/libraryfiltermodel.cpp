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

#include "libraryfiltermodel.h"

#include <core/scripting/scriptparser.h>

#include <QApplication>
#include <QPalette>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
LibraryFilterModel::LibraryFilterModel(QObject* parent)
    : ExtendableTableModel{parent}
{ }

void LibraryFilterModel::setPresets(std::vector<LibraryFilter> presets)
{
    beginResetModel();
    m_presets    = std::move(presets);
    m_pendingRow = {};
    endResetModel();
}

const std::vector<LibraryFilter>& LibraryFilterModel::presets() const
{
    return m_presets;
}

QString LibraryFilterModel::validationError() const
{
    ScriptParser parser;
    for(const LibraryFilter& preset : m_presets) {
        if(preset.name.trimmed().isEmpty()) {
            return tr("Every library filter needs a name.");
        }

        const QString expression = preset.expression.trimmed();
        if(expression.isEmpty() || !ScriptParser::canEvaluateAsQuery(parser.parseQuery(expression))) {
            return tr("The expression for library filter \"%1\" is invalid.").arg(preset.name);
        }
    }
    return {};
}

int LibraryFilterModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_presets.size());
}

int LibraryFilterModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

QVariant LibraryFilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Enabled");
        case 1:
            return tr("Name");
        case 2:
            return tr("Query");
        default:
            return {};
    }
}

QVariant LibraryFilterModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    if(role == Qt::TextAlignmentRole && index.column() == 0) {
        return Qt::AlignCenter;
    }

    const LibraryFilter& preset = m_presets.at(index.row());

    if(role == Qt::CheckStateRole && index.column() == 0) {
        return preset.enabled ? Qt::Checked : Qt::Unchecked;
    }

    if(role == Qt::ForegroundRole
       && ((index.column() == 1 && preset.name.isEmpty()) || (index.column() == 2 && preset.expression.isEmpty()))) {
        return QApplication::palette().color(QPalette::PlaceholderText);
    }

    if(role == Qt::EditRole) {
        if(index.column() == 1) {
            return preset.name;
        }
        if(index.column() == 2) {
            return preset.expression;
        }
        return {};
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(index.column()) {
        case 0:
            return {};
        case 1:
            return preset.name.isEmpty() ? tr("Enter name") : preset.name;
        case 2:
            return preset.expression.isEmpty() ? tr("Enter expression") : preset.expression;
        default:
            return {};
    }
}

bool LibraryFilterModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return false;
    }

    LibraryFilter& preset = m_presets.at(index.row());

    if(role == Qt::CheckStateRole && index.column() == 0) {
        const bool enabled = value.toInt() == Qt::Checked;
        if(preset.enabled == enabled) {
            return false;
        }
        preset.enabled = enabled;
        Q_EMIT dataChanged(index, index, {Qt::CheckStateRole});
        return true;
    }

    if(role != Qt::EditRole) {
        return false;
    }

    switch(index.column()) {
        case 0:
            return false;
        case 1: {
            const QString text = value.toString();
            if(preset.name == text) {
                if(m_pendingRow == index.row()) {
                    Q_EMIT pendingRowCancelled();
                }
                return false;
            }
            preset.name = text;
            break;
        }
        case 2: {
            const QString text = value.toString();
            if(preset.expression == text) {
                return false;
            }
            preset.expression = text;
            break;
        }
        default:
            return false;
    }

    if(m_pendingRow == index.row()) {
        m_pendingRow.reset();
    }

    Q_EMIT dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

Qt::ItemFlags LibraryFilterModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = ExtendableTableModel::flags(index);
    if(!index.isValid()) {
        return defaultFlags;
    }

    if(index.column() == 0) {
        defaultFlags |= Qt::ItemIsUserCheckable;
    }
    else {
        defaultFlags |= Qt::ItemIsEditable;
    }

    return defaultFlags;
}

void LibraryFilterModel::addPendingRow()
{
    const int row = rowCount({});
    beginInsertRows({}, row, row);

    LibraryFilter preset;
    preset.index = row;
    m_presets.push_back(std::move(preset));
    m_pendingRow = row;

    endInsertRows();
}

void LibraryFilterModel::removePendingRow()
{
    if(m_pendingRow) {
        removeRow(*m_pendingRow);
    }
}

void LibraryFilterModel::moveRowsUp(const QModelIndexList& indexes)
{
    if(indexes.empty()) {
        return;
    }

    const int row = indexes.front().row();
    if(row <= 0 || row >= rowCount({})) {
        return;
    }

    beginMoveRows({}, row, row, {}, row - 1);
    std::swap(m_presets.at(row), m_presets.at(row - 1));
    endMoveRows();
}

void LibraryFilterModel::moveRowsDown(const QModelIndexList& indexes)
{
    if(indexes.empty()) {
        return;
    }

    const int row = indexes.back().row();
    if(row < 0 || row >= rowCount({}) - 1) {
        return;
    }

    beginMoveRows({}, row, row, {}, row + 2);
    std::swap(m_presets.at(row), m_presets.at(row + 1));
    endMoveRows();
}

bool LibraryFilterModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if(parent.isValid() || row < 0 || count <= 0 || row + count > rowCount({})) {
        return false;
    }

    beginRemoveRows(parent, row, row + count - 1);
    m_presets.erase(m_presets.begin() + row, m_presets.begin() + row + count);
    endRemoveRows();

    if(m_pendingRow && *m_pendingRow >= row && *m_pendingRow < row + count) {
        m_pendingRow.reset();
    }
    else if(m_pendingRow && *m_pendingRow >= row + count) {
        m_pendingRow = *m_pendingRow - count;
    }

    return true;
}
} // namespace Fooyin::Filters

#include "moc_libraryfiltermodel.cpp"
