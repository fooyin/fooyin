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

#include "playlistitemmodels.h"
#include "playlistpreset.h"
#include "scripting/scriptvariableproviders.h"

#include <core/player/playercontroller.h>

#include <QTimer>

#include <ranges>
#include <utility>

namespace Fooyin {
class PlaylistPopulatorPrivate
{
public:
    explicit PlaylistPopulatorPrivate(PlaylistPopulator* self, PlayerController* playerController)
        : m_self{self}
        , m_playerController{playerController}
    {
        m_scriptContext.environment = &m_scriptEnvironment;
        m_parser.addProvider(playlistVariableProvider());
        m_parser.addProvider(artworkMarkerVariableProvider());
    }

    void resetState();

    void prepareScripts();

    PlaylistItem* getOrInsertItem(const UId& key, PlaylistItem::ItemType type, const Data& item, PlaylistItem* parent,
                                  const Md5Hash& baseKey);

    RichText evaluateTrackScript(const ParsedScript& parsedScript, const Track& track, const ScriptContext& context);
    RichText evaluateGroupScript(const ParsedScript& parsedScript, const TrackList& tracks,
                                 const ScriptContext& context);
    [[nodiscard]] const ScriptContext& makeContext(int index = -1, int depth = 0);
    PendingData buildBatchData() const;
    void clearBatchData();
    void updateContainerText(PlaylistContainerItem& container, PlaylistItem::ItemType type, int scriptIndex,
                             const TrackList& tracks);
    void updateContainers();

    void iterateHeader(const Track& track, PlaylistItem*& parent, int index);
    void iterateSubheaders(const Track& track, PlaylistItem*& parent, int index);
    PlaylistItem* iterateTrack(const PlaylistTrack& track, int index);

    void runBatch(int size, int index);
    void runTracksGroup(const std::map<int, PlaylistTrackList>& tracks);

    PlaylistPopulator* m_self;
    PlayerController* m_playerController;

    PlaylistPreset m_currentPreset;
    PlaylistColumnList m_columns;

    struct ParsedHeaderRow
    {
        ParsedScript title;
        ParsedScript subtitle;
        ParsedScript sideText;
        ParsedScript info;
    };
    ParsedHeaderRow m_parsedHeader;

    struct ParsedSubheaderRow
    {
        ParsedScript leftText;
        ParsedScript rightText;
    };
    std::vector<ParsedSubheaderRow> m_parsedSubheaders;

    struct ParsedTrackRow
    {
        std::vector<ParsedScript> columns;
        ParsedScript leftText;
        ParsedScript rightText;
    };
    ParsedTrackRow m_parsedTrack;

    ScriptParser m_parser;
    ScriptFormatter m_formatter;
    PlaylistScriptEnvironment m_scriptEnvironment;
    ScriptContext m_scriptContext;

    int m_preloadCount{2000};
    int m_trackDepth{0};
    Md5Hash m_prevBaseHeaderKey;
    UId m_prevHeaderKey;
    int m_prevIndex{0};
    std::vector<Md5Hash> m_prevBaseSubheaderKey;
    std::vector<UId> m_prevSubheaderKey;

    std::vector<PlaylistContainerItem> m_subheaders;

    PlaylistItem m_root;
    PendingData m_data;
    struct ContainerState
    {
        UId itemKey;
        TrackList tracks;
        int scriptIndex{-1};
        bool emitted{false};
    };
    using ContainerKeyMap = std::unordered_map<UId, ContainerState, UId::UIdHash>;
    ContainerKeyMap m_headers;
    PlaylistTrackList m_tracks;
    Playlist* m_playlist{nullptr};
    PlaylistTrackIndexes m_playlistQueue;
    qsizetype m_nextTrack{0};
};

void PlaylistPopulatorPrivate::resetState()
{
    m_data.clear();
    m_headers.clear();
    m_trackDepth = 0;
    m_prevBaseSubheaderKey.clear();
    m_prevSubheaderKey.clear();
    m_prevBaseHeaderKey = nullptr;
    m_prevHeaderKey     = {};
    m_tracks.clear();
    m_playlist = nullptr;
    m_playlistQueue.clear();
    m_nextTrack    = 0;
    m_parsedHeader = {};
    m_parsedSubheaders.clear();
    m_parsedTrack = {};
}

void PlaylistPopulatorPrivate::prepareScripts()
{
    m_parsedHeader.title    = m_parser.parse(m_currentPreset.header.title.script);
    m_parsedHeader.subtitle = m_parser.parse(m_currentPreset.header.subtitle.script);
    m_parsedHeader.sideText = m_parser.parse(m_currentPreset.header.sideText.script);
    m_parsedHeader.info     = m_parser.parse(m_currentPreset.header.info.script);

    m_parsedSubheaders.clear();
    m_parsedSubheaders.reserve(m_currentPreset.subHeaders.size());
    for(const auto& subheader : std::as_const(m_currentPreset.subHeaders)) {
        m_parsedSubheaders.push_back(
            {m_parser.parse(subheader.leftText.script), m_parser.parse(subheader.rightText.script)});
    }

    m_parsedTrack = {};
    if(!m_columns.empty()) {
        m_parsedTrack.columns.reserve(m_columns.size());
        for(const auto& column : m_columns) {
            m_parsedTrack.columns.push_back(m_parser.parse(column.field));
        }
    }
    else {
        m_parsedTrack.leftText  = m_parser.parse(m_currentPreset.track.leftText.script);
        m_parsedTrack.rightText = m_parser.parse(m_currentPreset.track.rightText.script);
    }
}

PlaylistItem* PlaylistPopulatorPrivate::getOrInsertItem(const UId& key, PlaylistItem::ItemType type, const Data& item,
                                                        PlaylistItem* parent, const Md5Hash& baseKey)
{
    auto [node, inserted] = m_data.items.try_emplace(key, PlaylistItem{type, item, parent});
    if(inserted) {
        node->second.setBaseKey(baseKey);
        node->second.setKey(key);
    }
    PlaylistItem* child = &node->second;

    if(!child->pending()) {
        child->setPending(true);
        m_data.nodes[parent->key()].push_back(key);
        if(type != PlaylistItem::Track) {
            m_data.containerOrder.push_back(key);
        }
    }
    return child;
}

RichText PlaylistPopulatorPrivate::evaluateTrackScript(const ParsedScript& parsedScript, const Track& track,
                                                       const ScriptContext& context)
{
    const auto evalScript = m_parser.evaluate(parsedScript, track, context);
    if(evalScript.isEmpty()) {
        return {};
    }
    return m_formatter.evaluate(evalScript);
}

RichText PlaylistPopulatorPrivate::evaluateGroupScript(const ParsedScript& parsedScript, const TrackList& tracks,
                                                       const ScriptContext& context)
{
    const auto evalScript = m_parser.evaluate(parsedScript, tracks, context);
    if(evalScript.isEmpty()) {
        return {};
    }
    return m_formatter.evaluate(evalScript);
}

const ScriptContext& PlaylistPopulatorPrivate::makeContext(int index, int depth)
{
    int currentPlayingTrackIndex{-1};
    int currentPlayingTrackId{-1};

    if(m_playerController && m_playlist) {
        const auto playingTrack = m_playerController->currentPlaylistTrack();
        if(playingTrack.playlistId == m_playlist->id()) {
            currentPlayingTrackIndex = playingTrack.indexInPlaylist;
            currentPlayingTrackId    = playingTrack.track.id();
        }
    }

    m_scriptEnvironment.setPlaylistData(m_playlist, &m_playlistQueue);
    m_scriptEnvironment.setTrackState(index, currentPlayingTrackIndex, currentPlayingTrackId, depth);
    m_scriptEnvironment.setPlaybackState(m_playerController ? m_playerController->currentPosition() : 0,
                                         m_playerController ? m_playerController->currentTrack().duration() : 0,
                                         m_playerController ? m_playerController->bitrate() : 0,
                                         m_playerController ? m_playerController->playState()
                                                            : Player::PlayState::Stopped);
    m_scriptContext.playlist = m_playlist;
    return m_scriptContext;
}

PendingData PlaylistPopulatorPrivate::buildBatchData() const
{
    PendingData batchData;
    batchData.playlistId     = m_data.playlistId;
    batchData.updatedItems   = m_data.updatedItems;
    batchData.nodes          = m_data.nodes;
    batchData.containerOrder = m_data.containerOrder;
    batchData.trackParents   = m_data.trackParents;
    batchData.parent         = m_data.parent;
    batchData.row            = m_data.row;
    batchData.indexNodes     = m_data.indexNodes;

    for(const auto& childKeys : batchData.nodes | std::views::values) {
        for(const auto& childKey : childKeys) {
            if(const auto it = m_data.items.find(childKey); it != m_data.items.cend()) {
                batchData.items.emplace(childKey, it->second);
            }
        }
    }

    return batchData;
}

void PlaylistPopulatorPrivate::clearBatchData()
{
    for(auto itemIt = m_data.items.begin(); itemIt != m_data.items.end();) {
        if(itemIt->second.type() == PlaylistItem::Track) {
            itemIt = m_data.items.erase(itemIt);
        }
        else {
            ++itemIt;
        }
    }

    m_data.updatedItems.clear();
    m_data.nodes.clear();
    m_data.containerOrder.clear();
    m_data.trackParents.clear();
    m_data.parent.clear();
    m_data.row = -1;
    m_data.indexNodes.clear();

    for(auto& state : m_headers | std::views::values) {
        state.emitted = true;
    }
}

void PlaylistPopulatorPrivate::updateContainerText(PlaylistContainerItem& container, PlaylistItem::ItemType type,
                                                   int scriptIndex, const TrackList& tracks)
{
    if(tracks.empty()) {
        container.setTitle({});
        container.setSubtitle({});
        container.setSideText({});
        container.setInfo({});
        container.clearCoverTrack();
        container.calculateSize();
        return;
    }

    if(type == PlaylistItem::Header) {
        const auto& context = makeContext();

        container.setTitle(evaluateGroupScript(m_parsedHeader.title, tracks, context));
        container.setSubtitle(evaluateGroupScript(m_parsedHeader.subtitle, tracks, context));
        container.setSideText(evaluateGroupScript(m_parsedHeader.sideText, tracks, context));
        container.setInfo(evaluateGroupScript(m_parsedHeader.info, tracks, context));
    }
    else if(type == PlaylistItem::Subheader && scriptIndex >= 0
            && std::cmp_less(scriptIndex, m_parsedSubheaders.size())) {
        const auto& parsedSubheader = m_parsedSubheaders.at(scriptIndex);

        const auto& context = makeContext();

        container.setTitle(evaluateGroupScript(parsedSubheader.leftText, tracks, context));
        container.setSubtitle(evaluateGroupScript(parsedSubheader.rightText, tracks, context));
        container.setSideText({});
        container.setInfo({});
    }

    container.setCoverTrack(tracks.front());
    container.calculateSize();
}

void PlaylistPopulatorPrivate::updateContainers()
{
    for(const auto& state : m_headers | std::views::values) {
        if(auto itemIt = m_data.items.find(state.itemKey); itemIt != m_data.items.end()) {
            auto& item      = itemIt->second;
            auto& container = std::get<PlaylistContainerItem>(item.data());
            updateContainerText(container, itemIt->second.type(), state.scriptIndex, state.tracks);
            if(state.emitted) {
                m_data.updatedItems.insert_or_assign(state.itemKey, item);
            }
        }
    }
}

void PlaylistPopulatorPrivate::iterateHeader(const Track& track, PlaylistItem*& parent, int index)
{
    if(!m_currentPreset.header.isValid()) {
        return;
    }

    const auto& context = makeContext(index, m_trackDepth);

    auto evaluateBlocks = [this, &track, &context](const ParsedScript& parsed) -> std::pair<QString, RichText> {
        const auto evalScript = m_parser.evaluate(parsed, track, context);
        RichText richText;
        if(!evalScript.isEmpty()) {
            richText = m_formatter.evaluate(evalScript);
        }
        return {evalScript, std::move(richText)};
    };

    auto generateHeaderKey = [this, &evaluateBlocks]() {
        const auto [titleScript, titleText]       = evaluateBlocks(m_parsedHeader.title);
        const auto [subtitleScript, subtitleText] = evaluateBlocks(m_parsedHeader.subtitle);
        const auto [sideScript, sideText]         = evaluateBlocks(m_parsedHeader.sideText);
        const auto [infoScript, infoText]         = evaluateBlocks(m_parsedHeader.info);

        PlaylistContainerItem header{m_currentPreset.header.simple};
        header.setTitle(titleText);
        header.setSubtitle(subtitleText);
        header.setSideText(sideText);
        header.setInfo(infoText);
        header.setRowHeight(m_currentPreset.header.rowHeight);
        header.setScriptIndex(-1);
        header.calculateSize();

        return std::pair{Utils::generateMd5Hash(titleScript, subtitleScript, sideScript, infoScript),
                         std::move(header)};
    };

    const auto [baseKey, headerData] = generateHeaderKey();
    UId key{UId::create()};
    if(m_prevHeaderKey.isValid() && m_prevBaseHeaderKey == baseKey && index == m_prevIndex + 1) {
        key = m_prevHeaderKey;
    }
    m_prevBaseHeaderKey = baseKey;
    m_prevHeaderKey     = key;

    if(!m_headers.contains(key)) {
        getOrInsertItem(key, PlaylistItem::Header, headerData, parent, baseKey);
        ContainerState state;
        state.itemKey = key;
        m_headers.emplace(key, std::move(state));
    }
    auto& headerState = m_headers.at(key);
    headerState.tracks.emplace_back(track);
    m_data.trackParents[track.id()].push_back(key);

    auto* headerItem = &m_data.items.at(key);
    parent           = headerItem;
    ++m_trackDepth;
}

void PlaylistPopulatorPrivate::iterateSubheaders(const Track& track, PlaylistItem*& parent, int index)
{
    for(int i{0}; i < m_currentPreset.subHeaders.size(); ++i) {
        auto subheader              = m_currentPreset.subHeaders.at(i);
        const auto& parsedSubheader = m_parsedSubheaders.at(i);
        const auto& context         = makeContext(index, m_trackDepth);
        const auto leftText         = evaluateTrackScript(parsedSubheader.leftText, track, context);
        const auto rightText        = evaluateTrackScript(parsedSubheader.rightText, track, context);

        PlaylistContainerItem currentContainer{false};
        currentContainer.setTitle(leftText);
        currentContainer.setSubtitle(rightText);
        currentContainer.setRowHeight(subheader.rowHeight);
        currentContainer.setScriptIndex(i);
        currentContainer.calculateSize();
        m_subheaders.push_back(currentContainer);
    }

    const int subheaderCount = static_cast<int>(m_subheaders.size());
    m_prevSubheaderKey.resize(subheaderCount);
    m_prevBaseSubheaderKey.resize(subheaderCount);

    auto generateSubheaderKey = [](const PlaylistContainerItem& subheader) {
        QString subheaderKey;
        for(const auto& block : subheader.title().blocks) {
            subheaderKey += block.text;
        }
        for(const auto& block : subheader.subtitle().blocks) {
            subheaderKey += block.text;
        }
        return subheaderKey;
    };

    for(int i{0}; const auto& subheader : m_subheaders) {
        const QString subheaderKey = generateSubheaderKey(subheader);

        if(subheaderKey.isEmpty()) {
            m_prevBaseSubheaderKey[i] = {};
            m_prevSubheaderKey[i]     = {};
            continue;
        }

        const auto baseKey = Utils::generateMd5Hash(parent->baseKey(), subheaderKey);
        UId key{UId::create()};
        if(std::cmp_greater(m_prevSubheaderKey.size(), i) && m_prevBaseSubheaderKey.at(i) == baseKey
           && index == m_prevIndex + 1) {
            key = m_prevSubheaderKey.at(i);
        }
        m_prevBaseSubheaderKey[i] = baseKey;
        m_prevSubheaderKey[i]     = key;

        if(!m_headers.contains(key)) {
            getOrInsertItem(key, PlaylistItem::Subheader, subheader, parent, baseKey);
            ContainerState state;
            state.itemKey     = key;
            state.scriptIndex = i;
            m_headers.emplace(key, std::move(state));
        }
        auto& subheaderState = m_headers.at(key);
        subheaderState.tracks.emplace_back(track);
        m_data.trackParents[track.id()].push_back(key);

        auto* subheaderItem = &m_data.items.at(key);
        parent              = subheaderItem;
        ++i;
        ++m_trackDepth;
    }

    m_subheaders.clear();
}

PlaylistItem* PlaylistPopulatorPrivate::iterateTrack(const PlaylistTrack& track, int index)
{
    PlaylistItem* parent = &m_root;

    iterateHeader(track.track, parent, index);
    iterateSubheaders(track.track, parent, index);

    if(!m_currentPreset.track.isValid()) {
        return nullptr;
    }

    const auto& context = makeContext(index, m_trackDepth);

    PlaylistTrackItem playlistTrack = [&] {
        if(!m_columns.empty()) {
            std::vector<RichText> trackColumns;
            trackColumns.reserve(m_columns.size());
            for(size_t i{0}; i < m_columns.size(); ++i) {
                const auto evalScript = m_parser.evaluate(m_parsedTrack.columns.at(i), track.track, context);
                trackColumns.emplace_back(m_formatter.evaluate(evalScript));
            }
            return PlaylistTrackItem{std::move(trackColumns), track};
        }

        return PlaylistTrackItem{evaluateTrackScript(m_parsedTrack.leftText, track.track, context),
                                 evaluateTrackScript(m_parsedTrack.rightText, track.track, context), track};
    }();

    playlistTrack.setRowHeight(m_currentPreset.track.rowHeight);
    playlistTrack.setDepth(m_trackDepth);
    playlistTrack.calculateSize();

    const auto baseKey
        = Utils::generateMd5Hash(parent->key().toString(UId::Id128), track.track.hash(), QString::number(index));
    const UId key{UId::create()};

    auto* trackItem = getOrInsertItem(key, PlaylistItem::Track, playlistTrack, parent, baseKey);
    m_data.trackParents[track.track.id()].push_back(key);

    m_trackDepth = 0;
    m_prevIndex  = index;
    return trackItem;
}

void PlaylistPopulatorPrivate::runBatch(int size, int index)
{
    if(size <= 0 || std::cmp_greater_equal(m_nextTrack, m_tracks.size())) {
        return;
    }

    const auto remainingTracks = static_cast<qsizetype>(m_tracks.size()) - m_nextTrack;
    const int batchSize        = std::min(size, static_cast<int>(remainingTracks));
    auto tracksBatch           = m_tracks | std::views::drop(m_nextTrack) | std::views::take(batchSize);

    for(const PlaylistTrack& track : tracksBatch) {
        if(!m_self->mayRun()) {
            return;
        }
        iterateTrack(track, index++);
    }

    updateContainers();

    if(!m_self->mayRun()) {
        return;
    }

    const PendingData batchData = buildBatchData();
    emit m_self->populated(batchData);

    m_nextTrack += batchSize;
    clearBatchData();

    const auto remaining = static_cast<int>(m_tracks.size() - m_nextTrack);
    runBatch(remaining, index);
}

void PlaylistPopulatorPrivate::runTracksGroup(const std::map<int, PlaylistTrackList>& tracks)
{
    int nextIndex = m_playlist ? m_playlist->trackCount() : 0;

    for(const auto& [index, trackGroup] : tracks) {
        std::vector<UId> trackKeys;

        int trackIndex = index >= 0 ? index : nextIndex;

        for(const PlaylistTrack& track : trackGroup) {
            if(!m_self->mayRun()) {
                return;
            }
            if(const auto* trackItem = iterateTrack(track, trackIndex++)) {
                trackKeys.push_back(trackItem->key());
            }
        }

        nextIndex = trackIndex;
        m_data.indexNodes.emplace(index, trackKeys);
    }

    updateContainers();

    if(!m_self->mayRun()) {
        return;
    }

    const PendingData batchData = buildBatchData();
    emit m_self->populatedTrackGroup(batchData);
    clearBatchData();
}

PlaylistPopulator::PlaylistPopulator(PlayerController* playerController, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<PlaylistPopulatorPrivate>(this, playerController)}
{
    qRegisterMetaType<PendingData>();
}

PlaylistPopulator::~PlaylistPopulator() = default;

void PlaylistPopulator::setFont(const QFont& font)
{
    p->m_formatter.setBaseFont(font);
}

void PlaylistPopulator::setUseVarious(bool enabled)
{
    p->m_scriptEnvironment.setEvaluationPolicy(TrackListContextPolicy::Placeholder, QStringLiteral("|Loading|"), true,
                                               enabled);
}

void PlaylistPopulator::setPreloadCount(int count)
{
    p->m_preloadCount = count;
}

void PlaylistPopulator::run(Playlist* playlist, const PlaylistPreset& preset, const PlaylistColumnList& columns,
                            const PlaylistTrackList& tracks)
{
    setState(Running);

    p->resetState();

    if(playlist) {
        p->m_data.playlistId = playlist->id();
    }
    p->m_currentPreset = preset;
    p->m_columns       = columns;
    p->m_tracks        = tracks;
    p->m_playlist      = playlist;
    p->m_playlistQueue
        = playlist ? p->m_playerController->playbackQueue().indexesForPlaylist(playlist->id()) : PlaylistTrackIndexes{};
    p->prepareScripts();

    const int preloadCount = p->m_preloadCount > 0 ? p->m_preloadCount : static_cast<int>(tracks.size());
    p->runBatch(preloadCount, 0);

    if(mayRun()) {
        emit finished();
    }

    setState(Idle);
}

void PlaylistPopulator::runTracks(Playlist* playlist, const PlaylistPreset& preset, const PlaylistColumnList& columns,
                                  const std::map<int, PlaylistTrackList>& tracks)
{
    setState(Running);

    p->resetState();

    if(playlist) {
        p->m_data.playlistId = playlist->id();
    }
    p->m_currentPreset = preset;
    p->m_columns       = columns;
    p->m_playlist      = playlist;
    p->m_playlistQueue
        = playlist ? p->m_playerController->playbackQueue().indexesForPlaylist(playlist->id()) : PlaylistTrackIndexes{};
    p->prepareScripts();

    p->runTracksGroup(tracks);

    setState(Idle);
}

void PlaylistPopulator::updateTracks(Playlist* playlist, const PlaylistPreset& preset,
                                     const PlaylistColumnList& columns, const std::set<int>& columnsToUpdate,
                                     const TrackItemMap& tracks)
{
    setState(Running);

    p->m_currentPreset = preset;
    p->m_columns       = columns;
    p->m_playlist      = playlist;
    p->m_playlistQueue
        = playlist ? p->m_playerController->playbackQueue().indexesForPlaylist(playlist->id()) : PlaylistTrackIndexes{};
    p->prepareScripts();

    ItemList updatedTracks;

    for(const auto& [track, item] : tracks) {
        PlaylistTrackItem& trackData = std::get<0>(item.data());

        trackData.setTrack(track);
        const auto& context = p->makeContext(trackData.track().indexInPlaylist, trackData.depth());

        if(!columnsToUpdate.empty()) {
            std::vector<RichText> trackColumns;
            trackColumns.reserve(columns.size());
            for(size_t i{0}; i < columns.size(); ++i) {
                const int columnIndex = static_cast<int>(i);
                if(columnsToUpdate.contains(columnIndex)) {
                    const auto evalScript = p->m_parser.evaluate(p->m_parsedTrack.columns.at(i), track.track, context);
                    trackColumns.emplace_back(p->m_formatter.evaluate(evalScript));
                }
                else {
                    trackColumns.emplace_back(trackData.column(columnIndex));
                }
            }
            trackData.setColumns(trackColumns);
        }
        else {
            const auto trackLeft  = p->evaluateTrackScript(p->m_parsedTrack.leftText, track.track, context);
            const auto trackRight = p->evaluateTrackScript(p->m_parsedTrack.rightText, track.track, context);
            trackData.setLeftRight(trackLeft, trackRight);
        }

        updatedTracks.push_back(item);
    }

    emit tracksUpdated(updatedTracks, columnsToUpdate);

    setState(Idle);
}

void PlaylistPopulator::updateHeaders(Playlist* playlist, const PlaylistPreset& preset, const HeaderUpdateList& headers)
{
    setState(Running);

    p->m_currentPreset = preset;
    p->m_playlist      = playlist;
    p->m_playlistQueue
        = playlist ? p->m_playerController->playbackQueue().indexesForPlaylist(playlist->id()) : PlaylistTrackIndexes{};
    p->prepareScripts();

    ItemKeyMap updatedHeaders;

    for(const auto& request : headers) {
        PlaylistItem item = request.item;
        auto& header      = std::get<PlaylistContainerItem>(item.data());
        p->updateContainerText(header, item.type(), header.scriptIndex(), request.tracks);
        updatedHeaders.emplace(item.key(), std::move(item));
    }

    emit headersUpdated(updatedHeaders);

    setState(Idle);
}
} // namespace Fooyin

#include "moc_playlistpopulator.cpp"
