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
#include "playlistscriptregistry.h"

#include <core/library/coverprovider.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>

#include <utils/settings/settingsmanager.h>

#include <QCryptographicHash>
#include <QIcon>
#include <QPalette>

namespace Fy::Gui::Widgets::Playlist {
using PlaylistItemHash = std::unordered_map<QString, std::unique_ptr<PlaylistItem>>;

QString generateHeaderKey(const QString& titleText, const QString& subtitleText, const QString& sideText)
{
    QCryptographicHash hash{QCryptographicHash::Md5};
    hash.addData(titleText.toUtf8());
    hash.addData(subtitleText.toUtf8());
    hash.addData(sideText.toUtf8());

    QString headerKey = hash.result().toHex();
    return headerKey;
}

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
    Core::Playlist::Playlist currentPlaylist;

    bool altColours;

    bool resetting{false};

    std::unique_ptr<PlaylistScriptRegistry> registry;
    Core::Scripting::Parser parser;

    PlaylistItemHash nodes;
    ContainerHashMap headers;
    QPixmap playingIcon;
    QPixmap pausedIcon;

    Private(PlaylistModel* model, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : model{model}
        , playerManager{playerManager}
        , settings{settings}
        , altColours{settings->value<Settings::PlaylistAltColours>()}
        , registry{std::make_unique<PlaylistScriptRegistry>()}
        , parser{registry.get()}
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
        headers.clear();
        nodes.clear();
        model->resetRoot();
    }

    void updateScripts()
    {
        parseBlock(parser, currentPreset.header.title);
        parseBlock(parser, currentPreset.header.subtitle);
        parseBlock(parser, currentPreset.header.sideText);
        parseBlock(parser, currentPreset.header.info);

        for(auto& row : currentPreset.subHeaders.rows) {
            parseBlock(parser, row.title);
            parseBlock(parser, row.info);
        }

        parseBlockList(parser, currentPreset.track.text);
    }

    void iterateHeader(const Core::Track& track, PlaylistItem*& parent)
    {
        HeaderRow row{currentPreset.header};
        if(!row.isValid()) {
            return;
        }
        row.title.text    = parser.evaluate(currentPreset.header.title.script, track);
        row.subtitle.text = parser.evaluate(currentPreset.header.subtitle.script, track);
        row.sideText.text = parser.evaluate(currentPreset.header.sideText.script, track);

        const QString key = generateHeaderKey(row.title.text, row.subtitle.text, row.sideText.text);

        auto [headerIt, inserted] = headers.try_emplace(key);

        if(inserted) {
            Container header;
            header.setTitle(row.title);
            header.setSubtitle(row.subtitle);
            header.setSideText(row.sideText);
            header.setCoverPath(track.thumbnailPath());
            headerIt->second = std::move(header);

            checkInsertKey(key, PlaylistItem::Header, &headerIt->second, parent);
        }
        Container* header = &headerIt->second;
        header->addTrack(track);

        registry->changeCurrentContainer(header);

        row.info.text = parser.evaluate(currentPreset.header.info.script, track);
        header->modifyInfo(row.info);

        parent = nodes.at(key).get();
    }

    void iterateSubheaders(const Core::Track& track, PlaylistItem*& parent)
    {
        for(const SubheaderRow& row : currentPreset.subHeaders.rows) {
            TextBlock rowLeft{row.title};
            if(!rowLeft.isValid()) {
                return;
            }
            rowLeft.text.clear();

            const QString result = parser.evaluate(rowLeft.script, track);
            if(result.isEmpty()) {
                continue;
            }

            rowLeft.text = result;

            const QString key = generateHeaderKey(parent->key(), rowLeft.text, "");

            auto [subheaderIt, inserted] = headers.try_emplace(key);
            if(inserted) {
                Container subheader;
                subheader.setTitle(rowLeft);
                subheaderIt->second = std::move(subheader);

                checkInsertKey(key, PlaylistItem::Subheader, &subheaderIt->second, parent);
            }
            Container* subheader = &subheaderIt->second;
            subheader->addTrack(track);

            registry->changeCurrentContainer(subheader);

            TextBlock rowRight{row.info};
            rowRight.text.clear();

            const QString rightResult = parser.evaluate(rowRight.script, track);
            if(!rightResult.isEmpty()) {
                rowRight.text = rightResult;
                subheader->modifyInfo(rowRight);
            }

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

        TrackRow trackLeft{currentPreset.track};
        TrackRow trackRight{currentPreset.track};

        if(!trackLeft.isValid() && !trackRight.isValid()) {
            return;
        }

        trackLeft.text.clear();
        trackRight.text.clear();

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
                    trackLeft.text.push_back(trackBlock);
                }
                if(!right.isEmpty()) {
                    trackBlock.text = right;
                    trackRight.text.push_back(trackBlock);
                }
            }
            else if(!leftFilled) {
                trackBlock.text = result;
                trackLeft.text.push_back(trackBlock);
            }
            else {
                trackBlock.text = result;
                trackRight.text.push_back(trackBlock);
            }
        }

        const QString key = QString{"%1%2"}.arg(parent->key(), track.hash());

        Track playlistTrack{trackLeft.text, trackRight.text, track};
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
        auto* header = std::get<Container*>(item->data());

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
        auto* header = std::get<Container*>(item->data());

        if(!header) {
            return {};
        }

        switch(role) {
            case(PlaylistItem::Role::Title): {
                return QVariant::fromValue(header->title());
            }
            case(PlaylistItem::Role::Subtitle): {
                return QVariant::fromValue(header->info());
            }
            case(PlaylistItem::Role::Indentation): {
                return item->indentation();
            }
            case(Qt::SizeHintRole): {
                return QSize{0, currentPreset.subHeaders.rowHeight};
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

    if(!p->currentPlaylist.isValid()) {
        return {};
    }
    return QString("%1: %2 Tracks").arg(p->currentPlaylist.name()).arg(p->currentPlaylist.trackCount());
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

void PlaylistModel::reset(const Core::Playlist::Playlist& playlist)
{
    if(!playlist.isValid()) {
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

void PlaylistModel::setupModelData(const Core::Playlist::Playlist& playlist)
{
    if(!playlist.isValid()) {
        return;
    }

    const auto tracks = playlist.tracks();

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