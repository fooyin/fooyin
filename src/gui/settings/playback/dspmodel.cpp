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

#include "dspmodel.h"

#include <QIODevice>
#include <QMimeData>

#include <algorithm>
#include <ranges>
#include <utility>

using namespace Qt::StringLiterals;

constexpr auto DspItems  = "application/x-fooyin-dspitems";
constexpr auto DspRows   = "application/x-fooyin-dsprows";
constexpr auto DspSource = "application/x-fooyin-dspsource";

namespace Fooyin {
DspModel::DspModel(QObject* parent)
    : QAbstractTableModel{parent}
    , m_id{UId::create()}
    , m_allowInternalMoves{true}
{ }

void DspModel::setAllowInternalMoves(bool allow)
{
    m_allowInternalMoves = allow;
}

void DspModel::setup(const Engine::DspChain& dsps)
{
    beginResetModel();
    m_dsps = dsps;
    endResetModel();
}

Engine::DspChain DspModel::dsps() const
{
    return m_dsps;
}

Qt::ItemFlags DspModel::flags(const QModelIndex& index) const
{
    auto flags = QAbstractTableModel::flags(index);

    if(index.isValid()) {
        flags |= Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable;
    }
    else {
        flags |= Qt::ItemIsDropEnabled;
    }

    return flags;
}

int DspModel::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(m_dsps.size());
}

int DspModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant DspModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    if(index.row() < 0 || index.row() >= static_cast<int>(m_dsps.size())) {
        return {};
    }

    const auto& dsp = m_dsps.at(index.row());

    switch(role) {
        case(Qt::DisplayRole):
            return dsp.name;
        case(Role::Id):
            return dsp.id;
        case(Role::HasSettings):
            return dsp.hasSettings;
        case(Role::Dsp):
            return QVariant::fromValue(dsp);
        default:
            return {};
    }
}

QStringList DspModel::mimeTypes() const
{
    return {QString::fromLatin1(DspItems), QString::fromLatin1(DspRows), QString::fromLatin1(DspSource)};
}

QMimeData* DspModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();

    if(const auto dsp = dspForIndex(indexes.constFirst())) {
        QByteArray data;
        QDataStream stream{&data, QIODevice::WriteOnly};
        stream << dsp.value();

        mimeData->setData(QString::fromLatin1(DspItems), data);
    }

    QList<int> selectedRows;
    for(const QModelIndex& index : indexes) {
        selectedRows.append(index.row());
    }

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream << selectedRows;

    mimeData->setData(QString::fromLatin1(DspRows), data);

    QByteArray sourceData;
    QDataStream sourceStream{&sourceData, QIODevice::WriteOnly};
    sourceStream << m_id;
    mimeData->setData(QString::fromLatin1(DspSource), sourceData);

    return mimeData;
}

bool DspModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                               const QModelIndex& parent) const
{
    if(action != Qt::MoveAction && action != Qt::CopyAction) {
        return QAbstractItemModel::canDropMimeData(data, action, row, column, parent);
    }

    const bool internal = isInternalDrag(data);

    if(internal && (!m_allowInternalMoves || action != Qt::MoveAction)) {
        return false;
    }

    if(internal && data->hasFormat(QString::fromLatin1(DspRows))) {
        return true;
    }

    if(!internal && data->hasFormat(QString::fromLatin1(DspItems))) {
        return true;
    }

    return false;
}

bool DspModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                            const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    if(row < 0) {
        row = static_cast<int>(m_dsps.size());
    }

    const bool internal = isInternalDrag(data);

    if(!internal && data->hasFormat(QString::fromLatin1(DspItems))) {
        QByteArray dspItems = data->data(QString::fromLatin1(DspItems));
        QDataStream stream{&dspItems, QIODevice::ReadOnly};

        Engine::DspDefinition dsp;
        stream >> dsp;

        beginInsertRows({}, row, row);
        m_dsps.insert(m_dsps.begin() + row, dsp);
        endInsertRows();

        return true;
    }

    if(internal && data->hasFormat(QString::fromLatin1(DspRows))) {
        QByteArray dspData = data->data(QString::fromLatin1(DspRows));
        QDataStream stream{&dspData, QIODevice::ReadOnly};

        QList<int> selectedRows;
        stream >> selectedRows;
        if(selectedRows.isEmpty()) {
            return false;
        }

        std::ranges::sort(selectedRows);
        selectedRows.erase(std::unique(selectedRows.begin(), selectedRows.end()), selectedRows.end());
        if(selectedRows.constFirst() < 0 || std::cmp_greater_equal(selectedRows.constLast(), m_dsps.size())) {
            return false;
        }

        beginResetModel();

        Engine::DspChain selectedDsps;
        selectedDsps.reserve(static_cast<size_t>(selectedRows.size()));
        for(const int selectedRow : std::as_const(selectedRows)) {
            selectedDsps.emplace_back(m_dsps.at(selectedRow));
        }

        std::ranges::for_each(selectedRows | std::views::reverse,
                              [this](int dspRow) { m_dsps.erase(m_dsps.begin() + dspRow); });

        const int rowsBeforeTarget = static_cast<int>(
            std::ranges::count_if(selectedRows, [row](const int selectedRow) { return selectedRow < row; }));
        const int insertRow = std::clamp(row - rowsBeforeTarget, 0, static_cast<int>(m_dsps.size()));
        m_dsps.insert(m_dsps.begin() + insertRow, selectedDsps.begin(), selectedDsps.end());

        endResetModel();

        return true;
    }

    return false;
}

bool DspModel::isInternalDrag(const QMimeData* data) const
{
    if(!data->hasFormat(QString::fromLatin1(DspSource))) {
        return false;
    }

    QByteArray sourceData = data->data(QString::fromLatin1(DspSource));
    QDataStream stream{&sourceData, QIODevice::ReadOnly};

    QUuid sourceId;
    stream >> sourceId;

    return sourceId == m_id;
}

std::optional<Engine::DspDefinition> DspModel::dspForIndex(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return {};
    }

    const int row = index.row();

    if(row < 0 || std::cmp_greater_equal(row, m_dsps.size())) {
        return {};
    }

    return m_dsps.at(row);
}

bool DspModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if(row < 0 || row + count > static_cast<int>(m_dsps.size())) {
        return false;
    }

    beginRemoveRows(parent, row, row + count - 1);
    m_dsps.erase(m_dsps.begin() + row, m_dsps.begin() + row + count);
    endRemoveRows();

    return true;
}

Qt::DropActions DspModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions DspModel::supportedDragActions() const
{
    return m_allowInternalMoves ? Qt::MoveAction : Qt::CopyAction;
}
} // namespace Fooyin
