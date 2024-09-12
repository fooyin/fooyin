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

#include "replaygainitem.h"
#include "replaygainpopulator.h"

#include <core/track.h>
#include <utils/treemodel.h>

#include <QThread>

namespace Fooyin {
class ReplayGainModel : public TreeModel<ReplayGainItem>
{
    Q_OBJECT

public:
    enum class ItemParent : uint8_t
    {
        Root = 0,
        Summary,
        Details
    };
    Q_ENUM(ItemParent)

    explicit ReplayGainModel(QObject* parent = nullptr);
    ~ReplayGainModel() override;

    void resetModel(const TrackList& tracks);
    TrackList applyChanges();

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

private:
    void populate(const RGInfoData& data);

    QThread m_populatorThread;
    ReplayGainPopulator m_populator;
    TrackList m_tracks;
    std::unordered_map<QString, ReplayGainItem> m_nodes;

    QFont m_headerFont;
};
} // namespace Fooyin
