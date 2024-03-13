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

#include "dirproxymodel.h"

#include <gui/guiconstants.h>
#include <utils/utils.h>

#include <QAbstractFileIconProvider>
#include <QApplication>
#include <QFileSystemModel>
#include <QPalette>

namespace Fooyin {
DirProxyModel::DirProxyModel(QObject* parent)
    : QSortFilterProxyModel{parent}
    , m_flat{true}
    , m_playingState{PlayState::Stopped}
    , m_showIcons{true}
    , m_playingColour{QApplication::palette().highlight().color()}
    , m_playingIcon{Utils::iconFromTheme(Constants::Icons::Play).pixmap(20, 20)}
    , m_pausedIcon{Utils::iconFromTheme(Constants::Icons::Pause).pixmap(20, 20)}
{
    m_playingColour.setAlpha(90);
}

void DirProxyModel::reset(const QModelIndex& root)
{
    if(!sourceModel()) {
        return;
    }

    beginResetModel();

    m_sourceRoot = root;
    m_nodes.clear();

    QDir path{root.data(QFileSystemModel::FilePathRole).toString()};
    m_rootPath = path.absolutePath();
    m_goUpPath = path.cdUp() ? path.absolutePath() : QString{};

    if(m_flat) {
        populate();
    }

    endResetModel();
}

void DirProxyModel::setSourceModel(QAbstractItemModel* model)
{
    if(model == sourceModel()) {
        return;
    }

    if(sourceModel()) {
        disconnect(sourceModel(), nullptr, this, nullptr);
    }

    if(auto* fileModel = qobject_cast<QFileSystemModel*>(model)) {
        m_iconProvider = fileModel->iconProvider();
    }

    // We only need to handle rowsRemoved as layoutChanged is emitted from the sourceModel
    // for all other changes, which resets this model
    QObject::connect(model, &QAbstractItemModel::rowsRemoved, this, &DirProxyModel::sourceRowsRemoved);

    QSortFilterProxyModel::setSourceModel(model);
}

Qt::ItemFlags DirProxyModel::flags(const QModelIndex& index) const
{
    if(!m_flat) {
        return QSortFilterProxyModel::flags(index);
    }

    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    if(canGoUp() && index.row() <= 0) {
        return sourceModel()->flags(m_sourceRoot);
    }

    return sourceModel()->flags(mapToSource(index));
}

QVariant DirProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    return sourceModel()->headerData(section, orientation, role);
}

QVariant DirProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
    if(!proxyIndex.isValid()) {
        return {};
    }

    if(!m_showIcons && role == Qt::DecorationRole) {
        return {};
    }

    QString sourcePath;

    if(m_flat) {
        if(canGoUp() && proxyIndex.row() == 0 && proxyIndex.column() == 0) {
            if(role == Qt::DisplayRole) {
                return QStringLiteral("…");
            }
            if(role == QFileSystemModel::FilePathRole) {
                return {};
            }
            if(role == Qt::TextAlignmentRole) {
                return Qt::AlignBottom;
            }
            if(m_iconProvider && m_showIcons && role == Qt::DecorationRole) {
                return m_iconProvider->icon(QAbstractFileIconProvider::IconType::Folder);
            }
        }
        sourcePath = sourceModel()->data(mapToSource(proxyIndex), QFileSystemModel::FilePathRole).toString();
    }
    else {
        sourcePath = QSortFilterProxyModel::data(proxyIndex, QFileSystemModel::FilePathRole).toString();
    }

    if(!m_playingTrackPath.isEmpty() && sourcePath == m_playingTrackPath) {
        if(role == Qt::BackgroundRole) {
            return m_playingColour;
        }
        if(role == Qt::DecorationRole) {
            switch(m_playingState) {
                case(PlayState::Playing):
                    return m_playingIcon;
                case(PlayState::Paused):
                    return m_pausedIcon;
                case(PlayState::Stopped):
                    break;
            }
        }
    }

    if(m_flat) {
        return sourceModel()->data(mapToSource(proxyIndex), role);
    }

    return QSortFilterProxyModel::data(proxyIndex, role);
}

QModelIndex DirProxyModel::parent(const QModelIndex& child) const
{
    if(m_flat) {
        return {};
    }

    return QSortFilterProxyModel::parent(child);
}

bool DirProxyModel::hasChildren(const QModelIndex& parent) const
{
    if(m_flat) {
        return !parent.isValid();
    }

    return QSortFilterProxyModel::hasChildren(parent);
}

QModelIndex DirProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!m_flat) {
        return QSortFilterProxyModel::index(row, column, parent);
    }

    if(!hasIndex(row, column, parent)) {
        return {};
    }

    if(canGoUp() && row == 0) {
        return createIndex(row, column, nullptr);
    }

    return createIndex(row, column, m_nodes.at(row).get());
}

int DirProxyModel::rowCount(const QModelIndex& parent) const
{
    if(!m_flat) {
        return QSortFilterProxyModel::rowCount(parent);
    }

    return parent.isValid() ? 0 : nodeCount();
}

int DirProxyModel::columnCount(const QModelIndex& /*index*/) const
{
    return 1;
}

QModelIndex DirProxyModel::mapFromSource(const QModelIndex& index) const
{
    if(!m_flat) {
        return QSortFilterProxyModel::mapFromSource(index);
    }

    if(!index.isValid() || m_nodes.empty()) {
        return {};
    }

    const auto indexIt = std::find_if(m_nodes.cbegin(), m_nodes.cend(),
                                      [&index](const auto& node) { return node->sourceIndex == index; });
    if(indexIt != m_nodes.cend()) {
        const auto row = static_cast<int>(std::distance(m_nodes.begin(), indexIt));
        return createIndex(row, 0, indexIt->get());
    }

    return {};
}

QModelIndex DirProxyModel::mapToSource(const QModelIndex& index) const
{
    if(!m_flat) {
        return QSortFilterProxyModel::mapToSource(index);
    }

    if(!index.isValid() || m_nodes.empty()) {
        return {};
    }

    if(auto* node = static_cast<DirNode*>(index.internalPointer())) {
        return node->sourceIndex;
    }

    return {};
}

bool DirProxyModel::canGoUp() const
{
    return !m_goUpPath.isEmpty();
}

void DirProxyModel::setFlat(bool isFlat)
{
    m_flat = isFlat;

    if(auto* fileModel = qobject_cast<QFileSystemModel*>(sourceModel())) {
        reset(fileModel->index(fileModel->rootPath()));
    }
}

void DirProxyModel::setIconsEnabled(bool enabled)
{
    m_showIcons = enabled;
    emit dataChanged({}, {}, {Qt::DecorationRole});
}

void DirProxyModel::setPlayState(PlayState state)
{
    m_playingState = state;

    if(state == PlayState::Stopped) {
        m_playingTrackPath.clear();
    }

    emit dataChanged({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
}

void DirProxyModel::setPlayingPath(const QString& path)
{
    m_playingTrackPath = path;
    emit dataChanged({}, {}, {Qt::DecorationRole, Qt::BackgroundRole});
}

void DirProxyModel::populate()
{
    m_nodes.clear();

    if(canGoUp()) {
        m_nodes.emplace_back(std::make_unique<DirNode>(QModelIndex{}));
    }

    const int rowCount = sourceModel()->rowCount(m_sourceRoot);

    if(rowCount <= 0) {
        return;
    }

    const int last = rowCount - 1;

    m_nodes.reserve(m_nodes.size() + rowCount);

    for(int row{0}; row <= last; ++row) {
        m_nodes.emplace_back(std::make_unique<DirNode>(sourceModel()->index(row, 0, m_sourceRoot)));
    }
}

int DirProxyModel::nodeCount() const
{
    return static_cast<int>(m_nodes.size());
}

void DirProxyModel::sourceRowsRemoved(const QModelIndex& parent, int first, int last)
{
    const QString path = parent.data(QFileSystemModel::FilePathRole).toString();
    if(path != m_rootPath) {
        return;
    }

    const int firstRow = first + (canGoUp() ? 1 : 0);
    const int lastRow  = last + (canGoUp() ? 1 : 0);

    if(firstRow < 0 || std::cmp_greater_equal(firstRow, m_nodes.size())) {
        return;
    }

    if(lastRow < 0 || std::cmp_greater_equal(lastRow, m_nodes.size())) {
        return;
    }

    beginRemoveRows({}, firstRow, lastRow);
    m_nodes.erase(m_nodes.begin() + firstRow, std::next(m_nodes.begin() + lastRow));
    endRemoveRows();
}
} // namespace Fooyin
