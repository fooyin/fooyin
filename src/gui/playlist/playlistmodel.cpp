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
#include "playlistpopulator.h"
#include "playlistpreset.h"

#include <core/library/coverprovider.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>

#include <utils/settings/settingsmanager.h>

#include <QIcon>
#include <QPalette>
#include <QThread>

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistModel::Private : public QObject
{
    PlaylistModel* model;
    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;
    Core::Library::CoverProvider coverProvider;

    PlaylistPreset currentPreset;
    Core::Playlist::Playlist currentPlaylist;

    bool altColours;

    bool resetting{false};

    QThread populatorThread;
    PlaylistPopulator populator;

    PlaylistItem root;
    ItemMap nodes;
    ItemMap oldNodes;

    QPixmap playingIcon;
    QPixmap pausedIcon;

    Private(PlaylistModel* model, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : model{model}
        , playerManager{playerManager}
        , settings{settings}
        , altColours{settings->value<Settings::PlaylistAltColours>()}
        , playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
        , pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
    {
        populator.moveToThread(&populatorThread);
        populatorThread.start();
    }

    void populateModel(PendingData& data)
    {
        for(const auto& pair : data.items) {
            nodes.emplace(pair);
        }

        auto insertRow = [](PlaylistItem* parent, PlaylistItem* child) {
            parent->appendChild(child);
            child->setParent(parent);
        };

        for(auto& [parentKey, rows] : data.nodes) {
            PlaylistItem* parent = parentKey.isEmpty() ? &root : &nodes.at(parentKey);
            const int baseRow    = parent->childCount();
            const int nodeCount  = static_cast<int>(rows.size());
            model->beginInsertRows(model->indexForItem(parent), baseRow, baseRow + nodeCount - 1);
            for(const QString& row : rows) {
                PlaylistItem* child = &nodes.at(row);
                insertRow(parent, child);
            }
            model->endInsertRows();
        }
    }

    void beginReset()
    {
        root = PlaylistItem{};
        // Acts as a cache in case model hasn't been fully cleared
        oldNodes = std::move(nodes);
        nodes.clear();
    }

    QVariant trackData(PlaylistItem* item, int role) const
    {
        const auto track = std::get<Track>(item->data());

        switch(role) {
            case(PlaylistItem::Role::Left): {
                return track.left();
            }
            case(PlaylistItem::Role::Right): {
                return track.right();
            }
            case(PlaylistItem::Role::Playing): {
                return playerManager->currentTrack() == track.track();
            }
            case(PlaylistItem::Role::ItemData): {
                return QVariant::fromValue<Core::Track>(track.track());
            }
            case(PlaylistItem::Role::Indentation): {
                return item->indentation();
            }
            case(Qt::BackgroundRole): {
                if(!altColours) {
                    return QPalette::Base;
                }
                return item->row() & 1 ? QPalette::Base : QPalette::AlternateBase;
            }
            case(Qt::SizeHintRole): {
                return QSize{0, currentPreset.track.rowHeight};
            }
            case(Qt::DecorationRole): {
                switch(playerManager->playState()) {
                    case(Core::Player::PlayState::Playing):
                        return playingIcon;
                    case(Core::Player::PlayState::Paused):
                        return pausedIcon;
                    case(Core::Player::PlayState::Stopped):
                    default:
                        return {};
                }
            }
            default:
                return {};
        }
    }

    QVariant headerData(PlaylistItem* item, int role) const
    {
        const auto& header = std::get<Container>(item->data());

        switch(role) {
            case(PlaylistItem::Role::Title): {
                return header.title();
            }
            case(PlaylistItem::Role::ShowCover): {
                return currentPreset.header.showCover;
            }
            case(PlaylistItem::Role::Simple): {
                return currentPreset.header.simple;
            }
            case(PlaylistItem::Role::Cover): {
                if(!currentPreset.header.showCover) {
                    return {};
                }
                return coverProvider.albumThumbnail(header.coverPath());
            }
            case(PlaylistItem::Role::Subtitle): {
                return header.subtitle();
            }
            case(PlaylistItem::Role::Info): {
                return header.info();
            }
            case(PlaylistItem::Role::Right): {
                return header.sideText();
            }
            case(Qt::SizeHintRole): {
                return QSize{0, currentPreset.header.rowHeight};
            }
            default:
                return {};
        }
    }

    QVariant subheaderData(PlaylistItem* item, int role) const
    {
        const auto& header = std::get<Container>(item->data());

        switch(role) {
            case(PlaylistItem::Role::Title): {
                return header.title();
            }
            case(PlaylistItem::Role::Subtitle): {
                return header.info();
            }
            case(PlaylistItem::Role::Indentation): {
                return item->indentation();
            }
            case(Qt::SizeHintRole): {
                return QSize{0, currentPreset.subHeader.rowHeight};
            }
            default:
                return {};
        }
    }
};

PlaylistModel::PlaylistModel(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                             QObject* parent)
    : QAbstractItemModel{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    p->settings->subscribe<Settings::PlaylistAltColours>(this, [this](bool enabled) {
        p->altColours = enabled;
        emit dataChanged({}, {}, {Qt::BackgroundRole});
    });

    QObject::connect(&p->populator, &PlaylistPopulator::populated, this, [this](PendingData data) {
        if(p->resetting) {
            beginResetModel();
            p->beginReset();
        }

        p->populateModel(data);

        if(p->resetting) {
            endResetModel();
        }
        p->resetting = false;
    });
}

PlaylistModel::~PlaylistModel()
{
    p->populator.stopThread();
    p->populatorThread.quit();
    p->populatorThread.wait();
}

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(!p->currentPlaylist.isValid()) {
        return {};
    }
    return QString("%1: %2 Tracks").arg(p->currentPlaylist.name()).arg(p->currentPlaylist.trackCount());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<PlaylistItem*>(index.internalPointer());

    const PlaylistItem::ItemType type = item->type();

    if(role == PlaylistItem::Type) {
        return type;
    }

    switch(type) {
        case(PlaylistItem::Header):
            return p->headerData(item, role);
        case(PlaylistItem::Track):
            return p->trackData(item, role);
        case(PlaylistItem::Subheader):
            return p->subheaderData(item, role);
        case(PlaylistItem::Root):
            return {};
    }
    return {};
}

QModelIndex PlaylistModel::index(int row, int column, const QModelIndex& parent) const
{
    if(!hasIndex(row, column, parent)) {
        return {};
    }

    PlaylistItem* parentItem;

    if(!parent.isValid()) {
        parentItem = &p->root;
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

    auto* childItem          = static_cast<PlaylistItem*>(index.internalPointer());
    PlaylistItem* parentItem = childItem->parent();

    if(parentItem == &p->root || !parentItem) {
        return {};
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    PlaylistItem* parentItem;

    if(!parent.isValid()) {
        parentItem = &p->root;
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
    return p->root.columnCount();
}

QModelIndex PlaylistModel::indexOfItem(const PlaylistItem& item)
{
    return createIndex(item.row(), 0, &item);
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();

    roles.insert(+PlaylistItem::Role::Id, "ID");
    roles.insert(+PlaylistItem::Role::Subtitle, "Subtitle");
    roles.insert(+PlaylistItem::Role::Cover, "Cover");
    roles.insert(+PlaylistItem::Role::Playing, "IsPlaying");
    roles.insert(+PlaylistItem::Role::Path, "Path");
    roles.insert(+PlaylistItem::Role::ItemData, "ItemData");

    return roles;
}

QModelIndex PlaylistModel::matchTrack(int id) const
{
    QModelIndexList stack{};
    while(!stack.isEmpty()) {
        const QModelIndex parent = stack.takeFirst();
        for(int i = 0; i < rowCount(parent); ++i) {
            const QModelIndex child = index(i, 0, parent);
            if(rowCount(child) > 0) {
                stack.append(child);
            }
            else {
                const auto* item = static_cast<PlaylistItem*>(child.internalPointer());
                const auto track = std::get<Track>(item->data());
                if(track.track().id() == id) {
                    return child;
                }
            }
        }
    }
    return {};
}

void PlaylistModel::reset(const Core::Playlist::Playlist& playlist)
{
    if(!playlist.isValid()) {
        return;
    }

    p->resetting       = true;
    p->currentPlaylist = playlist;

    p->populator.stopThread();

    QMetaObject::invokeMethod(&p->populator, [this, playlist] {
        p->populator.run(p->currentPreset, playlist.tracks());
    });
}

void PlaylistModel::changeTrackState()
{
    emit dataChanged({}, {}, {PlaylistItem::Role::Playing});
}

void PlaylistModel::changePreset(const PlaylistPreset& preset)
{
    p->currentPreset = preset;
}

QModelIndex PlaylistModel::indexForTrack(const Core::Track& track) const
{
    QModelIndex index;
    const auto key = track.hash();
    if(p->nodes.contains(key)) {
        PlaylistItem* item = &p->nodes.at(key);
        index              = createIndex(item->row(), 0, item);
    }
    return index;
}

QModelIndex PlaylistModel::indexForItem(PlaylistItem* item) const
{
    if(!item) {
        return {};
    }
    return createIndex(item->row(), 0, item);
}
} // namespace Fy::Gui::Widgets::Playlist
