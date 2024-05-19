/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistpopulator.h"

#include "playlistpreset.h"
#include "playlistscriptregistry.h"

#include <core/player/playercontroller.h>
#include <utils/crypto.h>

#include <QCryptographicHash>
#include <QTimer>

#include <ranges>

constexpr int TrackPreloadSize = 2000;

namespace Fooyin {
struct PlaylistPopulator::Private
{
    PlaylistPopulator* self;
    PlayerController* playerController;

    PlaylistPreset currentPreset;
    PlaylistColumnList columns;

    std::unique_ptr<PlaylistScriptRegistry> registry;
    ScriptParser parser;

    ScriptFormatter formatter;

    int trackDepth{0};
    QString prevBaseHeaderKey;
    QString prevHeaderKey;
    std::vector<QString> prevBaseSubheaderKey;
    std::vector<QString> prevSubheaderKey;

    std::vector<PlaylistContainerItem> subheaders;

    PlaylistItem root;
    PendingData data;
    ContainerKeyMap headers;
    TrackList pendingTracks;

    explicit Private(PlaylistPopulator* self_, PlayerController* playerController_)
        : self{self_}
        , playerController{playerController_}
        , registry{std::make_unique<PlaylistScriptRegistry>()}
        , parser{registry.get()}
    { }

    void reset()
    {
        data.clear();
        headers.clear();
        trackDepth = 0;
        prevBaseSubheaderKey.clear();
        prevSubheaderKey.clear();
        prevBaseHeaderKey.clear();
        prevHeaderKey.clear();
    }

    PlaylistItem* getOrInsertItem(const QString& key, PlaylistItem::ItemType type, const Data& item,
                                  PlaylistItem* parent, const QString& baseKey)
    {
        auto [node, inserted] = data.items.try_emplace(key, PlaylistItem{type, item, parent});
        if(inserted) {
            node->second.setBaseKey(baseKey);
            node->second.setKey(key);
        }
        PlaylistItem* child = &node->second;

        if(!child->pending()) {
            child->setPending(true);
            data.nodes[parent->key()].push_back(key);
            if(type != PlaylistItem::Track) {
                data.containerOrder.push_back(key);
            }
        }
        return child;
    }

    void updateContainers()
    {
        for(const auto& [key, container] : headers) {
            container->updateGroupText(&parser, &formatter);
        }
    }

    void iterateHeader(const Track& track, PlaylistItem*& parent)
    {
        HeaderRow row{currentPreset.header};
        if(!row.isValid()) {
            return;
        }

        auto evaluateBlocks = [this, track](RichScript& script) -> QString {
            script.text.clear();
            const auto evalScript = parser.evaluate(script.script, track);
            if(!evalScript.isEmpty()) {
                script.text = formatter.evaluate(evalScript);
            }
            return evalScript;
        };

        auto generateHeaderKey = [&row, &evaluateBlocks]() {
            return Utils::generateHash(evaluateBlocks(row.title), evaluateBlocks(row.subtitle),
                                       evaluateBlocks(row.sideText), evaluateBlocks(row.info));
        };

        const QString baseKey = generateHeaderKey();
        QString key           = Utils::generateRandomHash();
        if(!prevHeaderKey.isEmpty() && prevBaseHeaderKey == baseKey) {
            key = prevHeaderKey;
        }
        prevBaseHeaderKey = baseKey;
        prevHeaderKey     = key;

        if(!headers.contains(key)) {
            PlaylistContainerItem header{currentPreset.header.simple};
            header.setTitle(row.title);
            header.setSubtitle(row.subtitle);
            header.setSideText(row.sideText);
            header.setInfo(row.info);
            header.setRowHeight(row.rowHeight);
            header.calculateSize();

            auto* headerItem      = getOrInsertItem(key, PlaylistItem::Header, header, parent, baseKey);
            auto& headerContainer = std::get<1>(headerItem->data());
            headers.emplace(key, &headerContainer);
        }
        PlaylistContainerItem* header = headers.at(key);
        header->addTrack(track);
        data.trackParents[track.id()].push_back(key);

        auto* headerItem = &data.items.at(key);
        parent           = headerItem;
        ++trackDepth;
    }

    void iterateSubheaders(const Track& track, PlaylistItem*& parent)
    {
        for(auto& subheader : currentPreset.subHeaders) {
            const auto leftScript    = parser.evaluate(subheader.leftText.script, track);
            subheader.leftText.text  = formatter.evaluate(leftScript);
            const auto rightScript   = parser.evaluate(subheader.rightText.script, track);
            subheader.rightText.text = formatter.evaluate(rightScript);

            PlaylistContainerItem currentContainer{false};
            currentContainer.setTitle(subheader.leftText);
            currentContainer.setSubtitle(subheader.rightText);
            currentContainer.setRowHeight(subheader.rowHeight);
            currentContainer.calculateSize();
            subheaders.push_back(currentContainer);
        }

        const int subheaderCount = static_cast<int>(subheaders.size());
        prevSubheaderKey.resize(subheaderCount);
        prevBaseSubheaderKey.resize(subheaderCount);

        auto generateSubheaderKey = [](const PlaylistContainerItem& subheader) {
            QString subheaderKey;
            for(const auto& block : subheader.title().text) {
                subheaderKey += block.text;
            }
            for(const auto& block : subheader.subtitle().text) {
                subheaderKey += block.text;
            }
            return subheaderKey;
        };

        for(int i{0}; const auto& subheader : subheaders) {
            const QString subheaderKey = generateSubheaderKey(subheader);

            if(subheaderKey.isEmpty()) {
                prevBaseSubheaderKey[i].clear();
                prevSubheaderKey[i].clear();
                continue;
            }

            const QString baseKey = Utils::generateHash(parent->baseKey(), subheaderKey);
            QString key           = Utils::generateRandomHash();
            if(static_cast<int>(prevSubheaderKey.size()) > i && prevBaseSubheaderKey.at(i) == baseKey) {
                key = prevSubheaderKey.at(i);
            }
            prevBaseSubheaderKey[i] = baseKey;
            prevSubheaderKey[i]     = key;

            if(!headers.contains(key)) {
                auto* subheaderItem      = getOrInsertItem(key, PlaylistItem::Subheader, subheader, parent, baseKey);
                auto& subheaderContainer = std::get<1>(subheaderItem->data());
                headers.emplace(key, &subheaderContainer);
            }
            PlaylistContainerItem* subheaderContainer = headers.at(key);
            subheaderContainer->addTrack(track);
            data.trackParents[track.id()].push_back(key);

            auto* subheaderItem = &data.items.at(key);
            parent              = subheaderItem;
            ++i;
            ++trackDepth;
        }
        subheaders.clear();
    }

    PlaylistItem* iterateTrack(const Track& track, int index)
    {
        PlaylistItem* parent = &root;

        iterateHeader(track, parent);
        iterateSubheaders(track, parent);

        if(!currentPreset.track.isValid()) {
            return nullptr;
        }

        auto evaluateTrack = [this, &track](RichScript& script) {
            script.text.clear();
            const auto evalScript = parser.evaluate(script.script, track);
            if(!evalScript.isEmpty()) {
                script.text = formatter.evaluate(evalScript);
            }
        };

        registry->setTrackProperties(index, trackDepth);

        TrackRow trackRow{currentPreset.track};
        PlaylistTrackItem playlistTrack;

        if(!columns.empty()) {
            for(const auto& column : columns) {
                const auto evalScript = parser.evaluate(column.field, track);
                trackRow.columns.emplace_back(column.field, formatter.evaluate(evalScript));
            }
            playlistTrack = {trackRow.columns, track};
        }
        else {
            evaluateTrack(trackRow.leftText);
            evaluateTrack(trackRow.rightText);

            playlistTrack = {trackRow.leftText, trackRow.rightText, track};
        }

        playlistTrack.setRowHeight(trackRow.rowHeight);
        playlistTrack.setDepth(trackDepth);
        playlistTrack.calculateSize();

        const QString baseKey = Utils::generateHash(parent->key(), track.hash(), QString::number(index));
        const QString key     = Utils::generateRandomHash();

        auto* trackItem = getOrInsertItem(key, PlaylistItem::Track, playlistTrack, parent, baseKey);
        data.trackParents[track.id()].push_back(key);

        trackDepth = 0;
        return trackItem;
    }

    void runBatch(int size, int index)
    {
        if(size <= 0) {
            return;
        }

        auto tracksBatch = std::ranges::views::take(pendingTracks, size);

        for(const Track& track : tracksBatch) {
            if(!self->mayRun()) {
                return;
            }
            iterateTrack(track, index++);
        }

        updateContainers();

        if(!self->mayRun()) {
            return;
        }

        emit self->populated(data);

        auto tracksToKeep = std::ranges::views::drop(pendingTracks, size);
        TrackList tempTracks;
        std::ranges::copy(tracksToKeep, std::back_inserter(tempTracks));
        pendingTracks = std::move(tempTracks);

        data.nodes.clear();

        const auto remaining = static_cast<int>(pendingTracks.size());
        runBatch(remaining, index);
    }

    void runTracksGroup(const std::map<int, TrackList>& tracks)
    {
        for(const auto& [index, trackGroup] : tracks) {
            std::vector<QString> trackKeys;

            int trackIndex{index};

            for(const Track& track : trackGroup) {
                if(!self->mayRun()) {
                    return;
                }
                if(const auto* trackItem = iterateTrack(track, trackIndex++)) {
                    trackKeys.push_back(trackItem->key());
                }
            }
            data.indexNodes.emplace(index, trackKeys);
        }

        updateContainers();

        if(!self->mayRun()) {
            return;
        }

        emit self->populatedTrackGroup(data);
    }
};

PlaylistPopulator::PlaylistPopulator(PlayerController* playerController, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<Private>(this, playerController)}
{
    qRegisterMetaType<PendingData>();
}

void PlaylistPopulator::run(const Id& playlistId, const PlaylistPreset& preset, const PlaylistColumnList& columns,
                            const TrackList& tracks)
{
    setState(Running);

    p->reset();

    p->data.playlistId = playlistId;
    p->currentPreset   = preset;
    p->columns         = columns;
    p->pendingTracks   = tracks;
    p->registry->setup(playlistId, p->playerController->playbackQueue());

    p->runBatch(TrackPreloadSize, 0);

    emit finished();

    setState(Idle);
}

void PlaylistPopulator::runTracks(const Id& playlistId, const PlaylistPreset& preset, const PlaylistColumnList& columns,
                                  const std::map<int, TrackList>& tracks)
{
    setState(Running);

    p->reset();

    p->data.playlistId = playlistId;
    p->currentPreset   = preset;
    p->columns         = columns;
    p->registry->setup(playlistId, p->playerController->playbackQueue());

    p->runTracksGroup(tracks);

    setState(Idle);
}

void PlaylistPopulator::updateTracks(const Id& playlistId, const PlaylistPreset& preset,
                                     const PlaylistColumnList& columns, const TrackItemMap& tracks)
{
    setState(Running);

    p->currentPreset = preset;
    p->registry->setup(playlistId, p->playerController->playbackQueue());

    ItemList updatedTracks;

    for(const auto& [track, item] : tracks) {
        PlaylistTrackItem& trackData = std::get<0>(item.data());

        auto evaluateTrack = [this, &track](RichScript& script) {
            script.text.clear();
            const auto evalScript = p->parser.evaluate(script.script, track);
            if(!evalScript.isEmpty()) {
                script.text = p->formatter.evaluate(evalScript);
            }
        };

        p->registry->setTrackProperties(item.index(), trackData.depth());

        if(!columns.empty()) {
            std::vector<RichScript> trackColumns;
            for(const auto& column : columns) {
                const auto evalScript = p->parser.evaluate(column.field, track);
                trackColumns.emplace_back(column.field, p->formatter.evaluate(evalScript));
            }
            trackData.setColumns(trackColumns);
        }
        else {
            RichScript trackLeft{preset.track.leftText};
            RichScript trackRight{preset.track.rightText};

            evaluateTrack(trackLeft);
            evaluateTrack(trackRight);

            trackData.setLeftRight(trackLeft, trackRight);
        }

        updatedTracks.push_back(item);
    }

    emit tracksUpdated(updatedTracks);

    setState(Idle);
}

void PlaylistPopulator::updateHeaders(const ItemList& headers)
{
    setState(Running);

    ItemKeyMap updatedHeaders;

    for(const PlaylistItem& item : headers) {
        PlaylistContainerItem& header = std::get<1>(item.data());
        header.updateGroupText(&p->parser, &p->formatter);
        updatedHeaders.emplace(item.key(), item);
    }

    emit headersUpdated(updatedHeaders);

    setState(Idle);
}

PlaylistPopulator::~PlaylistPopulator() = default;
} // namespace Fooyin

#include "moc_playlistpopulator.cpp"
