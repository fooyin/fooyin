/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "librarymodel.h"

#include <core/library/librarymanager.h>
#include <utils/enum.h>

#include <QFont>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(LIB_MODEL, "LibraryModel")

namespace Fooyin {
LibraryItem::LibraryItem()
    : LibraryItem{{}, nullptr}
{ }

LibraryItem::LibraryItem(LibraryInfo info, LibraryItem* parent)
    : TreeStatusItem{parent}
    , m_info{std::move(info)}
{ }

LibraryInfo LibraryItem::info() const
{
    return m_info;
}

void LibraryItem::changeInfo(const LibraryInfo& info)
{
    m_info = info;
}

LibraryModel::LibraryModel(LibraryManager* libraryManager, QObject* parent)
    : ExtendableTableModel{parent}
    , m_libraryManager{libraryManager}
{
    QObject::connect(m_libraryManager, &LibraryManager::libraryStatusChanged, this, [this](const LibraryInfo& info) {
        if(m_nodes.contains(info.path)) {
            m_nodes.at(info.path).changeInfo(info);
            emit dataChanged({}, {}, {Qt::DisplayRole});
        }
    });
}

void LibraryModel::populate()
{
    beginResetModel();
    m_root = {};
    m_nodes.clear();
    m_librariesToAdd.clear();

    const LibraryInfoMap& libraries = m_libraryManager->allLibraries();

    for(const auto& [id, library] : libraries) {
        if(id < 0) {
            continue;
        }
        const QString key = library.path;

        if(!m_nodes.contains(key)) {
            m_nodes.emplace(key, LibraryItem{library, &m_root});
        }
        LibraryItem* child = &m_nodes.at(key);
        m_root.appendChild(child);
    }

    endResetModel();
}

void LibraryModel::markForAddition(const LibraryInfo& info)
{
    if(info.name.isEmpty() || info.path.isEmpty()) {
        emit pendingRowCancelled();
        return;
    }

    const bool exists   = m_nodes.contains(info.path);
    const bool isQueued = exists && m_nodes.at(info.path).status() == LibraryItem::Removed;

    LibraryItem* item{nullptr};

    if(!isQueued) {
        if(exists) {
            qCInfo(LIB_MODEL) << "Library already exists:" << info.path;
            return;
        }
        // New library
        auto library   = m_librariesToAdd.emplace_back(info);
        library.status = LibraryInfo::Status::Pending;
        m_nodes.emplace(library.path, LibraryItem{library, &m_root});
        item = &m_nodes.at(library.path);
        item->setStatus(LibraryItem::Added);
    }
    else {
        // Library marked for delete
        if(exists) {
            item = &m_nodes.at(info.path);
            item->setStatus(LibraryItem::None);
            return;
        }
    }

    if(!item) {
        return;
    }

    const int row = m_root.childCount();
    beginInsertRows({}, row, row);
    m_root.appendChild(item);
    endInsertRows();
}

void LibraryModel::processQueue()
{
    std::vector<QString> librariesToRemove;

    for(auto& [path, library] : m_nodes) {
        const LibraryItem::ItemStatus status = library.status();
        const LibraryInfo info               = library.info();

        switch(status) {
            case(LibraryItem::Added): {
                if(info.id >= 0) {
                    // Library already added
                    break;
                }
                if(info.name.isEmpty() || info.path.isEmpty()) {
                    break;
                }

                const int id = m_libraryManager->addLibrary(info.path, info.name);
                if(id >= 0) {
                    if(auto newLibrary = m_libraryManager->libraryInfo(id)) {
                        library.changeInfo(*newLibrary);
                        library.setStatus(LibraryItem::None);

                        emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
                    }
                }
                else {
                    qCWarning(LIB_MODEL) << "Library could not be added:" << info.name;
                }
                break;
            }
            case(LibraryItem::Removed): {
                const QString key = info.path;
                if(m_libraryManager->removeLibrary(info.id)) {
                    beginRemoveRows({}, library.row(), library.row());
                    m_root.removeChild(library.row());
                    endRemoveRows();
                    librariesToRemove.push_back(key);
                }
                else {
                    qCWarning(LIB_MODEL) << "Library could not be removed:" << info.name;
                }
                break;
            }
            case(LibraryItem::Changed): {
                if(m_libraryManager->renameLibrary(info.id, info.name)) {
                    library.setStatus(LibraryItem::None);

                    emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
                }
                else {
                    qCWarning(LIB_MODEL) << "Library could not be renamed:" << info.name;
                }
                break;
            }
            case(LibraryItem::None):
                break;
        }
    }
    m_librariesToAdd.clear();
    for(const QString& path : librariesToRemove) {
        m_nodes.erase(path);
    }
}

Qt::ItemFlags LibraryModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    auto flags = ExtendableTableModel::flags(index);
    if(index.column() == 1) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return tr("ID");
        case(1):
            return tr("Name");
        case(2):
            return tr("Path");
        case(3):
            return tr("Status");
        default:
            break;
    }

    return {};
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<LibraryItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == Qt::UserRole) {
        return QVariant::fromValue(item->info());
    }

    if(role == Qt::DisplayRole || role == Qt::EditRole) {
        switch(index.column()) {
            case(0):
                return item->info().id;
            case(1):
                return item->info().name;
            case(2):
                return item->info().path;
            case(3):
                return Utils::Enum::toString(item->info().status);
            default:
                break;
        }
    }

    return {};
}

bool LibraryModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole || index.column() != 1) {
        return false;
    }

    auto* item       = static_cast<LibraryItem*>(index.internalPointer());
    LibraryInfo info = item->info();

    if(info.name == value.toString()) {
        return false;
    }
    info.name = value.toString();

    item->changeInfo(info);

    if(item->status() == LibraryItem::None) {
        item->setStatus(LibraryItem::Changed);
    }

    emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});

    return true;
}

QModelIndex LibraryModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    LibraryItem* item = m_root.child(row);

    return createIndex(row, column, item);
}

int LibraryModel::rowCount(const QModelIndex& /*parent*/) const
{
    return m_root.childCount();
}

int LibraryModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 4;
}

bool LibraryModel::removeRows(int row, int count, const QModelIndex& /*parent*/)
{
    for(int i{row}; i < row + count; ++i) {
        const QModelIndex& index = this->index(i, 0, {});

        if(!index.isValid()) {
            return false;
        }

        auto* item = static_cast<LibraryItem*>(index.internalPointer());
        if(item) {
            if(item->status() == LibraryItem::Added) {
                beginRemoveRows({}, i, i);
                m_root.removeChild(i);
                endRemoveRows();
                m_nodes.erase(item->info().path);
            }
            else {
                item->setStatus(LibraryItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }
    return true;
}

void LibraryModel::addPendingRow()
{
    emit requestAddLibrary();
}

void LibraryModel::removePendingRow() { }
} // namespace Fooyin
