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

#include "quicktaggermodel.h"

#include <utils/id.h>

#include <QApplication>

using namespace Qt::StringLiterals;

namespace Fooyin::QuickTagger {
namespace {
QString normaliseField(const QString& field)
{
    return field.trimmed();
}

QStringList splitQuickTagValues(const QString& values)
{
    QStringList result;

    for(QString value : values.split(';'_L1, Qt::SkipEmptyParts)) {
        value = value.trimmed();
        if(!value.isEmpty()) {
            result.push_back(value);
        }
    }
    result.removeDuplicates();

    return result;
}

QString generateId()
{
    return UId::create().toString(QUuid::WithoutBraces);
}

QString fieldPlaceholder()
{
    return QuickTaggerModel::tr("Enter field");
}

QString valuesPlaceholder()
{
    return QuickTaggerModel::tr("Enter values");
}

QStringList tagValueTexts(const std::vector<QuickTagValue>& values)
{
    QStringList result;
    result.reserve(values.size());

    for(const auto& value : values) {
        result.emplace_back(value.value);
    }

    return result;
}
} // namespace

QuickTaggerModel::QuickTaggerModel(QObject* parent)
    : ExtendableTableModel{parent}
{ }

void QuickTaggerModel::setTags(std::vector<QuickTag> tags)
{
    for(auto& tag : tags) {
        if(!isBlank(tag) && tag.id.isEmpty()) {
            tag.id = generateId();
        }
        for(auto& value : tag.values) {
            if(value.id.isEmpty()) {
                value.id = generateId();
            }
        }
    }

    beginResetModel();
    m_pendingRow.reset();
    m_tags = std::move(tags);
    endResetModel();
}

std::vector<QuickTag> QuickTaggerModel::tags() const
{
    std::vector<QuickTag> tags;

    for(const auto& tag : m_tags) {
        if(isComplete(tag)) {
            auto savedTag{tag};
            savedTag.name = savedTag.name.trimmed();
            if(savedTag.name.isEmpty()) {
                savedTag.name = savedTag.field;
            }
            tags.emplace_back(savedTag);
        }
    }

    return tags;
}

QString QuickTaggerModel::validationError() const
{
    for(size_t i{0}; i < m_tags.size(); ++i) {
        const auto& tag = m_tags.at(i);
        if(isBlank(tag)) {
            continue;
        }
        if(tag.field.isEmpty()) {
            return tr("Row %1: Field is required.").arg(i + 1);
        }
        if(tag.values.empty()) {
            return tr("Row %1: At least one value is required.").arg(i + 1);
        }
    }
    return {};
}

int QuickTaggerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_tags.size());
}

int QuickTaggerModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant QuickTaggerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Vertical) {
        return {};
    }

    if(role == Qt::ToolTipRole) {
        switch(section) {
            case 0:
                return tr("Display name shown in menus and shortcut actions.");
            case 1:
                return tr("Metadata field to write.");
            case 2:
                return tr("Literal values separated by semicolons.");
            default:
                return {};
        }
    }

    if(role != Qt::DisplayRole) {
        return {};
    }
    switch(section) {
        case 0:
            return tr("Name");
        case 1:
            return tr("Field");
        case 2:
            return tr("Values");
        default:
            return {};
    }
}

QVariant QuickTaggerModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || index.row() >= rowCount({})) {
        return {};
    }

    const auto& tag = m_tags.at(static_cast<size_t>(index.row()));

    if(role == Qt::ForegroundRole) {
        if((index.column() == 1 && tag.field.isEmpty()) || (index.column() == 2 && tag.values.empty())) {
            return QApplication::palette().color(QPalette::PlaceholderText);
        }
    }

    if(role == Qt::EditRole) {
        switch(index.column()) {
            case 0:
                return tag.name;
            case 1:
                return tag.field;
            case 2:
                return tagValueTexts(tag.values).join(';'_L1);
            default:
                break;
        }
    }

    if(role == Qt::DisplayRole) {
        switch(index.column()) {
            case 0:
                return tag.name;
            case 1:
                return !tag.field.isEmpty() ? tag.field : fieldPlaceholder();
            case 2: {
                const QString values = tagValueTexts(tag.values).join(';'_L1);
                return !values.isEmpty() ? values : valuesPlaceholder();
            }
            default:
                break;
        }
    }

    return {};
}

bool QuickTaggerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole || !index.isValid() || index.row() >= rowCount({})) {
        return false;
    }

    auto& tag = m_tags.at(static_cast<size_t>(index.row()));

    if(index.column() == 0) {
        tag.name = value.toString().trimmed();
    }
    else if(index.column() == 1) {
        const QString field = normaliseField(value.toString());
        if(field.isEmpty() || field == fieldPlaceholder()) {
            if(m_pendingRow == index.row()) {
                Q_EMIT pendingRowCancelled();
            }
            return false;
        }
        if(tag.field == field) {
            return false;
        }
        tag.field = field;
        if(m_pendingRow == index.row()) {
            m_pendingRow.reset();
        }
    }
    else {
        const auto existingValues{tag.values};
        const QString valuesText = value.toString();
        if(valuesText == valuesPlaceholder()) {
            return false;
        }

        QStringList values = splitQuickTagValues(valuesText);
        if(tagValueTexts(tag.values) == values) {
            return false;
        }

        tag.values.clear();
        tag.values.reserve(values.size());

        for(const QString& text : values) {
            const auto existingIt = std::ranges::find(existingValues, text, &QuickTagValue::value);
            tag.values.emplace_back(existingIt == existingValues.cend() ? generateId() : existingIt->id, text);
        }
    }

    Q_EMIT dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole, Qt::ForegroundRole});
    return true;
}

Qt::ItemFlags QuickTaggerModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return ExtendableTableModel::flags(index) | Qt::ItemIsEditable;
}

void QuickTaggerModel::addPendingRow()
{
    const int row = rowCount({});
    beginInsertRows({}, row, row);
    QuickTag tag;
    tag.id = generateId();
    m_tags.push_back(std::move(tag));
    m_pendingRow = row;
    endInsertRows();
}

void QuickTaggerModel::removePendingRow()
{
    if(!m_pendingRow || *m_pendingRow < 0 || *m_pendingRow >= rowCount({})) {
        return;
    }

    removeRow(*m_pendingRow);
    m_pendingRow.reset();
}

void QuickTaggerModel::moveRowsUp(const QModelIndexList& indexes)
{
    if(indexes.empty()) {
        return;
    }

    const int row = indexes.front().row();
    if(row <= 0 || row >= rowCount({})) {
        return;
    }

    beginMoveRows({}, row, row, {}, row - 1);
    std::swap(m_tags.at(static_cast<size_t>(row)), m_tags.at(static_cast<size_t>(row - 1)));
    if(m_pendingRow == row) {
        m_pendingRow = row - 1;
    }
    else if(m_pendingRow == row - 1) {
        m_pendingRow = row;
    }
    endMoveRows();
}

void QuickTaggerModel::moveRowsDown(const QModelIndexList& indexes)
{
    if(indexes.empty()) {
        return;
    }

    const int row = indexes.back().row();
    if(row < 0 || row >= rowCount({}) - 1) {
        return;
    }

    beginMoveRows({}, row, row, {}, row + 2);
    std::swap(m_tags.at(static_cast<size_t>(row)), m_tags.at(static_cast<size_t>(row + 1)));
    if(m_pendingRow == row) {
        m_pendingRow = row + 1;
    }
    else if(m_pendingRow == row + 1) {
        m_pendingRow = row;
    }
    endMoveRows();
}

bool QuickTaggerModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if(parent.isValid() || row < 0 || count <= 0 || row + count > rowCount({})) {
        return false;
    }

    beginRemoveRows(parent, row, row + count - 1);
    m_tags.erase(m_tags.begin() + row, m_tags.begin() + row + count);
    endRemoveRows();

    if(m_pendingRow && *m_pendingRow >= row && *m_pendingRow < row + count) {
        m_pendingRow.reset();
    }
    else if(m_pendingRow && *m_pendingRow >= row + count) {
        m_pendingRow = *m_pendingRow - count;
    }

    return true;
}

bool QuickTaggerModel::isBlank(const QuickTag& tag)
{
    return tag.name.isEmpty() && tag.field.isEmpty() && tag.values.empty();
}

bool QuickTaggerModel::isComplete(const QuickTag& tag)
{
    return !tag.field.isEmpty() && !tag.values.empty();
}
} // namespace Fooyin::QuickTagger
