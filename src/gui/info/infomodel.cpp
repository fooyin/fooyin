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

#include "infomodel.h"

#include "infoitem.h"

#include <core/models/track.h>
#include <core/player/playermanager.h>
#include <utils/utils.h>

namespace Fy::Gui::Widgets {
InfoModel::InfoModel(Core::Player::PlayerManager* playerManager, QObject* parent)
    : QAbstractItemModel{parent}
    , m_playerManager{playerManager}
    , m_root{std::make_unique<InfoItem>()}
{
    setupModel();
}

void InfoModel::setupModel()
{
    auto* metadataHeader = new InfoItem{InfoItem::Header, "Metadata"};
    m_root->appendChild(metadataHeader);

    auto* title = new InfoItem{InfoItem::Entry, "Title"};
    m_root->appendChild(title);
    auto* album = new InfoItem{InfoItem::Entry, "Album"};
    m_root->appendChild(album);
    auto* artist = new InfoItem{InfoItem::Entry, "Artist"};
    m_root->appendChild(artist);
    auto* year = new InfoItem{InfoItem::Entry, "Year"};
    m_root->appendChild(year);
    auto* genre = new InfoItem{InfoItem::Entry, "Genre"};
    m_root->appendChild(genre);
    auto* trackNumber = new InfoItem{InfoItem::Entry, "Track Number"};
    m_root->appendChild(trackNumber);

    auto* detailsHeader = new InfoItem{InfoItem::Header, "Details"};
    m_root->appendChild(detailsHeader);

    auto* fileName = new InfoItem{InfoItem::Entry, "Filename"};
    m_root->appendChild(fileName);
    auto* path = new InfoItem{InfoItem::Entry, "Path"};
    m_root->appendChild(path);
    auto* duration = new InfoItem{InfoItem::Entry, "Duration"};
    m_root->appendChild(duration);
    auto* bitrate = new InfoItem{InfoItem::Entry, "Bitrate"};
    m_root->appendChild(bitrate);
    auto* sampleRate = new InfoItem{InfoItem::Entry, "Sample Rate"};
    m_root->appendChild(sampleRate);
}

void InfoModel::reset()
{
    emit dataChanged({}, {}, {Qt::DisplayRole});
}

QVariant InfoModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return "Name";
        case(1):
            return "Value";
    }
    return {};
}

int InfoModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return m_root->childCount();
}

int InfoModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return 2;
}

QVariant InfoModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return {};
    }

    auto* item                = static_cast<InfoItem*>(index.internalPointer());
    const InfoItem::Type type = item->type();

    const int row    = index.row();
    const int column = index.column();

    if(role == Info::Role::Type) {
        return QVariant::fromValue<InfoItem::Type>(type);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    if(column == 0) {
        return m_root->child(row)->data();
    }

    Core::Track* track = m_playerManager->currentTrack();

    if(track) {
        switch(row) {
            case(InfoItem::Title):
                return track->title();
            case(InfoItem::Artist):
                return track->artists().join(", ");
            case(InfoItem::Album):
                return track->album();
            case(InfoItem::Year):
                return track->year();
            case(InfoItem::Genre):
                return track->genres().join(", ");
            case(InfoItem::TrackNumber):
                return track->trackNumber();
            case(InfoItem::Filename):
                return track->filepath().split("/").constLast();
            case(InfoItem::Path):
                return track->filepath();
            case(InfoItem::Duration):
                return Utils::msToString(track->duration());
            case(InfoItem::Bitrate):
                return QString::number(track->bitrate()).append(" kbps");
            case(InfoItem::SampleRate):
                return track->sampleRate();
        }
    }
    return {};
}

QModelIndex InfoModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    InfoItem* parentItem = m_root.get();

    InfoItem* childItem = parentItem->child(row);

    if(childItem) {
        return createIndex(row, column, childItem);
    }
    return {};
}

QModelIndex InfoModel::parent(const QModelIndex& child) const
{
    Q_UNUSED(child)
    // All rows are parents of root
    return {};
}
} // namespace Fy::Gui::Widgets
