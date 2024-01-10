/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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
#include <utils/treestatusitem.h>

#include <QFont>

namespace Fooyin {
class LibraryItem : public TreeStatusItem<LibraryItem>
{
public:
    LibraryItem()
        : LibraryItem{{}, nullptr}
    { }

    explicit LibraryItem(LibraryInfo info, LibraryItem* parent)
        : TreeStatusItem{parent}
        , m_info{std::move(info)}
    { }

    [[nodiscard]] LibraryInfo info() const
    {
        return m_info;
    }

    void changeInfo(const LibraryInfo& info)
    {
        m_info = info;
    }

private:
    LibraryInfo m_info;
};

struct LibraryModel::Private
{
    LibraryModel* self;

    LibraryManager* libraryManager;

    LibraryItem root;
    std::unordered_map<QString, LibraryItem> nodes;
    std::vector<LibraryInfo> librariesToAdd;

    Private(LibraryModel* self, LibraryManager* libraryManager)
        : self{self}
        , libraryManager{libraryManager}
    { }

    void updateDisplay(const LibraryInfo& info)
    {
        if(nodes.contains(info.path)) {
            nodes.at(info.path).changeInfo(info);
            QMetaObject::invokeMethod(self, "dataChanged", Q_ARG(const QModelIndex&, {}), Q_ARG(const QModelIndex&, {}),
                                      Q_ARG(const QList<int>&, {Qt::DisplayRole}));
        }
    }
};

LibraryModel::LibraryModel(LibraryManager* libraryManager, QObject* parent)
    : ExtendableTableModel{parent}
    , p{std::make_unique<Private>(this, libraryManager)}
{
    QObject::connect(p->libraryManager, &LibraryManager::libraryStatusChanged, this,
                     [this](const LibraryInfo& info) { p->updateDisplay(info); });
}

LibraryModel::~LibraryModel() = default;

void LibraryModel::populate()
{
    beginResetModel();
    p->root = {};
    p->nodes.clear();
    p->librariesToAdd.clear();

    const LibraryInfoMap& libraries = p->libraryManager->allLibraries();

    for(const auto& [id, library] : libraries) {
        if(id < 0) {
            continue;
        }
        const QString key = library.path;

        if(!p->nodes.contains(key)) {
            p->nodes.emplace(key, LibraryItem{library, &p->root});
        }
        LibraryItem* child = &p->nodes.at(key);
        p->root.appendChild(child);
    }

    endResetModel();
}

void LibraryModel::markForAddition(const LibraryInfo& info)
{
    if(info.name.isEmpty() || info.path.isEmpty()) {
        emit pendingRowCancelled();
        return;
    }

    const bool exists   = p->nodes.contains(info.path);
    const bool isQueued = exists && p->nodes.at(info.path).status() == LibraryItem::Removed;

    LibraryItem* item{nullptr};

    if(!isQueued) {
        if(exists) {
            qInfo() << "Library at " + info.path + " already exists!";
            return;
        }
        // New library
        auto library   = p->librariesToAdd.emplace_back(info);
        library.status = LibraryInfo::Status::Pending;
        p->nodes.emplace(library.path, LibraryItem{library, &p->root});
        item = &p->nodes.at(library.path);
        item->setStatus(LibraryItem::Added);
    }
    else {
        // Library marked for delete
        if(exists) {
            item = &p->nodes.at(info.path);
            item->setStatus(LibraryItem::None);
            return;
        }
    }

    if(!item) {
        return;
    }

    const int row = p->root.childCount();
    beginInsertRows({}, row, row);
    p->root.appendChild(item);
    endInsertRows();

    emit pendingRowAdded();
}

void LibraryModel::processQueue()
{
    std::vector<QString> librariesToRemove;

    for(auto& [path, library] : p->nodes) {
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

                const int id = p->libraryManager->addLibrary(info.path, info.name);
                if(id >= 0) {
                    if(auto newLibrary = p->libraryManager->libraryInfo(id)) {
                        library.changeInfo(*newLibrary);
                        library.setStatus(LibraryItem::None);

                        emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
                    }
                }
                else {
                    qWarning() << "Library " + info.name + " could not be added";
                }
                break;
            }
            case(LibraryItem::Removed): {
                const QString key = info.path;
                if(p->libraryManager->removeLibrary(info.id)) {
                    beginRemoveRows({}, library.row(), library.row());
                    p->root.removeChild(library.row());
                    endRemoveRows();
                    librariesToRemove.push_back(key);
                }
                else {
                    qWarning() << "Library " + info.name + " could not be removed";
                }
                break;
            }
            case(LibraryItem::Changed): {
                if(p->libraryManager->renameLibrary(info.id, info.name)) {
                    library.setStatus(LibraryItem::None);

                    emit dataChanged({}, {}, {Qt::DisplayRole, Qt::FontRole});
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
    p->librariesToAdd.clear();
    for(const QString& path : librariesToRemove) {
        p->nodes.erase(path);
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

    LibraryItem* item = p->root.child(row);

    return createIndex(row, column, item);
}

int LibraryModel::rowCount(const QModelIndex& /*parent*/) const
{
    return p->root.childCount();
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
                p->root.removeChild(i);
                endRemoveRows();
                p->nodes.erase(item->info().path);
            }
            else {
                item->setStatus(LibraryItem::Removed);
                emit dataChanged({}, {}, {Qt::FontRole});
            }
        }
    }
    return true;
}

void LibraryModel::addPendingRow() { }

void LibraryModel::removePendingRow() { }
} // namespace Fooyin
