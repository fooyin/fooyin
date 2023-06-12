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
#include "playlistpreset.h"

#include <core/library/coverprovider.h>
#include <core/models/container.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>

#include <utils/settings/settingsmanager.h>

#include <QIcon>
#include <QPalette>
#include <QStringBuilder>

namespace Fy::Gui::Widgets::Playlist {
using PlaylistItemHash = std::unordered_map<QString, std::unique_ptr<PlaylistItem>>;

void parseBlock(Core::Scripting::Parser& parser, TextBlock& block)
{
    block.script = parser.parse(block.text);
}

void parseBlockList(Core::Scripting::Parser& parser, TextBlockList& blocks)
{
    for(TextBlock& block : blocks) {
        parseBlock(parser, block);
    }
}

struct PlaylistModel::Private
{
    PlaylistModel* model;
    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;
    Core::Library::CoverProvider coverProvider;

    PlaylistPreset currentPreset;
    Core::Playlist::Playlist* currentPlaylist{nullptr};

    bool altColours;

    bool resetting{false};

    Core::Scripting::Parser parser;

    PlaylistItemHash nodes;
    ContainerHashMap headers;
    Core::ContainerHash containers;
    QPixmap playingIcon;
    QPixmap pausedIcon;

    Private(PlaylistModel* model, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : model{model}
        , playerManager{playerManager}
        , settings{settings}
        , altColours{settings->value<Settings::PlaylistAltColours>()}
        , playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
        , pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
    { }

    PlaylistItem* checkInsertKey(const QString& key, PlaylistItem::ItemType type, const Data& item,
                                 PlaylistItem* parent)
    {
        auto [node, inserted] = nodes.try_emplace(key, std::make_unique<PlaylistItem>(type, item, parent));
        if(inserted) {
            node->second->setKey(key);
        }
        PlaylistItem* child = node->second.get();

        parent->appendChild(child);
        return child;
    }

    void insertRow(PlaylistItem* parent, PlaylistItem* child)
    {
        const int row = parent->childCount();
        model->beginInsertRows(model->indexForItem(parent), row, row);
        parent->appendChild(child);
        model->endInsertRows();
    }

    void beginReset()
    {
        containers.clear();
        headers.clear();
        nodes.clear();
        model->resetRoot();
    }

    void updateScripts()
    {
        parseBlock(parser, currentPreset.header.title);
        parseBlock(parser, currentPreset.header.subtitle);
        parseBlock(parser, currentPreset.header.sideText);

        parseBlockList(parser, currentPreset.subHeader.text);
        parseBlockList(parser, currentPreset.track.text);
    }

    void iterateHeader(const Core::Track& track, PlaylistItem*& parent)
    {
        HeaderRow headerData{currentPreset.header};
        headerData.title.text    = parser.evaluate(currentPreset.header.title.script, track);
        headerData.subtitle.text = parser.evaluate(currentPreset.header.subtitle.script, track);
        headerData.sideText.text = parser.evaluate(currentPreset.header.sideText.script, track);
        headerData.info.text     = parser.evaluate(currentPreset.header.info.script, track);

        const QString headerKey = headerData.title.text % headerData.subtitle.text % headerData.sideText.text;

        auto [header, inserted] = headers.try_emplace(headerKey);
        if(inserted) {
            header->second = std::make_unique<Header>(headerData.title, headerData.subtitle, headerData.sideText,
                                                      headerData.info, track.thumbnailPath());
            checkInsertKey(headerKey, PlaylistItem::Header, header->second.get(), parent);
        }
        header->second->addTrack(track);

        parent = nodes.at(headerKey).get();
    }

    void iterateSubheaders(const Core::Track& track, PlaylistItem*& parent)
    {
        if(currentPreset.subHeader.text.empty()) {
            return;
        }

        for(const TextBlock& block : currentPreset.subHeader.text) {
            SubheaderRow subheaderLeftData{currentPreset.subHeader};
            SubheaderRow subheaderRightData{currentPreset.subHeader};
            subheaderLeftData.text.clear();
            subheaderRightData.text.clear();

            TextBlock subheaderBlock;
            subheaderBlock.cloneProperties(block);

            const QString result = parser.evaluate(block.script, track);
            if(result.isEmpty()) {
                continue;
            }
            const QString left  = result.section("||", 0, 0);
            const QString right = result.section("||", 1);
            if(!left.isEmpty()) {
                subheaderBlock.text = left;
                subheaderLeftData.text.emplace_back(subheaderBlock);
            }
            if(!right.isEmpty()) {
                subheaderBlock.text = right;
                subheaderRightData.text.emplace_back(subheaderBlock);
            }

            QString key = parent->key();
            for(const auto& str : subheaderLeftData.text) {
                key = key % str.text;
            }

            auto [subheader, inserted] = headers.try_emplace(key);
            if(inserted) {
                subheader->second = std::make_unique<Subheader>(subheaderLeftData.text, subheaderRightData.text);
                checkInsertKey(key, PlaylistItem::Subheader, subheader->second.get(), parent);
            }
            subheader->second->addTrack(track);

            parent = nodes.at(key).get();
            if(parent->parent()->type() != PlaylistItem::Header) {
                parent->setIndentation(parent->parent()->indentation() + 20);
            }
        }
    }

    void iterateTrack(const Core::Track& track)
    {
        PlaylistItem* parent = model->rootItem();

        iterateHeader(track, parent);
        iterateSubheaders(track, parent);

        TrackRow trackLeftData{currentPreset.track};
        TrackRow trackRightData{currentPreset.track};

        trackLeftData.text.clear();
        trackRightData.text.clear();

        bool leftFilled{false};
        for(const TextBlock& block : currentPreset.track.text) {
            TextBlock trackBlock;
            trackBlock.cloneProperties(block);

            const QString result = parser.evaluate(block.script, track);
            if(result.isEmpty()) {
                continue;
            }
            if(result.contains("||") && !leftFilled) {
                leftFilled          = true;
                const QString left  = result.section("||", 0, 0);
                const QString right = result.section("||", 1);
                if(!left.isEmpty()) {
                    trackBlock.text = left;
                    trackLeftData.text.emplace_back(trackBlock);
                }
                if(!right.isEmpty()) {
                    trackBlock.text = right;
                    trackRightData.text.emplace_back(trackBlock);
                }
            }
            else if(!leftFilled) {
                trackBlock.text = result;
                trackLeftData.text.emplace_back(trackBlock);
            }
            else {
                trackBlock.text = result;
                trackRightData.text.emplace_back(trackBlock);
            }
        }

        const QString key = QString{"%1%2"}.arg(parent->key(), track.hash());

        Track playlistTrack{trackLeftData.text, trackRightData.text, track};
        auto* trackItem = checkInsertKey(key, PlaylistItem::Track, playlistTrack, parent);

        if(parent->type() != PlaylistItem::Header) {
            trackItem->setIndentation(parent->indentation() + 20);
        }
    }

    QVariant trackData(PlaylistItem* item, int role) const
    {
        const auto track = std::get<Track>(item->data());

        switch(role) {
            case(PlaylistItem::Role::Left): {
                return QVariant::fromValue(track.left());
            }
            case(PlaylistItem::Role::Right): {
                return QVariant::fromValue(track.right());
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
        auto* header = static_cast<Header*>(std::get<Container*>(item->data()));

        if(!header) {
            return {};
        }

        switch(role) {
            case(PlaylistItem::Role::Title): {
                return header->title();
            }
            case(PlaylistItem::Role::ShowCover): {
                return currentPreset.header.showCover;
            }
            case(PlaylistItem::Role::Simple): {
                return currentPreset.header.simple;
            }
            case(PlaylistItem::Role::Cover): {
                return coverProvider.albumThumbnail(header->coverPath());
            }
            case(PlaylistItem::Role::Subtitle): {
                return header->subtitle();
            }
            case(PlaylistItem::Role::Info): {
                return header->info();
            }
            case(PlaylistItem::Role::Right): {
                return header->sideText();
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
        auto* header = static_cast<Subheader*>(std::get<Container*>(item->data()));

        if(!header) {
            return {};
        }

        switch(role) {
            case(PlaylistItem::Role::Title): {
                return QVariant::fromValue(header->title());
            }
            case(PlaylistItem::Role::Subtitle): {
                return QVariant::fromValue(header->subtitle());
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
    : TreeModel{parent}
    , p{std::make_unique<Private>(this, playerManager, settings)}
{
    p->settings->subscribe<Settings::PlaylistAltColours>(this, [this](bool enabled) {
        p->altColours = enabled;
        emit dataChanged({}, {}, {Qt::BackgroundRole});
    });
}

PlaylistModel::~PlaylistModel() = default;

QVariant PlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(!p->currentPlaylist) {
        return {};
    }
    return QString("%1: %2 Tracks").arg(p->currentPlaylist->name()).arg(p->currentPlaylist->trackCount());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
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

void PlaylistModel::reset(Core::Playlist::Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    p->resetting       = true;
    p->currentPlaylist = playlist;
    beginResetModel();
    p->beginReset();
    setupModelData(playlist);
    endResetModel();
    p->resetting = false;
}

void PlaylistModel::setupModelData(const Core::Playlist::Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    const auto tracks = playlist->tracks();

    for(const Core::Track& track : tracks) {
        if(!track.enabled()) {
            continue;
        }
        p->iterateTrack(track);
    }
}

void PlaylistModel::changeTrackState()
{
    emit dataChanged({}, {}, {PlaylistItem::Role::Playing});
}

void PlaylistModel::changePreset(const PlaylistPreset& preset)
{
    p->currentPreset = preset;
    p->updateScripts();
}

QModelIndex PlaylistModel::indexForTrack(const Core::Track& track) const
{
    QModelIndex index;
    const auto key = track.hash();
    if(p->nodes.contains(key)) {
        const auto* item = p->nodes.at(key).get();
        index            = createIndex(item->row(), 0, item);
    }
    return index;
}

QModelIndex PlaylistModel::indexForItem(PlaylistItem* item) const
{
    QModelIndex index;
    const auto key = item->key();
    if(p->nodes.contains(key)) {
        auto* node = p->nodes.at(key).get();
        index      = createIndex(node->row(), 0, node);
    }
    return index;
}
} // namespace Fy::Gui::Widgets::Playlist
