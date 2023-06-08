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
#include "presetfwd.h"

#include <core/constants.h>
#include <core/library/coverprovider.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlisthandler.h>
#include <core/scripting/scriptparser.h>

#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QIcon>
#include <QPalette>

namespace Fy::Gui::Widgets::Playlist {
QString trackArtistString(const Core::Track& track)
{
    QString artistString;
    for(const QString& artist : track.artists()) {
        if(artist != track.albumArtist()) {
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

using PlaylistItemHash = std::unordered_map<QString, std::unique_ptr<PlaylistItem>>;

struct PlaylistModel::Private
{
    PlaylistModel* model;
    Core::Player::PlayerManager* playerManager;
    Utils::SettingsManager* settings;
    Core::Library::CoverProvider coverProvider;

    PlaylistPreset currentPreset;
    Core::Playlist::Playlist* currentPlaylist;

    bool altColours;

    bool resetting;

    Core::Scripting::Parser parser;

    PlaylistItemHash nodes;
    ContainerHashMap headers;
    Core::ContainerHash containers;
    QPixmap playingIcon;
    QPixmap pausedIcon;

    Core::Scripting::ParsedScript headerTitleScript;
    Core::Scripting::ParsedScript headerSubtitleScript;
    Core::Scripting::ParsedScript headerRightTextScript;
    Core::Scripting::ParsedScript subheadersScript;
    Core::Scripting::ParsedScript trackLeftScript;
    Core::Scripting::ParsedScript trackRightScript;

    Private(PlaylistModel* model, Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings)
        : model{model}
        , playerManager{playerManager}
        , settings{settings}
        , currentPlaylist{nullptr}
        , altColours{settings->value<Settings::PlaylistAltColours>()}
        , resetting{false}
        , playingIcon{QIcon::fromTheme(Constants::Icons::Play).pixmap(20)}
        , pausedIcon{QIcon::fromTheme(Constants::Icons::Pause).pixmap(20)}
    { }

    PlaylistItem* checkInsertKey(const QString& key, PlaylistItem::ItemType type, const Data& item,
                                 PlaylistItem* parent)
    {
        if(!nodes.count(key)) {
            auto* node = nodes.emplace(key, std::make_unique<PlaylistItem>(type, item, parent)).first->second.get();
            node->setKey(key);
        }
        PlaylistItem* child = nodes.at(key).get();
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
        headerTitleScript     = parser.parse(currentPreset.header.title);
        headerSubtitleScript  = parser.parse(currentPreset.header.subtitle);
        headerRightTextScript = parser.parse(currentPreset.header.sideText);
        subheadersScript      = parser.parse(currentPreset.subHeader.title);
        trackLeftScript       = parser.parse(currentPreset.track.leftText);
        trackRightScript      = parser.parse(currentPreset.track.rightText);
    }

    void iterateHeader(const Core::Track& track, PlaylistItem*& parent)
    {
        const QString headerTitle     = parser.evaluate(headerTitleScript, track);
        const QString headerSubtitle  = parser.evaluate(headerSubtitleScript, track);
        const QString headerRightText = parser.evaluate(headerRightTextScript, track);

        if(headerTitle.isEmpty() && headerSubtitle.isEmpty() && headerRightText.isEmpty()) {
            return;
        }

        const QString headerKey = QString{"%1%2%3%4"}.arg(parent->key(), headerTitle, headerSubtitle, headerRightText);

        auto [header, inserted] = headers.try_emplace(headerKey);
        if(inserted) {
            header->second
                = std::make_unique<Header>(headerTitle, headerSubtitle, headerRightText, track.thumbnailPath());
            checkInsertKey(headerKey, PlaylistItem::Header, header->second.get(), parent);
        }
        header->second->addTrack(track);

        parent = nodes.at(headerKey).get();
    }

    void iterateSubheaders(const Core::Track& track, PlaylistItem*& parent)
    {
        const QString subheaders = parser.evaluate(subheadersScript, track);

        if(subheaders.isEmpty()) {
            return;
        }

        const QStringList values = subheaders.split(Core::Constants::Separator);
        for(const QString& value : values) {
            if(value.isNull()) {
                continue;
            }
            const QStringList levels = value.split("||");
            for(auto [it, end, i] = std::tuple{levels.cbegin(), levels.cend(), 0}; it != end; ++it, ++i) {
                const QString title = *it;
                const QString key   = QString{"%1%2%3"}.arg(parent->key(), title).arg(i);

                auto [subheader, inserted] = headers.try_emplace(key);
                if(inserted) {
                    subheader->second = std::make_unique<Subheader>(title);
                    checkInsertKey(key, PlaylistItem::Subheader, subheader->second.get(), parent);
                }
                subheader->second->addTrack(track);

                parent = nodes.at(key).get();
                if(parent->parent()->type() != PlaylistItem::Header) {
                    parent->setIndentation(parent->parent()->indentation() + 20);
                }
            }
        }
    }

    void iterateTrack(const Core::Track& track)
    {
        PlaylistItem* parent = model->rootItem();

        iterateHeader(track, parent);
        iterateSubheaders(track, parent);

        const QString trackLeft  = parser.evaluate(trackLeftScript, track);
        const QString trackRight = parser.evaluate(trackRightScript, track);

        const QString key = QString{"%1%2"}.arg(parent->key(), track.hash());

        Track playlistTrack{track, trackLeft.split("||"), trackRight.split("||")};
        auto* trackItem = checkInsertKey(key, PlaylistItem::Track, playlistTrack, parent);

        if(parent->type() != PlaylistItem::Header) {
            trackItem->setIndentation(parent->indentation() + 20);
        }
    }

    QVariant trackData(PlaylistItem* item, int role) const
    {
        const auto track = std::get<Track>(item->data());

        switch(role) {
            case(Qt::DisplayRole): {
                return track.leftSide();
            }
            case(PlaylistItem::Role::RightText): {
                return track.rightSide();
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
            case(PlaylistItem::Role::LeftFont): {
                return currentPreset.track.leftTextFont;
            }
            case(PlaylistItem::Role::RightFont): {
                return currentPreset.track.rightTextFont;
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
                    case Core::Player::Stopped:
                        break;
                }
            }
        }
        return {};
    }

    QVariant headerData(PlaylistItem* item, int role) const
    {
        auto* header = static_cast<Header*>(std::get<Container*>(item->data()));

        if(!header) {
            return {};
        }

        switch(role) {
            case(Qt::DisplayRole): {
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
            case(PlaylistItem::Role::RightText): {
                return header->rightText();
            }
            case(PlaylistItem::Role::TitleFont): {
                return currentPreset.header.titleFont;
            }
            case(PlaylistItem::Role::SubtitleFont): {
                return currentPreset.header.subtitleFont;
            }
            case(PlaylistItem::Role::RightTextFont): {
                return currentPreset.header.sideTextFont;
            }
            case(PlaylistItem::Role::InfoFont): {
                return currentPreset.header.infoFont;
            }
            case(Qt::SizeHintRole): {
                return QSize{0, currentPreset.header.rowHeight};
            }
        }
        return {};
    }

    QVariant subheaderData(PlaylistItem* item, int role) const
    {
        auto* header = static_cast<Subheader*>(std::get<Container*>(item->data()));

        if(!header) {
            return {};
        }

        switch(role) {
            case(Qt::DisplayRole): {
                return header->title();
            }
            case(PlaylistItem::Role::Duration): {
                auto duration = static_cast<int>(header->duration());
                return QString(Utils::msToString(duration));
            }
            case(PlaylistItem::Role::Indentation): {
                return item->indentation();
            }
            case(PlaylistItem::Role::TitleFont): {
                return currentPreset.subHeader.titleFont;
            }
            case(PlaylistItem::Role::RightTextFont): {
                return currentPreset.subHeader.rightFont;
            }
            case(Qt::SizeHintRole): {
                return QSize{0, currentPreset.subHeader.rowHeight};
            }
        }
        return {};
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
    roles.insert(+PlaylistItem::Role::RightText, "RightText");
    roles.insert(+PlaylistItem::Role::Duration, "Duration");
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
    if(p->nodes.count(key)) {
        const auto* item = p->nodes.at(key).get();
        index            = createIndex(item->row(), 0, item);
    }
    return index;
}

QModelIndex PlaylistModel::indexForItem(PlaylistItem* item) const
{
    QModelIndex index;
    const auto key = item->key();
    if(p->nodes.count(key)) {
        auto* node = p->nodes.at(key).get();
        index      = createIndex(node->row(), 0, node);
    }
    return index;
}
} // namespace Fy::Gui::Widgets::Playlist
