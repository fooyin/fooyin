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

#include <utils/enumhelper.h>

#include <QFont>
#include <QSize>

namespace Fy::Gui::Settings {
LibraryItem::LibraryItem()
    : LibraryItem{nullptr, nullptr}
{ }

LibraryItem::LibraryItem(Core::Library::LibraryInfo* info, LibraryItem* parent)
    : TreeStatusItem{parent}
    , m_info{info}
{ }

Core::Library::LibraryInfo* LibraryItem::info() const
{
    return m_info;
}

void LibraryItem::changeInfo(Core::Library::LibraryInfo* info)
{
    m_info = info;
}

LibraryModel::LibraryModel(Core::Library::LibraryManager* libraryManager, QObject* parent)
    : TableModel{parent}
    , m_libraryManager{libraryManager}
{
    setupModelData();

    QObject::connect(m_libraryManager, &Core::Library::LibraryManager::libraryStatusChanged, this,
                     &LibraryModel::updateDisplay);
}

void LibraryModel::setupModelData()
{
    const LibraryInfoList& libraries = m_libraryManager->allLibraries();

    for(const auto& library : libraries) {
        if(library->id < 0) {
            continue;
        }
        const QString key   = library->path;
        LibraryItem* parent = rootItem();

        if(!m_nodes.contains(key)) {
            m_nodes.emplace(key, LibraryItem{library.get(), parent});
        }
        LibraryItem* child = &m_nodes.at(key);
        parent->appendChild(child);
    }
}

void LibraryModel::markForAddition(const Core::Library::LibraryInfo& info)
{
    const bool exists   = m_nodes.contains(info.path);
    const bool isQueued = exists && m_nodes.at(info.path).status() == LibraryItem::Removed;

    LibraryItem* parent = rootItem();
    LibraryItem* item{nullptr};

    if(!isQueued) {
        if(exists) {
            qInfo() << QString{"Library at %1 already exists!"}.arg(info.path);
            return;
        }
        // New library
        auto* library   = m_librariesToAdd.emplace_back(std::make_unique<Core::Library::LibraryInfo>(info)).get();
        library->status = Core::Library::Pending;
        m_nodes.emplace(library->path, LibraryItem{library, parent});
        item = &m_nodes.at(library->path);
        item->setStatus(LibraryItem::Added);
    }
    else {
        // Library marked for delete
        if(exists) {
            item = &m_nodes.at(info.path);
            item->setStatus(LibraryItem::None);
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

void LibraryModel::markForRemoval(Core::Library::LibraryInfo* info)
{
    if(!info || !m_nodes.contains(info->path)) {
        return;
    }

    LibraryItem* item = &m_nodes.at(info->path);

    if(item->status() == LibraryItem::Added) {
        beginRemoveRows({}, item->row(), item->row());
        rootItem()->removeChild(item->row());
        endRemoveRows();

        m_nodes.erase(info->path);
    }
    else {
        item->setStatus(LibraryItem::Removed);
        emit dataChanged({}, {}, {Qt::FontRole});
    }
}

void LibraryModel::markForChange(Core::Library::LibraryInfo* info)
{
    if(!info || !m_nodes.contains(info->path)) {
        return;
    }

    LibraryItem* item = &m_nodes.at(info->path);

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
        Core::Library::LibraryInfo* info     = library.info();

        switch(status) {
            case(LibraryItem::Added): {
                if(info->id >= 0) {
                    // Library already added
                    continue;
                }
                const int id = m_libraryManager->addLibrary(info->path, info->name);
                if(id >= 0) {
                    info = m_libraryManager->libraryInfo(id);
                    library.changeInfo(info);
                    library.setStatus(LibraryItem::None);
                }
                else {
                    qWarning() << QString{"Library (%1) could not be added"}.arg(info->name);
                }
                break;
            }
            case(LibraryItem::Removed): {
                const QString key = info->path;
                if(info->id < 0 || m_libraryManager->removeLibrary(info->id)) {
                    beginRemoveRows({}, library.row(), library.row());
                    rootItem()->removeChild(library.row());
                    endRemoveRows();
                    librariesToRemove.push_back(key);
                }
                else {
                    qWarning() << QString{"Library (%1) could not be removed"}.arg(info->name);
                }
                break;
            }
            case(LibraryItem::Changed): {
                if(m_libraryManager->renameLibrary(info->id, info->name)) {
                    library.setStatus(LibraryItem::None);
                }
                else {
                    qWarning() << QString{"Library (%1) could not be renamed"}.arg(info->path);
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
        case(3):
            return "Status";
    }
    return {};
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::FontRole) {
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
            return item->info()->id;
        case(1):
            return item->info()->name;
        case(2):
            return item->info()->path;
        case(3):
            return Utils::EnumHelper::toString(item->info()->status);
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
    return 4;
}

void LibraryModel::updateDisplay()
{
    emit dataChanged({}, {}, {Qt::DisplayRole});
}
} // namespace Fy::Gui::Settings
