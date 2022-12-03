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

#include "playlistmodel.h"

#include "core/library/coverprovider.h"
#include "core/library/models/album.h"
#include "core/library/models/disc.h"
#include "core/library/models/libraryitem.h"
#include "core/library/models/track.h"
#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"
#include "playlistitem.h"
#include "core/settings/settings.h"

#include <PluginManager>
#include <QPalette>
#include <utils/utils.h>

namespace Library {
PlaylistModel::PlaylistModel(PlayerManager* playerManager, Library::MusicLibrary* library, QObject* parent)
    : QAbstractItemModel(parent)
    , m_root(std::make_unique<PlaylistItem>())
    , m_playerManager(playerManager)
    , m_library(library)
    , m_settings(PluginSystem::object<Settings>())
{
    setupModelData();

    connect(m_settings, &Settings::playlistSettingChanged, this, &PlaylistModel::reset);
    connect(m_settings, &Settings::playlistAltColorsChanged, this, &PlaylistModel::changeRowColours);
    connect(m_library, &Library::MusicLibrary::filteredTracks, this, &PlaylistModel::reset);
}

PlaylistModel::~PlaylistModel() = default;

void PlaylistModel::setupModelData()
{
    const bool discHeaders = m_settings->value(Settings::Setting::DiscHeaders).toBool();
    const bool splitDiscs = m_settings->value(Settings::Setting::SplitDiscs).toBool();

    if(m_library) {
        const TrackPtrList& tracks = m_library->tracks();
        if(!tracks.isEmpty()) {
            // Create albums before model to ensure discs (based on discCount) are properly created
            createAlbums(tracks);
            for(int i = 0; i < tracks.size(); ++i) {
                Track* track = tracks.at(i);
                if(track && track->isEnabled()) {
                    const QString trackKey = QString::number(track->id()) + track->title();
                    PlaylistItem* parent = iterateTrack(track, discHeaders, splitDiscs);

                    if(parent) {
                        if(!m_nodes.count(trackKey)) {
                            auto trackItem = std::make_unique<PlaylistItem>(PlaylistItem::Type::Track, track, parent);
                            m_nodes.emplace(trackKey, std::move(trackItem));
                        }
                        auto* trackItem = m_nodes.at(trackKey).get();
                        trackItem->setIndex(i);
                        parent->appendChild(trackItem);
                    }
                }
            }
        }
    }
}

PlaylistItem* PlaylistModel::iterateTrack(Track* track, bool discHeaders, bool splitDiscs)
{
    PlaylistItem* parent = nullptr;

    const QString albumKey = QString::number(track->albumId()) + track->album();
    const QString discKey = albumKey + QString::number(track->discNumber());

    if(m_containers.count(albumKey) == 1) {
        auto* album = dynamic_cast<Album*>(m_containers.at(albumKey).get());
        const bool singleDisk = album->isSingleDiscAlbum() || (!splitDiscs && !discHeaders);

        if(singleDisk) {
            parent = checkInsertKey(albumKey, PlaylistItem::Type::Album, album, m_root.get());
        }

        else if(splitDiscs) {
            if(!m_containers.count(discKey)) {
                auto discAlbum = std::make_unique<Album>(*album);
                const QString discTitle = "Disc #" + QString::number(track->discNumber());
                discAlbum->setSubTitle(discTitle);
                discAlbum->reset();
                checkInsertKey(discKey, PlaylistItem::Type::Album, discAlbum.get(), m_root.get());
                m_containers.emplace(discKey, std::move(discAlbum));
            }
            Container* discAlbum = m_containers.at(discKey).get();
            discAlbum->addTrack(track);
            parent = m_nodes.at(discKey).get();
        }

        else {
            if(!m_containers.count(discKey)) {
                auto disc = std::make_unique<Disc>(track->discNumber());
                PlaylistItem* parentNode = checkInsertKey(albumKey, PlaylistItem::Type::Album, album, m_root.get());
                checkInsertKey(discKey, PlaylistItem::Type::Disc, disc.get(), parentNode);
                m_containers.emplace(discKey, std::move(disc));
            }
            Container* disc = m_containers.at(discKey).get();
            disc->addTrack(track);
            parent = m_nodes.at(discKey).get();
        }
    }
    return parent;
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }

    return QAbstractItemModel::flags(index);
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(!m_library || role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    return QString("%1 Tracks").arg(m_library->tracks().size());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return {};
    }

    auto* item = static_cast<PlaylistItem*>(index.internalPointer());

    const PlaylistItem::Type type = item->type();

    if(role == Role::PlaylistType) {
        return m_settings->value(Settings::Setting::SimplePlaylist);
    }

    if(role == Role::Type) {
        return QVariant::fromValue<PlaylistItem::Type>(type);
    }

    switch(type) {
        case(PlaylistItem::Type::Album):
            return albumData(item, role);
        case(PlaylistItem::Type::Track):
            return trackData(item, role);
        case(PlaylistItem::Type::Disc):
            return discData(item, role);
        default:
            return {};
    }
    return {};
}

QVariant PlaylistModel::trackData(PlaylistItem* item, int role) const
{
    auto* track = static_cast<Track*>(item->data());

    if(!track) {
        return {};
    }

    switch(role) {
        case(ItemRole::Id): {
            return track->id();
        }
        case(ItemRole::Number): {
            return QStringLiteral("%1").arg(track->trackNumber(), 2, 10, QLatin1Char('0'));
        }
        case(Qt::DisplayRole): {
            return !track->title().isEmpty() ? track->title() : "Unknown Title";
        }
        case(ItemRole::Artist): {
            return trackArtistString(track);
        }
        case(ItemRole::PlayCount): {
            const int count = track->playCount();
            if(count > 0) {
                return QString::number(count) + QString("|");
            }
            return {};
        }
        case(ItemRole::Duration): {
            return Util::msToString(track->duration());
        }
        case(ItemRole::MultiDisk): {
            if(item->parent()->type() == PlaylistItem::Type::Disc
               && m_settings->value(Settings::Setting::DiscHeaders).toBool()
               && !m_settings->value(Settings::Setting::SplitDiscs).toBool()) {
                return true;
            }
            return false;
        }
        case(ItemRole::Playing): {
            return m_playerManager->currentTrack() && m_playerManager->currentTrack()->id() == track->id();
        }
        case(ItemRole::State): {
            switch(m_playerManager->playState()) {
                case(Player::PlayState::Playing):
                    return "1";
                case(Player::PlayState::Paused):
                    return "2";
                default:
                    break;
            }
            break;
        }
        case(ItemRole::Path): {
            return track->filepath();
        }
        case(ItemRole::Index): {
            return item->index();
        }
        case(ItemRole::Data): {
            return QVariant::fromValue<Track*>(track);
        }
        case(Qt::BackgroundRole): {
            return m_settings->value(Settings::Setting::PlaylistAltColours).toBool()
                     ? item->row() & 1 ? QPalette::Base : QPalette::AlternateBase
                     : QPalette::Base;
        }
        default: {
            return {};
        }
    }
    return {};
}

QVariant PlaylistModel::albumData(PlaylistItem* item, int role) const
{
    auto* album = static_cast<Album*>(item->data());

    if(!album) {
        return {};
    }

    switch(role) {
        case(ItemRole::Id): {
            return album->id();
        }
        case(Qt::DisplayRole): {
            QString title = !album->title().isEmpty() ? album->title() : "Unknown Title";
            if(!album->subTitle().isEmpty()) {
                title += " \u25AA ";
                title += album->subTitle();
            }
            return title;
        }
        case(ItemRole::Cover): {
            return Covers::albumCover(album);
        }
        case(ItemRole::Artist): {
            return !album->artist().isEmpty() ? album->artist() : "Unknown Artist";
        }
        case(ItemRole::Duration): {
            const auto genre = album->genres().join(" / ");
            const auto count = album->trackCount();
            const auto duration = album->duration();

            QString dur = genre;
            if(!genre.isEmpty()) {
                dur += " | ";
            }
            dur += QString(QString::number(count) + (count > 1 ? " Tracks" : " Track") + " | "
                           + Util::msToString(duration));
            return dur;
        }
        case(ItemRole::Year): {
            return album->year();
        }
        default: {
            return {};
        }
    }
    return {};
}

QVariant PlaylistModel::discData(PlaylistItem* item, int role) const
{
    auto* disc = static_cast<Disc*>(item->data());

    if(!disc) {
        return {};
    }

    switch(role) {
        case(Qt::DisplayRole): {
            return "Disc #" + QString::number(disc->number());
        }
        case(ItemRole::Duration): {
            auto duration = static_cast<int>(disc->duration());
            return QString(Util::msToString(duration));
        }
        default: {
            return {};
        }
    }
}

QString PlaylistModel::trackArtistString(Track* track) const
{
    QString artistString;
    for(const auto& artist : track->artists()) {
        if(artist != track->albumArtist()) {
            if(artistString.isEmpty()) {
                artistString += "  \u2022  ";
            }
            else {
                artistString += ", ";
            }
            artistString += artist;
        }
    }
    return artistString;
}

QModelIndex PlaylistModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    PlaylistItem* parentItem;

    if(!parent.isValid()) {
        parentItem = m_root.get();
    }
    else {
        parentItem = static_cast<PlaylistItem*>(parent.internalPointer());
    }

    PlaylistItem* childItem = parentItem->child(row);
    if(childItem) {
        return createIndex(row, column, childItem);
    }
    return {};
}

QModelIndex PlaylistModel::parent(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return {};
    }

    auto* childItem = static_cast<PlaylistItem*>(index.internalPointer());
    PlaylistItem* parentItem = childItem->parent();

    if(parentItem == m_root.get()) {
        return {};
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    PlaylistItem* parentItem;
    if(parent.column() > 0) {
        return 0;
    }

    if(!parent.isValid()) {
        parentItem = m_root.get();
    }
    else {
        parentItem = static_cast<PlaylistItem*>(parent.internalPointer());
    }

    return parentItem->childCount();
}

int PlaylistModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid()) {
        return static_cast<PlaylistItem*>(parent.internalPointer())->columnCount();
    }
    return m_root->columnCount();
}

void PlaylistModel::changeTrackState()
{
    emit dataChanged({}, {}, {ItemRole::Playing});
}

void PlaylistModel::changeRowColours()
{
    emit dataChanged({}, {}, {Qt::BackgroundRole});
}

void PlaylistModel::reset()
{
    beginResetModel();
    beginReset();
    setupModelData();
    endResetModel();
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+ItemRole::Id, "ID");
    roles.insert(+ItemRole::Artist, "Artist");
    roles.insert(+ItemRole::Year, "Year");
    roles.insert(+ItemRole::Duration, "Duration");
    roles.insert(+ItemRole::Cover, "Cover");
    roles.insert(+ItemRole::Number, "TrackNumber");
    roles.insert(+ItemRole::PlayCount, "PlayCount");
    roles.insert(+ItemRole::MultiDisk, "Multiple Discs");
    roles.insert(+ItemRole::Playing, "IsPlaying");
    roles.insert(+ItemRole::State, "State");
    roles.insert(+ItemRole::Path, "Path");
    roles.insert(+ItemRole::Index, "Index");
    roles.insert(+ItemRole::Data, "Data");

    return roles;
}

QModelIndexList PlaylistModel::match(const QModelIndex& start, int role, const QVariant& value, int hits,
                                     Qt::MatchFlags flags) const
{
    if(role != ItemRole::Id) {
        return QAbstractItemModel::match(start, role, value, hits, flags);
    }

    QModelIndexList matches;
    QModelIndexList list;

    traverseIndex(start, list);

    for(const auto& index : list) {
        const auto* item = static_cast<PlaylistItem*>(index.internalPointer());
        const auto* track = dynamic_cast<Track*>(item->data());
        if(track->id() == value.toInt()) {
            matches.append(index);
            if(matches.size() == hits) {
                break;
            }
        }
    }
    return matches;
}

void PlaylistModel::traverseIndex(const QModelIndex& start, QModelIndexList& list) const
{
    QModelIndexList stack{start};
    while(!stack.isEmpty()) {
        const QModelIndex parent = stack.takeFirst();
        for(int i = 0; i < rowCount(parent); ++i) {
            const QModelIndex child = index(i, 0, parent);
            if(rowCount(child) > 0) {
                stack.append(child);
            }
            else {
                list.append(child);
            }
        }
    }
}

void PlaylistModel::resetContainers()
{
    //    for (const auto& [key, container]: m_containers)
    //    {
    //        container->reset();
    //    }
    m_containers.clear();
}

void PlaylistModel::beginReset()
{
    resetContainers();
    m_root.reset();
    m_nodes.clear();
    m_root = std::make_unique<PlaylistItem>();
}

PlaylistItem* PlaylistModel::checkInsertKey(const QString& key, PlaylistItem::Type type, LibraryItem* item,
                                            PlaylistItem* parent)
{
    if(!m_nodes.count(key)) {
        auto childItem = std::make_unique<PlaylistItem>(type, item, parent);
        parent->appendChild(childItem.get());
        m_nodes.emplace(key, std::move(childItem));
    }
    PlaylistItem* child = m_nodes.at(key).get();
    return child;
}

void PlaylistModel::createAlbums(const TrackPtrList& tracks)
{
    for(const auto& track : tracks) {
        if(track->isEnabled()) {
            const QString albumKey = QString::number(track->albumId()) + track->album();
            if(!m_containers.count(albumKey)) {
                auto album = std::make_unique<Album>(track->album());
                album->setId(track->albumId());
                album->setYear(track->year());
                album->setArtistId(track->albumArtistId());
                album->setArtist(track->albumArtist());
                album->setCoverPath(track->coverPath());
                m_containers.emplace(albumKey, std::move(album));
            }
            Container* album = m_containers.at(albumKey).get();
            album->addTrack(track);
        }
    }
}

QModelIndex PlaylistModel::indexOfItem(int id)
{
    QModelIndex index;
    const auto key = QString("track%1").arg(id);
    if(m_nodes.count(key)) {
        const auto* item = m_nodes.at(key).get();
        index = createIndex(item->row(), 0, item);
    }
    return index;
}

QModelIndex PlaylistModel::indexOfItem(const PlaylistItem* item)
{
    QModelIndex index;
    if(item) {
        index = createIndex(item->row(), 0, item);
    }
    return index;
}
} // namespace Library
