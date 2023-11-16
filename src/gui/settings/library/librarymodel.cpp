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

#include "librarymodel.h"

#include <core/library/librarymanager.h>

#include <utils/enum.h>

#include <QFont>
#include <QSize>

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
    : TableModel{parent}
    , m_libraryManager{libraryManager}
{
    QObject::connect(m_libraryManager, &LibraryManager::libraryStatusChanged, this, &LibraryModel::updateDisplay);
}

void LibraryModel::populate()
{
    beginResetModel();
    reset();

    const LibraryInfoMap& libraries = m_libraryManager->allLibraries();

    for(const auto& [id, library] : libraries) {
        if(id < 0) {
            continue;
        }
        const QString key   = library.path;
        LibraryItem* parent = rootItem();

        if(!m_nodes.contains(key)) {
            m_nodes.emplace(key, LibraryItem{library, parent});
        }
        LibraryItem* child = &m_nodes.at(key);
        parent->appendChild(child);
    }

    endResetModel();
}

void LibraryModel::markForAddition(const LibraryInfo& info)
{
    const bool exists   = m_nodes.contains(info.path);
    const bool isQueued = exists && m_nodes.at(info.path).status() == LibraryItem::Removed;

    LibraryItem* parent = rootItem();
    LibraryItem* item{nullptr};

    if(!isQueued) {
        if(exists) {
            qInfo() << "Library at " + info.path + " already exists!";
            return;
        }
        // New library
        auto library   = m_librariesToAdd.emplace_back(info);
        library.status = LibraryInfo::Status::Pending;
        m_nodes.emplace(library.path, LibraryItem{library, parent});
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

    const int row = parent->childCount();
    beginInsertRows({}, row, row);
    parent->appendChild(item);
    endInsertRows();
}

void LibraryModel::markForRemoval(const LibraryInfo& info)
{
    if(!m_nodes.contains(info.path)) {
        return;
    }

    LibraryItem* item = &m_nodes.at(info.path);

    if(item->status() == LibraryItem::Added) {
        beginRemoveRows({}, item->row(), item->row());
        rootItem()->removeChild(item->row());
        endRemoveRows();

        m_nodes.erase(info.path);
    }
    else {
        item->setStatus(LibraryItem::Removed);
        emit dataChanged({}, {}, {Qt::FontRole});
    }
}

void LibraryModel::markForChange(const LibraryInfo& info)
{
    if(!m_nodes.contains(info.path)) {
        return;
    }

    LibraryItem* item = &m_nodes.at(info.path);

    item->changeInfo(info);
    const QModelIndex index = createIndex(item->row(), 1, item);
    emit dataChanged(index, index, {Qt::DisplayRole});

    if(item->status() == LibraryItem::None) {
        item->setStatus(LibraryItem::Changed);
    }
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
                    continue;
                }
                const int id = m_libraryManager->addLibrary(info.path, info.name);
                if(id >= 0) {
                    if(auto newLibrary = m_libraryManager->libraryInfo(id)) {
                        library.changeInfo(*newLibrary);
                        library.setStatus(LibraryItem::None);

                        emit dataChanged({}, {});
                    }
                }
                else {
                    qWarning() << "Library " + info.name + " could not be added";
                }
                break;
            }
            case(LibraryItem::Removed): {
                const QString key = info.path;
                if(info.id < 0 || m_libraryManager->removeLibrary(info.id)) {
                    beginRemoveRows({}, library.row(), library.row());
                    rootItem()->removeChild(library.row());
                    endRemoveRows();
                    librariesToRemove.push_back(key);
                }
                else {
                    qWarning() << "Library " + info.name + " could not be removed";
                }
                break;
            }
            case(LibraryItem::Changed): {
                if(m_libraryManager->renameLibrary(info.id, info.name)) {
                    library.setStatus(LibraryItem::None);

                    emit dataChanged({}, {});
                }
                else {
                    qWarning() << "Library " + info.name + " could not be renamed";
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

    auto flags = TableModel::flags(index);
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
            return QStringLiteral("ID");
        case(1):
            return QStringLiteral("Name");
        case(2):
            return QStringLiteral("Path");
        case(3):
            return QStringLiteral("Status");
    }
    return {};
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::FontRole && role != Qt::EditRole) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<LibraryItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    switch(index.column()) {
        case(0):
            return item->info().id;
        case(1):
            return item->info().name;
        case(2):
            return item->info().path;
        case(3):
            return Utils::Enum::toString(item->info().status);
    }

    return {};
}

bool LibraryModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole || index.column() != 1) {
        return false;
    }

    const auto* item = static_cast<LibraryItem*>(index.internalPointer());

    LibraryInfo info = item->info();

    if(info.name == value.toString()) {
        return false;
    }
    info.name = value.toString();

    markForChange(info);

    return true;
}

int LibraryModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 4;
}

void LibraryModel::reset()
{
    resetRoot();
    m_nodes.clear();
    m_librariesToAdd.clear();
}

void LibraryModel::updateDisplay(const LibraryInfo& info)
{
    if(m_nodes.contains(info.path)) {
        m_nodes.at(info.path).changeInfo(info);
        emit dataChanged({}, {}, {Qt::DisplayRole});
    }
}
} // namespace Fooyin
