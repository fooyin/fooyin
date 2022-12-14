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

#include "infomodel.h"

#include "core/library/models/track.h"
#include "core/typedefs.h"
#include "infoitem.h"

#include <utils/utils.h>

InfoModel::InfoModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_root(std::make_unique<InfoItem>())
    , m_currentTrack(nullptr)
{
    setupModel();
}

void InfoModel::setupModel()
{
    auto* metadataHeader = new InfoItem{InfoItem::Type::Header, "Metadata"};
    m_root->appendChild(metadataHeader);

    auto* title = new InfoItem{InfoItem::Type::Entry, "Title"};
    m_root->appendChild(title);
    auto* album = new InfoItem{InfoItem::Type::Entry, "Album"};
    m_root->appendChild(album);
    auto* artist = new InfoItem{InfoItem::Type::Entry, "Artist"};
    m_root->appendChild(artist);
    auto* year = new InfoItem{InfoItem::Type::Entry, "Year"};
    m_root->appendChild(year);
    auto* genre = new InfoItem{InfoItem::Type::Entry, "Genre"};
    m_root->appendChild(genre);
    auto* trackNumber = new InfoItem{InfoItem::Type::Entry, "Track Number"};
    m_root->appendChild(trackNumber);

    auto* detailsHeader = new InfoItem{InfoItem::Type::Header, "Details"};
    m_root->appendChild(detailsHeader);

    auto* fileName = new InfoItem{InfoItem::Type::Entry, "Filename"};
    m_root->appendChild(fileName);
    auto* path = new InfoItem{InfoItem::Type::Entry, "Path"};
    m_root->appendChild(path);
    auto* duration = new InfoItem{InfoItem::Type::Entry, "Duration"};
    m_root->appendChild(duration);
    auto* bitrate = new InfoItem{InfoItem::Type::Entry, "Bitrate"};
    m_root->appendChild(bitrate);
    auto* sampleRate = new InfoItem{InfoItem::Type::Entry, "Sample Rate"};
    m_root->appendChild(sampleRate);
}

void InfoModel::reset(Track* track)
{
    m_currentTrack = track;
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

InfoModel::~InfoModel() = default;

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

    auto* item = static_cast<InfoItem*>(index.internalPointer());
    const InfoItem::Type type = item->type();

    const int row = index.row();
    const int column = index.column();

    if(role == Role::Type) {
        return QVariant::fromValue<InfoItem::Type>(type);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    if(column == 0) {
        return m_root->child(row)->data();
    }

    if(m_currentTrack) {
        switch(row) {
            case(InfoRole::Title):
                return m_currentTrack->title();
            case(InfoRole::Artist):
                return m_currentTrack->artists().join(", ");
            case(InfoRole::Album):
                return m_currentTrack->album();
            case(InfoRole::Year):
                return m_currentTrack->year();
            case(InfoRole::Genre):
                return m_currentTrack->genres().join(", ");
            case(InfoRole::TrackNumber):
                return m_currentTrack->trackNumber();
            case(InfoRole::Filename):
                return m_currentTrack->filepath().split("/").constLast();
            case(InfoRole::Path):
                return m_currentTrack->filepath();
            case(InfoRole::Duration):
                return Util::msToString(m_currentTrack->duration());
            case(InfoRole::Bitrate):
                return QString::number(m_currentTrack->bitrate()).append(" kbps");
            case(InfoRole::SampleRate):
                return m_currentTrack->sampleRate();
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
