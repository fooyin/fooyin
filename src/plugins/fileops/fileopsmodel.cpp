/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "fileopsmodel.h"

#include <core/library/musiclibrary.h>
#include <utils/enum.h>

namespace Fooyin::FileOps {
FileOpsModel::FileOpsModel(MusicLibrary* library, TrackList tracks, SettingsManager* settings, QObject* parent)
    : QAbstractItemModel{parent}
    , m_worker{library, std::move(tracks), settings}
{
    m_worker.moveToThread(&m_workerThread);

    QObject::connect(&m_worker, &FileOpsWorker::simulated, this, &FileOpsModel::populate);
    QObject::connect(&m_worker, &FileOpsWorker::operationFinished, this, &FileOpsModel::operationFinished);

    m_workerThread.start();
}

FileOpsModel::~FileOpsModel()
{
    m_worker.stopThread();
    m_workerThread.quit();
    m_workerThread.wait();
}

void FileOpsModel::simulate(const FileOpPreset& preset)
{
    m_worker.stopThread();

    QMetaObject::invokeMethod(&m_worker, [this, preset]() { m_worker.simulate(preset); });
}

void FileOpsModel::run()
{
    m_worker.stopThread();

    QMetaObject::invokeMethod(&m_worker, &FileOpsWorker::run);
}

void FileOpsModel::stop()
{
    m_worker.stopThread();
}

Qt::ItemFlags FileOpsModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = QAbstractItemModel::flags(index);
    flags |= Qt::ItemNeverHasChildren;

    return flags;
}

QVariant FileOpsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return tr("Operation");
        case(1):
            return tr("Source");
        case(2):
            return tr("Destination");
        default:
            break;
    }

    return {};
}

QVariant FileOpsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    const auto& item = m_operations.at(index.row());

    switch(index.column()) {
        case(0):
            return operationToString(item.op);
        case(1):
            return item.displayName();
        case(2):
            return item.displayDestination();
        default:
            break;
    }

    return {};
}

QModelIndex FileOpsModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    return createIndex(row, column);
}

QModelIndex FileOpsModel::parent(const QModelIndex& /*child*/) const
{
    return {};
}

int FileOpsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

int FileOpsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(m_operations.size());
}

void FileOpsModel::populate(const FileOperations& operations)
{
    beginResetModel();
    m_operations = operations;
    endResetModel();

    emit simulated();
}

void FileOpsModel::operationFinished(const FileOpsItem& /*operation*/)
{
    beginRemoveRows({}, 0, 0);
    m_operations.pop_front();
    endRemoveRows();
}

QString FileOpsModel::operationToString(Operation op) const
{
    switch(op) {
        case(Operation::Copy):
            return tr("Copy");
        case(Operation::Move):
            return tr("Move");
        case(Operation::Rename):
            return tr("Rename");
        case(Operation::Create):
            return tr("Create");
        case(Operation::Remove):
            return tr("Remove");
    }
    return tr("Unknown");
}
} // namespace Fooyin::FileOps
