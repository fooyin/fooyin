/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "infoitem.h"
#include "infopopulator.h"

#include <utils/treemodel.h>

#include <QFont>
#include <QThread>

namespace Fooyin {
struct InfoData;
class InfoPopulator;

class InfoModel : public TreeModel<InfoItem>
{
    Q_OBJECT

public:
    enum class ItemParent
    {
        Root,
        Metadata,
        Location,
        General
    };
    Q_ENUM(ItemParent)

    explicit InfoModel(QObject* parent = nullptr);
    ~InfoModel() override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    [[nodiscard]] InfoItem::Options options() const;
    void setOption(InfoItem::Option option, bool enabled);
    void setOptions(InfoItem::Options options);

    void resetModel(const TrackList& tracks, const Track& playingTrack);

private:
    void populate(const InfoData& data);

    QThread m_populatorThread;
    InfoPopulator m_populator;
    std::unordered_map<QString, InfoItem> m_nodes;

    InfoItem::Options m_options{InfoItem::Default};
    QFont m_headerFont;
};
} // namespace Fooyin
