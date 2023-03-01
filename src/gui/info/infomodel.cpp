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
    : TreeModel{parent}
    , m_playerManager{playerManager}
{
    setupModel();
}

void InfoModel::setupModel()
{
    InfoItem* root = rootItem();

    auto* metaData = addNode("Metadata", root, InfoItem::None, InfoItem::Header);

    addNode("Title", metaData, InfoItem::Title);
    addNode("Album", metaData, InfoItem::Album);
    addNode("Artist", metaData, InfoItem::Artist);
    addNode("Year", metaData, InfoItem::Year);
    addNode("Genre", metaData, InfoItem::Genre);
    addNode("Track Number", metaData, InfoItem::TrackNumber);

    auto* details = addNode("Details", root, InfoItem::None, InfoItem::Header);

    addNode("Filename", details, InfoItem::Filename);
    addNode("Path", details, InfoItem::Path);
    addNode("Duration", details, InfoItem::Duration);
    addNode("Bitrate", details, InfoItem::Bitrate);
    addNode("Sample Rate", details, InfoItem::SampleRate);
}

InfoItem* InfoModel::addNode(const QString& title, InfoItem* parent, InfoItem::Role role, InfoItem::Type type)
{
    auto* node = m_nodes.emplace_back(std::make_unique<InfoItem>(type, role, title, parent)).get();
    parent->appendChild(node);
    return node;
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

    if(role == Info::Role::Type) {
        return QVariant::fromValue<InfoItem::Type>(type);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    if(index.column() == 0) {
        return item->data();
    }

    Core::Track* track = m_playerManager->currentTrack();

    if(track) {
        switch(item->role()) {
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
            case(InfoItem::None):
                break;
        }
    }
    return {};
}
} // namespace Fy::Gui::Widgets
