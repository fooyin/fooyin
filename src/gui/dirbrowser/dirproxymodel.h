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

#pragma once

#include <core/player/playerdefs.h>

#include <QPixmap>
#include <QSortFilterProxyModel>

class QAbstractFileIconProvider;
class QDir;

namespace Fooyin {
struct DirNode
{
    QPersistentModelIndex sourceIndex;

    explicit DirNode(const QModelIndex& index)
        : sourceIndex{index}
    { }
};

class DirProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit DirProxyModel(QObject* parent = nullptr);

    void reset(const QModelIndex& root);
    void resetPalette();

    void setSourceModel(QAbstractItemModel* model) override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    [[nodiscard]] int rowCount(const QModelIndex& index) const override;
    [[nodiscard]] int columnCount(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex mapFromSource(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex mapToSource(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex sibling(int row, int column, const QModelIndex& index) const override;
    [[nodiscard]] bool hasChildren(const QModelIndex& parent) const override;

    [[nodiscard]] bool canGoUp() const;

    void setFlat(bool isFlat);
    void setIconsEnabled(bool enabled);
    void setPlayState(Player::PlayState state);
    void setPlayingPath(const QString& path);

private:
    void populate();
    [[nodiscard]] int nodeCount() const;
    void sourceRowsRemoved(const QModelIndex& parent, int first, int last);

    QAbstractFileIconProvider* m_iconProvider;
    bool m_flat;
    QString m_rootPath;
    QString m_goUpPath;

    QPersistentModelIndex m_sourceRoot;
    std::vector<std::unique_ptr<DirNode>> m_nodes;

    Player::PlayState m_playingState;
    QString m_playingTrackPath;

    bool m_showIcons;
    QColor m_playingColour;
    QPixmap m_playingIcon;
    QPixmap m_pausedIcon;
};
} // namespace Fooyin
