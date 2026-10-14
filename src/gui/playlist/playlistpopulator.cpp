/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include <core/scripting/scriptenvironmenthelpers.h>
#include <gui/scripting/richtextutils.h>

#include <QThread>

#include <ranges>
#include <utility>

namespace Fooyin {
class PlaylistPopulatorPrivate
{
public:
    explicit PlaylistPopulatorPrivate(PlaylistPopulator* self)
        : m_self{self}
    {
        m_scriptContext.environment = &m_request.environment;
        m_parser.addProvider(playlistVariableProvider());
        m_parser.addProvider(artworkMarkerVariableProvider());
    }

    [[nodiscard]] bool mayRun() const;

    void resetState();

    void prepareScripts();
    bool prepareRequest(const PlaylistPopulationRequest& request);

    PlaylistItem* getOrInsertItem(const UId& key, PlaylistItem::ItemType type, const Data& item, PlaylistItem* parent,
                                  const Md5Hash& baseKey);

    RichText evaluateTrackScript(const ParsedScript& parsedScript, const Track& track, const ScriptContext& context);
    RichText evaluateGroupScript(const ParsedScript& parsedScript, const TrackList& tracks,
                                 const ScriptContext& context);
    [[nodiscard]] const ScriptContext& makeContext(int index = -1, int depth = 0, const Track* track = nullptr);
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
    PlaylistPopulationRequest m_request;

    struct ParsedHeaderRow
    {
        ParsedScript grouping;
        ParsedScript text;
    };
    ParsedHeaderRow m_parsedHeader;

    struct ParsedSubheaderRow
    {
        ParsedScript grouping;
        ParsedScript text;
    };
    std::vector<ParsedSubheaderRow> m_parsedSubheaders;

    struct ParsedTrackRow
    {
        std::vector<ParsedScript> columns;
        ParsedScript text;
    };
    ParsedTrackRow m_parsedTrack;

    ScriptParser m_parser;
    ScriptFormatter m_formatter;
    ScriptContext m_scriptContext;

    int m_trackDepth{0};
    Md5Hash m_prevBaseHeaderKey;
    UId m_prevHeaderKey;
    int m_prevIndex{0};
    std::vector<Md5Hash> m_prevBaseSubheaderKey;
    std::vector<UId> m_prevSubheaderKey;

    PlaylistItem m_root;
    PendingData m_data;

    struct ContainerState
    {
        UId itemKey;
        TrackList tracks;
        int scriptIndex{-1};
        bool emitted{false};
        bool dirty{false};
    };
    using ContainerKeyMap = std::unordered_map<UId, ContainerState, UId::UIdHash>;

    ContainerKeyMap m_headers;
    PlaylistTrackList m_tracks;
    qsizetype m_nextTrack{0};
};

bool PlaylistPopulatorPrivate::mayRun() const
{
    return m_self->mayRun() && !m_request.stopToken.stop_requested();
}

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
    m_nextTrack    = 0;
    m_parsedHeader = {};
    m_parsedSubheaders.clear();
    m_parsedTrack = {};
}

void PlaylistPopulatorPrivate::prepareScripts()
{
    m_parsedHeader.grouping = m_parser.parse(m_request.preset.header.grouping);
    m_parsedHeader.text     = m_parser.parse(m_request.preset.header.text.script);

    m_parsedSubheaders.clear();
    m_parsedSubheaders.reserve(m_request.preset.subHeaders.size());
    for(const auto& subheader : std::as_const(m_request.preset.subHeaders)) {
        m_parsedSubheaders.emplace_back(m_parser.parse(subheader.grouping), m_parser.parse(subheader.text.script));
    }

    m_parsedTrack = {};
    if(!m_request.columns.empty()) {
        m_parsedTrack.columns.reserve(m_request.columns.size());
        for(const auto& column : m_request.columns) {
            m_parsedTrack.columns.push_back(m_parser.parse(column.field));
        }
    }
    else {
        m_parsedTrack.text = m_parser.parse(m_request.preset.track.text.script);
    }
}

bool PlaylistPopulatorPrivate::prepareRequest(const PlaylistPopulationRequest& request)
{
    if(m_self->closing() || request.stopToken.stop_requested()) {
        return false;
    }

    m_self->setState(Worker::Running);
    m_request = request;
    m_formatter.setBaseFont(request.font);
    prepareScripts();
    return true;
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

const ScriptContext& PlaylistPopulatorPrivate::makeContext(int index, int depth, const Track* track)
{
    int currentPlayingTrackIndex{-1};
    int currentPlayingTrackId{-1};

    const auto& playingTrack = m_request.playingTrack;
    if(m_request.playlistId.isValid() && playingTrack.playlistId == m_request.playlistId) {
        currentPlayingTrackIndex = playingTrack.indexInPlaylist;
        currentPlayingTrackId    = playingTrack.track.id();
    }
    else if(!m_request.playlistId.isValid() && track && track->sameIdentityAs(playingTrack.track)) {
        currentPlayingTrackIndex = index;
        currentPlayingTrackId    = track->id();
    }

    if(const auto queueIndexes = m_request.queueIndexes.find(index); queueIndexes != m_request.queueIndexes.cend()) {
        m_request.environment.setQueueState(queueIndexes->second, m_request.queueTotal);
    }
    else {
        m_request.environment.setQueueState({}, m_request.queueTotal);
    }

    m_request.environment.setTrackState(index, currentPlayingTrackIndex, currentPlayingTrackId, depth);
    return m_scriptContext;
}

PendingData PlaylistPopulatorPrivate::buildBatchData() const
{
    PendingData batchData;
    batchData.generation     = m_request.generation;
    batchData.playlistId     = m_request.playlistId;
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
        container.clearCoverTrack();
        container.calculateSize();
        return;
    }

    if(type == PlaylistItem::Header) {
        const auto& context = makeContext();

        container.setTitle(evaluateGroupScript(m_parsedHeader.text, tracks, context));
        container.setSubtitle({});
        container.setShowCover(m_request.preset.header.showCover);
    }
    else if(type == PlaylistItem::Subheader && scriptIndex >= 0
            && std::cmp_less(scriptIndex, m_parsedSubheaders.size())) {
        const auto& parsedSubheader = m_parsedSubheaders.at(scriptIndex);

        const auto& context = makeContext();

        const auto text = evaluateGroupScript(parsedSubheader.text, tracks, context);
        container.setTitle(text);
        container.setSubtitle({});
    }

    container.setCoverTrack(tracks.front());
    container.calculateSize();
}

void PlaylistPopulatorPrivate::updateContainers()
{
    for(auto& state : m_headers | std::views::values) {
        if(!state.dirty) {
            continue;
        }
        if(auto itemIt = m_data.items.find(state.itemKey); itemIt != m_data.items.end()) {
            auto& item      = itemIt->second;
            auto& container = std::get<PlaylistContainerItem>(item.data());
            updateContainerText(container, itemIt->second.type(), state.scriptIndex, state.tracks);
            if(state.emitted) {
                m_data.updatedItems.insert_or_assign(state.itemKey, item);
            }
            state.dirty = false;
        }
    }
}

void PlaylistPopulatorPrivate::iterateHeader(const Track& track, PlaylistItem*& parent, int index)
{
    if(!m_request.preset.header.isValid()) {
        return;
    }

    const auto& context = makeContext(index, m_trackDepth);
    const auto script   = m_parser.evaluate(m_parsedHeader.text, track, context);
    const auto baseKey  = !m_request.preset.header.grouping.isEmpty()
                            ? Utils::generateMd5Hash(m_parser.evaluate(m_parsedHeader.grouping, track, context))
                            : Utils::generateMd5Hash(script);

    UId key;
    if(m_prevHeaderKey.isValid() && m_prevBaseHeaderKey == baseKey && index == m_prevIndex + 1) {
        key = m_prevHeaderKey;
    }
    else {
        key = UId::create();
    }

    m_prevBaseHeaderKey = baseKey;
    m_prevHeaderKey     = key;

    if(!m_headers.contains(key)) {
        PlaylistContainerItem headerData{PlaylistContainerItem::LayoutKind::Header};
        headerData.setRowHeight(m_request.preset.header.rowHeight);
        headerData.setScriptIndex(-1);

        getOrInsertItem(key, PlaylistItem::Header, headerData, parent, baseKey);
        ContainerState state;
        state.itemKey = key;
        m_headers.emplace(key, std::move(state));
    }

    auto& headerState = m_headers.at(key);
    headerState.tracks.emplace_back(track);
    headerState.dirty = true;
    m_data.trackParents[track.id()].push_back(key);

    auto* headerItem = &m_data.items.at(key);
    parent           = headerItem;
    ++m_trackDepth;
}

void PlaylistPopulatorPrivate::iterateSubheaders(const Track& track, PlaylistItem*& parent, int index)
{
    const auto subheaderCount = static_cast<int>(m_request.preset.subHeaders.size());
    m_prevSubheaderKey.resize(subheaderCount);
    m_prevBaseSubheaderKey.resize(subheaderCount);

    for(int i{0}; i < subheaderCount; ++i) {
        auto subheader              = m_request.preset.subHeaders.at(i);
        const auto& parsedSubheader = m_parsedSubheaders.at(i);
        const auto& context         = makeContext(index, m_trackDepth);
        const auto subheaderScript  = m_parser.evaluate(parsedSubheader.text, track, context);
        const auto subheaderText    = m_formatter.evaluate(subheaderScript);

        if(subheaderText.joinedText().isEmpty()) {
            m_prevBaseSubheaderKey[i] = {};
            m_prevSubheaderKey[i]     = {};
            continue;
        }

        const auto groupingKey = !subheader.grouping.isEmpty()
                                   ? m_parser.evaluate(parsedSubheader.grouping, track, context)
                                   : subheaderScript;
        const auto baseKey     = Utils::generateMd5Hash(parent->baseKey(), groupingKey);

        UId key;
        if(std::cmp_greater(m_prevSubheaderKey.size(), i) && m_prevBaseSubheaderKey.at(i) == baseKey
           && index == m_prevIndex + 1) {
            key = m_prevSubheaderKey.at(i);
        }
        else {
            key = UId::create();
        }

        m_prevBaseSubheaderKey[i] = baseKey;
        m_prevSubheaderKey[i]     = key;

        if(!m_headers.contains(key)) {
            PlaylistContainerItem subheaderData{PlaylistContainerItem::LayoutKind::Subheader};
            subheaderData.setRowHeight(subheader.rowHeight);
            subheaderData.setScriptIndex(i);

            getOrInsertItem(key, PlaylistItem::Subheader, subheaderData, parent, baseKey);
            ContainerState state;
            state.itemKey     = key;
            state.scriptIndex = i;
            m_headers.emplace(key, std::move(state));
        }
        auto& subheaderState = m_headers.at(key);
        subheaderState.tracks.emplace_back(track);
        subheaderState.dirty = true;
        m_data.trackParents[track.id()].push_back(key);

        auto* subheaderItem = &m_data.items.at(key);
        parent              = subheaderItem;
        ++m_trackDepth;
    }
}

PlaylistItem* PlaylistPopulatorPrivate::iterateTrack(const PlaylistTrack& track, int index)
{
    PlaylistItem* parent = &m_root;

    iterateHeader(track.track, parent, index);
    iterateSubheaders(track.track, parent, index);

    if(!m_request.preset.track.isValid()) {
        return nullptr;
    }

    const auto& context = makeContext(index, m_trackDepth, &track.track);

    PlaylistTrackItem playlistTrack = [&] {
        if(!m_request.columns.empty()) {
            std::vector<RichText> trackColumns;
            trackColumns.reserve(m_request.columns.size());
            for(size_t i{0}; i < m_request.columns.size(); ++i) {
                const auto evalScript = m_parser.evaluate(m_parsedTrack.columns.at(i), track.track, context);
                trackColumns.emplace_back(m_formatter.evaluate(evalScript));
            }
            return PlaylistTrackItem{std::move(trackColumns), track};
        }

        const auto text = evaluateTrackScript(m_parsedTrack.text, track.track, context);
        return PlaylistTrackItem{richTextForAlignment(text, RichAlignment::Left),
                                 richTextForAlignment(text, RichAlignment::Right), track};
    }();

    playlistTrack.setRowHeight(m_request.preset.track.rowHeight);
    playlistTrack.setDepth(m_trackDepth);
    playlistTrack.calculateHeight();

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
        if(!mayRun()) {
            return;
        }
        iterateTrack(track, index++);
    }

    updateContainers();

    if(!mayRun()) {
        return;
    }

    const PendingData batchData = buildBatchData();
    Q_EMIT m_self->populated(batchData);

    m_nextTrack += batchSize;
    clearBatchData();

    const auto remaining = static_cast<int>(m_tracks.size() - m_nextTrack);
    runBatch(remaining, index);
}

void PlaylistPopulatorPrivate::runTracksGroup(const std::map<int, PlaylistTrackList>& tracks)
{
    int nextIndex = m_request.trackCount;

    for(const auto& [index, trackGroup] : tracks) {
        std::vector<UId> trackKeys;

        int trackIndex = index >= 0 ? index : nextIndex;

        for(const PlaylistTrack& track : trackGroup) {
            if(!mayRun()) {
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

    if(!mayRun()) {
        return;
    }

    const PendingData batchData = buildBatchData();
    Q_EMIT m_self->populatedTrackGroup(batchData);
    clearBatchData();
}

PlaylistPopulator::PlaylistPopulator(QObject* parent)
    : Worker{parent}
    , p{std::make_unique<PlaylistPopulatorPrivate>(this)}
{
    qRegisterMetaType<PendingData>();
}

PlaylistPopulator::~PlaylistPopulator() = default;

void PlaylistPopulator::run(const PlaylistPopulationRequest& request, const PlaylistTrackList& tracks)
{
    if(request.stopToken.stop_requested()) {
        return;
    }

    p->resetState();

    if(!p->prepareRequest(request)) {
        return;
    }

    p->m_tracks = tracks;

    const int preloadCount = request.preloadCount > 0 ? request.preloadCount : static_cast<int>(tracks.size());
    p->runBatch(preloadCount, 0);

    if(p->mayRun()) {
        Q_EMIT populationFinished(request.generation);
    }

    setState(Idle);
}

void PlaylistPopulator::runTracks(const PlaylistPopulationRequest& request,
                                  const std::map<int, PlaylistTrackList>& tracks)
{
    if(request.stopToken.stop_requested()) {
        return;
    }

    p->resetState();

    if(!p->prepareRequest(request)) {
        return;
    }

    p->runTracksGroup(tracks);

    setState(Idle);
}

void PlaylistPopulator::updateTracks(const PlaylistPopulationRequest& request, const std::set<int>& columnsToUpdate,
                                     const TrackItemMap& tracks)
{
    if(!p->prepareRequest(request)) {
        return;
    }

    const auto& columns = request.columns;

    ItemList updatedTracks;

    for(const auto& [track, item] : tracks) {
        if(!p->mayRun()) {
            setState(Idle);
            return;
        }

        PlaylistTrackItem& trackData = std::get<0>(item.data());

        trackData.setTrack(track);
        const auto& context
            = p->makeContext(trackData.track().indexInPlaylist, trackData.depth(), &trackData.track().track);

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
            const auto text = p->evaluateTrackScript(p->m_parsedTrack.text, track.track, context);
            trackData.setLeftRight(richTextForAlignment(text, RichAlignment::Left),
                                   richTextForAlignment(text, RichAlignment::Right));
        }

        updatedTracks.push_back(item);
    }

    if(p->mayRun()) {
        Q_EMIT tracksUpdated(request.generation, updatedTracks, columnsToUpdate);
    }

    setState(Idle);
}

void PlaylistPopulator::updateHeaders(const PlaylistPopulationRequest& request, const HeaderUpdateList& headers)
{
    if(!p->prepareRequest(request)) {
        return;
    }

    ItemKeyMap updatedHeaders;

    for(const auto& headerRequest : headers) {
        if(!p->mayRun()) {
            setState(Idle);
            return;
        }

        PlaylistItem item = headerRequest.item;
        auto& header      = std::get<PlaylistContainerItem>(item.data());
        p->updateContainerText(header, item.type(), header.scriptIndex(), headerRequest.tracks);
        updatedHeaders.emplace(item.key(), std::move(item));
    }

    if(p->mayRun()) {
        Q_EMIT headersUpdated(request.generation, updatedHeaders);
    }

    setState(Idle);
}
} // namespace Fooyin

#include "moc_playlistpopulator.cpp"
