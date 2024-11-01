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

#include "queuevieweritem.h"

#include <core/player/playbackqueue.h>
#include <core/scripting/scriptparser.h>
#include <gui/coverprovider.h>
#include <utils/treemodel.h>

#include <QIcon>

namespace Fooyin {
class QueueViewerModel : public TreeModel<QueueViewerItem>
{
    Q_OBJECT

public:
    explicit QueueViewerModel(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings,
                              QObject* parent = nullptr);

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] bool hasChildren(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

    void insertTracks(const QueueTracks& tracks, int row);
    void removeTracks(const QueueTracks& tracks);
    void removeIndexes(const std::vector<int>& indexes);

    void reset(const QueueTracks& tracks);
    QueueTracks queueTracks() const;

signals:
    void queueChanged();
    void tracksDropped(int row, const QByteArray& data);
    void playlistTracksDropped(int row, const QByteArray& data);

private:
    void regenerateTitles();
    void moveTracks(int row, const QModelIndexList& indexes);

    SettingsManager* m_settings;

    CoverProvider m_coverProvider;
    ScriptParser m_scriptParser;
    std::vector<std::unique_ptr<QueueViewerItem>> m_trackItems;
    std::unordered_map<QString, std::vector<QueueViewerItem*>> m_trackParents;
    bool m_showIcon;
    QSize m_iconSize;
};
} // namespace Fooyin
