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

#include "fileopsmodel.h"

#include <core/library/musiclibrary.h>
#include <utils/enum.h>

#include <QColor>

namespace Fooyin::FileOps {
FileOpsModel::FileOpsModel(MusicLibrary* library, std::shared_ptr<AudioLoader> audioLoader, TrackList tracks,
                           SettingsManager* settings, QObject* parent)
    : QAbstractItemModel{parent}
    , m_worker{library, std::move(audioLoader), std::move(tracks), settings}
    , m_succeededCount{0}
{
    m_worker.moveToThread(&m_workerThread);

    QObject::connect(&m_worker, &FileOpsWorker::simulated, this, &FileOpsModel::populate);
    QObject::connect(&m_worker, &FileOpsWorker::operationCompleted, this, &FileOpsModel::operationCompleted);
    QObject::connect(&m_worker, &Worker::finished, this, &FileOpsModel::workerFinished);

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

int FileOpsModel::pendingCount() const
{
    return static_cast<int>(m_operations.size());
}

int FileOpsModel::succeededCount() const
{
    return m_succeededCount;
}

int FileOpsModel::failedCount() const
{
    return static_cast<int>(std::ranges::count(m_results, FileOpStatus::Failed, &FileOpResult::status));
}

int FileOpsModel::skippedCount() const
{
    return static_cast<int>(std::ranges::count(m_results, FileOpStatus::Skipped, &FileOpResult::status));
}

int FileOpsModel::cancelledCount() const
{
    return static_cast<int>(std::ranges::count(m_results, FileOpStatus::Cancelled, &FileOpResult::status));
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
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Operation");
        case 1:
            return tr("Source");
        case 2:
            return tr("Destination");
        case 3:
            return tr("Result");
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

    const int pending   = pendingCount();
    const bool isResult = index.row() >= pending;
    const auto& item    = isResult ? m_results.at(index.row() - pending).operation : m_operations.at(index.row());

    if(isResult) {
        const auto& result = m_results.at(index.row() - pending);

        if(role == Qt::ToolTipRole) {
            return result.error;
        }
        if(role == Qt::ForegroundRole && result.status == FileOpStatus::Failed) {
            return QColor{Qt::red};
        }
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(index.column()) {
        case 0:
            return operationToString(item.op);
        case 1:
            return item.displayName();
        case 2:
            return item.displayDestination();
        case 3:
            return isResult ? resultToString(m_results.at(index.row() - pending)) : tr("Pending");
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
    return 4;
}

int FileOpsModel::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(m_operations.size() + m_results.size());
}

void FileOpsModel::populate(const FileOperations& operations)
{
    beginResetModel();
    m_operations = operations;
    m_results.clear();
    m_succeededCount = 0;
    endResetModel();

    Q_EMIT simulated();
}

void FileOpsModel::operationCompleted(const FileOpResult& result)
{
    if(m_operations.empty()) {
        return;
    }

    beginRemoveRows({}, 0, 0);
    m_operations.pop_front();
    endRemoveRows();

    if(result.status == FileOpStatus::Succeeded) {
        ++m_succeededCount;
        return;
    }

    const int resultRow = rowCount({});
    beginInsertRows({}, resultRow, resultRow);
    m_results.push_back(result);
    endInsertRows();
}

void FileOpsModel::workerFinished()
{
    Q_EMIT finished();
}

QString FileOpsModel::operationToString(Operation op) const
{
    switch(op) {
        case Operation::Copy:
            return tr("Copy");
        case Operation::Move:
            return tr("Move");
        case Operation::Rename:
            return tr("Rename");
        case Operation::Create:
            return tr("Create");
        case Operation::Remove:
            return tr("Remove");
        case Operation::Delete:
            return tr("Delete");
        case Operation::Extract:
            return tr("Extract");
        case Operation::RemoveArchive:
            return tr("Remove");
    }
    return tr("Unknown");
}

QString FileOpsModel::resultToString(const FileOpResult& result)
{
    switch(result.status) {
        case FileOpStatus::Succeeded:
            return tr("Succeeded");
        case FileOpStatus::Failed:
            return tr("Failed: %1").arg(result.error);
        case FileOpStatus::Skipped:
            return tr("Skipped: %1").arg(result.error);
        case FileOpStatus::Cancelled:
            return result.error.isEmpty() ? tr("Cancelled") : tr("Cancelled: %1").arg(result.error);
    }
    return {};
}
} // namespace Fooyin::FileOps
