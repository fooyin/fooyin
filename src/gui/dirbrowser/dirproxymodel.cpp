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
    : QAbstractProxyModel{parent}
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

    QDir path{root.data(QFileSystemModel::FilePathRole).toString()};
    m_goUpPath   = path.cdUp() ? path.absolutePath() : QString{};
    m_sourceRoot = root;
    populate();

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

    QAbstractProxyModel::setSourceModel(model);
}

Qt::ItemFlags DirProxyModel::flags(const QModelIndex& index) const
{
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

    if(canGoUp() && proxyIndex.row() == 0 && proxyIndex.column() == 0) {
        if(role == Qt::DisplayRole) {
            return QStringLiteral("…");
        }
        if(role == QFileSystemModel::FilePathRole) {
            return m_goUpPath;
        }
        if(role == Qt::TextAlignmentRole) {
            return Qt::AlignBottom;
        }
        if(m_showIcons && role == Qt::DecorationRole) {
            return m_iconProvider->icon(QAbstractFileIconProvider::IconType::Folder);
        }
    }

    const QModelIndex sourceIndex = mapToSource(proxyIndex);
    const QString sourcePath      = sourceModel()->data(sourceIndex, QFileSystemModel::FilePathRole).toString();

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

    if(!m_showIcons && role == Qt::DecorationRole) {
        return {};
    }

    return sourceModel()->data(sourceIndex, role);
}

QModelIndex DirProxyModel::parent(const QModelIndex& /*child*/) const
{
    return {};
}

bool DirProxyModel::hasChildren(const QModelIndex& parent) const
{
    return !parent.isValid();
}

QModelIndex DirProxyModel::index(int row, int column, const QModelIndex& parent) const
{
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
    return parent.isValid() ? 0 : nodeCount();
}

int DirProxyModel::columnCount(const QModelIndex& /*index*/) const
{
    return 1;
}

QModelIndex DirProxyModel::mapFromSource(const QModelIndex& index) const
{
    if(!index.isValid() || m_nodes.empty()) {
        return {};
    }

    for(int row{0}; const auto& node : m_nodes) {
        if(node->sourceIndex == index) {
            return createIndex(row++, 0, node.get());
        }
    }

    return {};
}

QModelIndex DirProxyModel::mapToSource(const QModelIndex& index) const
{
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
} // namespace Fooyin
