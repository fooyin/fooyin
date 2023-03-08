/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Gui::Settings {
LibraryModel::LibraryModel(Core::Library::LibraryManager* libraryManager, QObject* parent)
    : TableModel{parent}
    , m_libraryManager{libraryManager}
{
    setupModelData();
}

void LibraryModel::setupModelData()
{
    const auto& libraries = m_libraryManager->allLibrariesInfo();

    for(const auto& library : libraries) {
        if(!(library->id >= 0)) {
            continue;
        }
        const QString key   = library->path;
        LibraryItem* parent = rootItem();

        if(!m_nodes.count(key)) {
            m_nodes.emplace(key, std::make_unique<LibraryItem>(library.get(), parent));
        }
        LibraryItem* child = m_nodes.at(key).get();
        parent->appendChild(child);
    }
}

void LibraryModel::markForAddition(const Core::Library::LibraryInfo& info)
{
    LibraryItem* item{nullptr};
    LibraryItem* parent = rootItem();
    Core::Library::LibraryInfo* library;
    QueueEntry operation;

    const bool exists   = m_nodes.count(info.path);
    const bool isQueued = findInQueue(info.path, Remove, &operation);

    if(!isQueued) {
        if(exists) {
            qInfo() << QString{"Library at %1 already exists!"}.arg(info.path);
            return;
        }
        // New library
        m_librariesToAdd.emplace_back(std::make_unique<Core::Library::LibraryInfo>(info));
        library = m_librariesToAdd.back().get();
        m_nodes.emplace(library->path, std::make_unique<LibraryItem>(library, parent));
        item = m_nodes.at(library->path).get();
    }
    else {
        // Library marked for delete
        if(exists) {
            item    = m_nodes.at(info.path).get();
            library = item->info();
        }
        removeFromQueue(operation);
    }

    if(!item) {
        return;
    }

    const int row = parent->childCount();
    beginInsertRows({}, row, row);
    parent->appendChild(item);
    endInsertRows();

    operation = {Add, library};
    m_queue.emplace_back(operation);
}

void LibraryModel::markForRemoval(Core::Library::LibraryInfo* info)
{
    if(!info) {
        return;
    }

    LibraryItem* item{nullptr};
    QueueEntry operation;

    const bool exists   = m_nodes.count(info->path);
    const bool isQueued = findInQueue(info->path, Add, &operation);

    if(!isQueued) {
        if(!exists) {
            qInfo() << QString{"Library at %1 doesn't exist!"}.arg(info->path);
            return;
        }
        item = m_nodes.at(info->path).get();
    }
    else {
        removeFromQueue(operation);
    }

    if(!item) {
        return;
    }

    beginRemoveRows({}, item->row(), item->row());
    rootItem()->removeChild(item->row());
    endRemoveRows();

    operation = {Remove, info};
    m_queue.emplace_back(operation);
}

void LibraryModel::markForRename(Core::Library::LibraryInfo* info)
{
    if(!info) {
        return;
    }

    QueueEntry operation;

    const bool exists   = m_nodes.count(info->path);
    const bool isQueued = findInQueue(info->path, Add, &operation);

    if(!exists) {
        qInfo() << QString{"Library at %1 doesn't exist!"}.arg(info->path);
        return;
    }

    if(isQueued && operation.info->name == info->name) {
        // Name already changed
        return;
    }

    LibraryItem* item = m_nodes.at(info->path).get();
    item->changeInfo(info);
    const QModelIndex index = indexOfItem(item);
    emit dataChanged(index, index, {Qt::DisplayRole});

    operation = {Rename, info};
    m_queue.emplace_back(operation);
}

void LibraryModel::processQueue()
{
    while(!m_queue.empty()) {
        const auto library = m_queue.front();
        const auto type    = library.type;
        auto* info         = library.info;

        m_queue.pop_front();

        if(type == Add) {
            if(info->id >= 0) {
                // Library already added
                continue;
            }
            const int id = m_libraryManager->addLibrary(info->path, info->name);
            if(id >= 0) {
                info = m_libraryManager->libraryInfo(id);
                m_nodes.at(info->path)->changeInfo(info);
            }
            else {
                qWarning() << QString{"Library (%1) could not be added"}.arg(info->name);

                auto* item = m_nodes.at(info->path).get();
                beginRemoveRows({}, item->row(), item->row());
                rootItem()->removeChild(item->row());
                endRemoveRows();

                m_nodes.erase(info->path);
            }
        }
        else if(type == Remove) {
            const QString key = info->path;
            if(info->id < 0 || m_libraryManager->removeLibrary(info->id)) {
                m_nodes.erase(key);
            }
            else {
                qWarning() << QString{"Library (%1) could not be removed"}.arg(info->name);

                auto* item    = m_nodes.at(info->path).get();
                const int row = rootItem()->childCount();
                beginInsertRows({}, row, row);
                rootItem()->appendChild(item);
                endInsertRows();
            }
        }
        else if(type == Rename) {
            if(!m_libraryManager->renameLibrary(info->id, info->name)) {
                qWarning() << QString{"Library (%1) could not be renamed"}.arg(info->path);

                auto* item = m_nodes.at(info->path).get();
                item->changeInfo(m_libraryManager->libraryInfo(info->id));
                const QModelIndex index = indexOfItem(item);
                emit dataChanged(index, index, {Qt::DisplayRole});
            }
        }
    }
    m_librariesToAdd.clear();
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
            return "ID";
        case(1):
            return "Name";
        case(2):
            return "Path";
    }
    return {};
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole) {
        return {};
    }

    if(!index.isValid()) {
        return {};
    }

    const int column = index.column();
    auto* item       = static_cast<LibraryItem*>(index.internalPointer());

    switch(column) {
        case(0):
            return item->info()->id;
        case(1):
            return item->info()->name;
        case(2):
            return item->info()->path;
    }

    return {};
}

int LibraryModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return rootItem()->childCount();
}

int LibraryModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 3;
}

bool LibraryModel::findInQueue(const QString& id, OperationType type, QueueEntry* library) const
{
    return std::any_of(m_queue.cbegin(), m_queue.cend(), [id, type, library](const QueueEntry& entry) {
        if(entry.info->path == id && entry.type == type) {
            *library = entry;
            return true;
        }
        return false;
    });
}

void LibraryModel::removeFromQueue(const QueueEntry& libraryToDelete)
{
    m_queue.erase(std::remove_if(m_queue.begin(), m_queue.end(),
                                 [libraryToDelete](const QueueEntry& library) {
                                     return library == libraryToDelete;
                                 }),
                  m_queue.end());
}
} // namespace Fy::Gui::Settings
