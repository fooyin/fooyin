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

#include "playlistmodel.h"

#include "gui/guiconstants.h"
#include "gui/guisettings.h"
#include "playlistitem.h"

#include <core/library/coverprovider.h>
#include <core/library/musiclibrary.h>
#include <core/models/disc.h>
#include <core/player/playermanager.h>

#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QPalette>

namespace Fy::Gui::Widgets {
QString trackArtistString(Core::Track* track)
{
    QString artistString;
    for(const auto& artist : track->artists()) {
        if(artist != track->albumArtist()) {
            if(!artistString.isEmpty()) {
                artistString += ", ";
            }
            artistString += artist;
        }
    }
    if(!artistString.isEmpty()) {
        artistString.prepend("  \u2022  ");
    }
    return artistString;
}

PlaylistModel::PlaylistModel(Core::Player::PlayerManager* playerManager, Core::Library::MusicLibrary* library,
                             Utils::SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , m_playerManager{playerManager}
    , m_library{library}
    , m_settings{settings}
    , m_discHeaders{m_settings->value<Settings::DiscHeaders>()}
    , m_splitDiscs{m_settings->value<Settings::SplitDiscs>()}
    , m_altColours{m_settings->value<Settings::PlaylistAltColours>()}
    , m_simplePlaylist{m_settings->value<Settings::SimplePlaylist>()}
    , m_resetting{false}
    , m_playingIcon{Constants::Icons::Play}
    , m_pausedIcon{Constants::Icons::Pause}
{
    setupModelData();

    m_settings->subscribe<Settings::DiscHeaders>(this, [this](bool enabled) {
        m_discHeaders = enabled;
    });
    m_settings->subscribe<Settings::SplitDiscs>(this, [this](bool enabled) {
        m_splitDiscs = enabled;
    });
    m_settings->subscribe<Settings::PlaylistAltColours>(this, [this](bool enabled) {
        m_altColours = enabled;
    });
    m_settings->subscribe<Settings::SimplePlaylist>(this, [this](bool enabled) {
        m_simplePlaylist = enabled;
    });
    connect(m_library, &Core::Library::MusicLibrary::tracksLoaded, this, &PlaylistModel::setupModelData);
    connect(m_library, &Core::Library::MusicLibrary::tracksChanged, this, &PlaylistModel::reset);
    connect(m_library, &Core::Library::MusicLibrary::tracksDeleted, this, &PlaylistModel::reset);
    connect(m_library, &Core::Library::MusicLibrary::tracksAdded, this, &PlaylistModel::setupModelData);
    connect(m_library, &Core::Library::MusicLibrary::libraryRemoved, this, &PlaylistModel::reset);
    connect(m_library, &Core::Library::MusicLibrary::libraryChanged, this, &PlaylistModel::reset);

    m_playingIcon = m_playingIcon.scaled({20, 20}, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_pausedIcon  = m_pausedIcon.scaled({20, 20}, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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

    return QString("%1 Tracks").arg(static_cast<int>(m_tracks.size()));
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return {};
    }

    auto* item = static_cast<PlaylistItem*>(index.internalPointer());

    const PlaylistItem::Type type = item->type();

    if(role == Playlist::Mode) {
        return m_simplePlaylist;
    }

    if(role == Playlist::Type) {
        return QVariant::fromValue<PlaylistItem::Type>(type);
    }

    switch(type) {
        case(PlaylistItem::Album):
            return albumData(item, role);
        case(PlaylistItem::Track):
            return trackData(item, role);
        case(PlaylistItem::Disc):
            return discData(item, role);
        case(PlaylistItem::Container):
        case(PlaylistItem::Root):
            return {};
    }
    return {};
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+PlaylistItem::Role::Id, "ID");
    roles.insert(+PlaylistItem::Role::Artist, "Artist");
    roles.insert(+PlaylistItem::Role::Year, "Year");
    roles.insert(+PlaylistItem::Role::Duration, "Duration");
    roles.insert(+PlaylistItem::Role::Cover, "Cover");
    roles.insert(+PlaylistItem::Role::Number, "TrackNumber");
    roles.insert(+PlaylistItem::Role::PlayCount, "PlayCount");
    roles.insert(+PlaylistItem::Role::MultiDisk, "Multiple Discs");
    roles.insert(+PlaylistItem::Role::Playing, "IsPlaying");
    roles.insert(+PlaylistItem::Role::Path, "Path");
    roles.insert(+PlaylistItem::Role::Data, "Data");

    return roles;
}

QModelIndexList PlaylistModel::match(const QModelIndex& start, int role, const QVariant& value, int hits,
                                     Qt::MatchFlags flags) const
{
    if(role != PlaylistItem::Role::Id) {
        return QAbstractItemModel::match(start, role, value, hits, flags);
    }

    QModelIndexList matches;
    QModelIndexList list;

    traverseIndex(start, list);

    for(const auto& index : list) {
        const auto* item  = static_cast<PlaylistItem*>(index.internalPointer());
        const auto* track = static_cast<Core::Track*>(item->data());
        if(track->id() == value.toInt()) {
            matches.append(index);
            if(matches.size() == hits) {
                break;
            }
        }
    }
    return matches;
}

Core::TrackPtrList PlaylistModel::tracks() const
{
    return m_tracks;
}

void PlaylistModel::reset()
{
    m_resetting = true;
    beginResetModel();
    beginReset();
    setupModelData();
    endResetModel();
    m_resetting = false;
}

void PlaylistModel::changeRowColours()
{
    emit dataChanged({}, {}, {Qt::BackgroundRole});
}

void PlaylistModel::changeTrackState()
{
    emit dataChanged({}, {}, {PlaylistItem::Role::Playing});
}

QModelIndex PlaylistModel::indexForId(int id) const
{
    QModelIndex index;
    const auto key = QString("track%1").arg(id);
    if(m_nodes.count(key)) {
        const auto* item = m_nodes.at(key).get();
        index            = createIndex(item->row(), 0, item);
    }
    return index;
}

QModelIndex PlaylistModel::indexForItem(PlaylistItem* item) const
{
    QModelIndex index;
    const auto key = item->key();
    if(m_nodes.count(key)) {
        const auto* item = m_nodes.at(key).get();
        index            = createIndex(item->row(), 0, item);
    }
    return index;
}

int PlaylistModel::findTrackIndex(Core::Track* track) const
{
    return Utils::findIndex(m_tracks, track);
}

void PlaylistModel::setupModelData()
{
    if(!m_library) {
        return;
    }
    m_tracks = m_library->tracks();

    if(m_tracks.empty()) {
        return;
    }

    // Create albums before model to ensure discs (based on discCount) are properly created
    createAlbums(m_tracks);

    for(const auto& track : m_tracks) {
        if(track && track->isEnabled() && !m_nodes.count(track->uid())) {
            if(auto* parent = iterateTrack(track, m_discHeaders, m_splitDiscs)) {
                checkInsertKey(track->uid(), PlaylistItem::Track, track, parent);
            }
        }
    }
}

void PlaylistModel::createAlbums(const Core::TrackPtrList& tracks)
{
    for(const auto& track : tracks) {
        if(track->isEnabled() && !m_nodes.count(track->uid())) {
            const QString albumKey = QString::number(track->year()) + track->album();
            if(!m_containers.count(albumKey)) {
                auto album = std::make_unique<Core::Album>(track->album());
                album->setDate(track->date());
                album->setArtist(track->albumArtist());
                album->setCoverPath(track->coverPath());
                m_containers.emplace(albumKey, std::move(album));
            }
            Core::Container* album = m_containers.at(albumKey).get();
            album->addTrack(track);
        }
    }
}

PlaylistItem* PlaylistModel::iterateTrack(Core::Track* track, bool discHeaders, bool splitDiscs)
{
    PlaylistItem* parent = nullptr;

    const QString albumKey = QString::number(track->year()) + track->album();
    const QString discKey  = albumKey + QString::number(track->discNumber());

    if(m_containers.count(albumKey) == 1) {
        auto* album           = static_cast<Core::Album*>(m_containers.at(albumKey).get());
        const bool singleDisk = album->isSingleDiscAlbum() || (!splitDiscs && !discHeaders);

        if(singleDisk) {
            parent = checkInsertKey(albumKey, PlaylistItem::Album, album, rootItem());
        }

        else if(splitDiscs) {
            if(!m_containers.count(discKey)) {
                auto discAlbum          = std::make_unique<Core::Album>(*album);
                const QString discTitle = "Disc #" + QString::number(track->discNumber());
                discAlbum->setSubTitle(discTitle);
                discAlbum->reset();
                checkInsertKey(discKey, PlaylistItem::Album, discAlbum.get(), rootItem());
                m_containers.emplace(discKey, std::move(discAlbum));
            }
            Core::Container* discAlbum = m_containers.at(discKey).get();
            discAlbum->addTrack(track);
            parent = m_nodes.at(discKey).get();
        }

        else {
            if(!m_containers.count(discKey)) {
                auto disc                = std::make_unique<Core::Disc>(track->discNumber());
                PlaylistItem* parentNode = checkInsertKey(albumKey, PlaylistItem::Album, album, rootItem());
                checkInsertKey(discKey, PlaylistItem::Disc, disc.get(), parentNode);
                m_containers.emplace(discKey, std::move(disc));
            }
            Core::Container* disc = m_containers.at(discKey).get();
            disc->addTrack(track);
            parent = m_nodes.at(discKey).get();
        }
    }
    return parent;
}

PlaylistItem* PlaylistModel::checkInsertKey(const QString& key, PlaylistItem::Type type, Core::MusicItem* item,
                                            PlaylistItem* parent)
{
    if(!m_nodes.count(key)) {
        auto* node = m_nodes.emplace(key, std::make_unique<PlaylistItem>(type, item, parent)).first->second.get();
        node->setKey(key);
    }
    PlaylistItem* child = m_nodes.at(key).get();
    if(Utils::contains(parent->children(), child)) {
        return child;
    }
    if(m_resetting) {
        parent->appendChild(child);
    }
    else {
        insertRow(parent, child);
    }
    return child;
}

void PlaylistModel::insertRow(PlaylistItem* parent, PlaylistItem* child)
{
    const int row = parent->childCount();
    beginInsertRows(indexForItem(parent), row, row);
    parent->appendChild(child);
    endInsertRows();
}

void PlaylistModel::beginReset()
{
    m_tracks.clear();
    m_containers.clear();
    m_nodes.clear();
    resetRoot();
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

QVariant PlaylistModel::trackData(PlaylistItem* item, int role) const
{
    auto* track = static_cast<Core::Track*>(item->data());

    if(!track) {
        return {};
    }

    switch(role) {
        case(PlaylistItem::Role::Id): {
            return track->id();
        }
        case(PlaylistItem::Role::Number): {
            return QStringLiteral("%1").arg(track->trackNumber(), 2, 10, QLatin1Char('0'));
        }
        case(Qt::DisplayRole): {
            return !track->title().isEmpty() ? track->title() : "Unknown Title";
        }
        case(PlaylistItem::Role::Artist): {
            return trackArtistString(track);
        }
        case(PlaylistItem::Role::PlayCount): {
            const int count = track->playCount();
            if(count > 0) {
                return QString::number(count) + QString("|");
            }
            return {};
        }
        case(PlaylistItem::Role::Duration): {
            return Utils::msToString(track->duration());
        }
        case(PlaylistItem::Role::MultiDisk): {
            return item->parent()->type() != PlaylistItem::Type::Album && m_discHeaders && !m_splitDiscs;
        }
        case(PlaylistItem::Role::Playing): {
            return m_playerManager->currentTrack() && m_playerManager->currentTrack()->id() == track->id();
        }
        case(PlaylistItem::Role::Path): {
            return track->filepath();
        }
        case(PlaylistItem::Role::Data): {
            return QVariant::fromValue<Core::Track*>(track);
        }
        case(Qt::BackgroundRole): {
            return m_altColours && !(item->row() & 1) ? QPalette::AlternateBase : QPalette::Base;
        }
        case(Qt::DecorationRole): {
            switch(m_playerManager->playState()) {
                case(Core::Player::PlayState::Playing):
                    return m_playingIcon;
                case(Core::Player::PlayState::Paused):
                    return m_pausedIcon;
                default:
                    break;
            }
            break;
        }
        default: {
            return {};
        }
    }
    return {};
}

QVariant PlaylistModel::albumData(PlaylistItem* item, int role) const
{
    auto* album = static_cast<Core::Album*>(item->data());

    if(!album) {
        return {};
    }

    switch(role) {
        case(PlaylistItem::Role::Id): {
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
        case(PlaylistItem::Role::Cover): {
            return Core::Covers::albumCover(album);
        }
        case(PlaylistItem::Role::Artist): {
            return !album->artist().isEmpty() ? album->artist() : "Unknown Artist";
        }
        case(PlaylistItem::Role::Duration): {
            const auto genre    = album->genres().join(" / ");
            const auto count    = album->trackCount();
            const auto duration = album->duration();

            QString dur = genre;
            if(!genre.isEmpty()) {
                dur += " | ";
            }
            dur += QString(QString::number(count) + (count > 1 ? " Tracks" : " Track") + " | "
                           + Utils::msToString(duration));
            return dur;
        }
        case(PlaylistItem::Role::Year): {
            return album->year();
        }
    }
    return {};
}

QVariant PlaylistModel::discData(PlaylistItem* item, int role) const
{
    auto* disc = static_cast<Core::Disc*>(item->data());

    if(!disc) {
        return {};
    }

    switch(role) {
        case(Qt::DisplayRole): {
            return "Disc #" + QString::number(disc->number());
        }
        case(PlaylistItem::Role::Duration): {
            auto duration = static_cast<int>(disc->duration());
            return QString(Utils::msToString(duration));
        }
        default: {
            return {};
        }
    }
}
} // namespace Fy::Gui::Widgets
