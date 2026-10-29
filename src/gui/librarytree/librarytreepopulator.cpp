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

#include "librarytreepopulator.h"

#include "librarytreescriptcontext.h"

#include <core/constants.h>
#include <core/scripting/scriptparser.h>
#include <gui/scripting/richtextutils.h>
#include <gui/scripting/scriptformatter.h>

#include <unordered_map>
#include <unordered_set>

using namespace Qt::StringLiterals;

constexpr int InitialBatchSize = 3000;
constexpr int BatchSize        = 4000;

namespace Fooyin {
namespace {
QString identityText(const RichText& richText)
{
    return richText.joinedText().trimmed();
}
} // namespace

class LibraryTreePopulatorPrivate
{
public:
    explicit LibraryTreePopulatorPrivate(LibraryTreePopulator* self)
        : m_self{self}
    {
        m_scriptContext.values = m_nodeValues;
    }

    LibraryTreeItem* getOrInsertItem(const Md5Hash& key, const LibraryTreeItem* parent, const QString& title,
                                     const RichText& richTitle, const QString& sortTitle, int level);
    Md5Hash nodeKey(const Md5Hash& parentKey, const QString& title);
    void setNodeScriptSource(LibraryTreeItem& item, const Track& track, int valueIndex, const QStringList& levels);
    void refreshNodeScriptSource(LibraryTreeItem& item, const Track& track);
    QString evaluateNodeTitle(LibraryTreeItem& item);
    void updateRichTitle(LibraryTreeItem& item);
    PendingTreeData buildBatchData();
    PendingTreeData buildCompleteData();
    void clearBatchData();
    void iterateTrack(const Track& track);
    bool runBatch(int size);

    LibraryTreePopulator* m_self;
    LibraryTreePopulationContext m_context;

    bool mayRun() const
    {
        return m_self->mayRun();
    }

    void setContext(LibraryTreePopulationContext context)
    {
        m_context = std::move(context);
        m_formatter.setBaseFont(m_context.font);
        m_scriptContext.libraries   = &m_context.libraries;
        m_options.ratingSymbols     = m_context.ratingSymbols;
        m_options.escapeRichText    = true;
        m_options.useVariousArtists = m_context.useVarious;
    }

    ScriptParser m_parser;
    ScriptFormatter m_formatter;
    LibraryTreeScriptValues m_nodeValues{libraryTreeScriptValues(1, 0)};
    ScriptEvaluationOptions m_options;
    ScriptContext m_scriptContext;

    LibraryTreeGrouping m_currentGrouping;
    ParsedScript m_groupScript;
    ParsedScript m_displayScript;
    ParsedScript m_sortScript;
    bool m_usesNodeCounts{false};

    LibraryTreeItem m_root;
    ItemKeyMap m_items;
    PendingTreeData m_data;
    std::unordered_set<Md5Hash> m_touchedItems;
    std::unordered_set<Md5Hash> m_emittedItems;
    std::unordered_map<Md5Hash, std::unordered_set<Md5Hash>> m_childKeys;
    std::unordered_map<Md5Hash, std::unordered_map<QString, Md5Hash>> m_nodeKeyCache;
    TrackList m_pendingTracks;
    size_t m_pendingTrackIndex{0};
    LibraryTreePopulator::PopulationMode m_populationMode{LibraryTreePopulator::PopulationMode::Incremental};
};

LibraryTreeItem* LibraryTreePopulatorPrivate::getOrInsertItem(const Md5Hash& key, const LibraryTreeItem* parent,
                                                              const QString& title, const RichText& richTitle,
                                                              const QString& sortTitle, int level)
{
    auto [node, inserted] = m_items.try_emplace(key, LibraryTreeItem{title, nullptr, level});
    if(inserted) {
        node->second.setKey(key);
        node->second.setTitleSource(title);
        node->second.setRichTitle(richTitle);
        node->second.setSortTitle(sortTitle);
    }
    LibraryTreeItem* child = &node->second;
    m_touchedItems.insert(key);
    m_childKeys[parent->key()].insert(key);

    if(child->sortTitle().isEmpty() && !sortTitle.isEmpty()) {
        child->setSortTitle(sortTitle);
    }

    if(inserted) {
        m_data.nodes[parent->key()].push_back(key);
    }
    return child;
}

Md5Hash LibraryTreePopulatorPrivate::nodeKey(const Md5Hash& parentKey, const QString& title)
{
    auto& childKeys      = m_nodeKeyCache[parentKey];
    auto [key, inserted] = childKeys.try_emplace(title);
    if(inserted) {
        key->second = Utils::generateMd5Hash(parentKey, title);
    }
    return key->second;
}

void LibraryTreePopulatorPrivate::setNodeScriptSource(LibraryTreeItem& item, const Track&, int valueIndex,
                                                      const QStringList& levels)
{
    item.setTitleSource(levels.value(item.level()));
    if(!m_usesNodeCounts) {
        item.setScriptSource({});
        return;
    }
    item.setScriptSource({.scriptIndex = 0, .valueIndex = valueIndex, .level = item.level()});
}

void LibraryTreePopulatorPrivate::refreshNodeScriptSource(LibraryTreeItem& item, const Track& track)
{
    const auto identityValues = m_parser.evaluate(m_groupScript, track, m_scriptContext, m_options)
                                    .split(QLatin1StringView{Constants::UnitSeparator}, Qt::SkipEmptyParts);
    const auto displayValues  = m_currentGrouping.groupingScript.isEmpty()
                                  ? identityValues
                                  : m_parser.evaluate(m_displayScript, track, m_scriptContext, m_options)
                                        .split(QLatin1StringView{Constants::UnitSeparator});
    for(int index{0}; const auto& value : identityValues) {
        Md5Hash key;
        const auto levels = value.split(u"||"_s);
        for(int level{0}; const auto& text : levels) {
            key = nodeKey(key, identityText(trimRichText(m_formatter.evaluate(text))));
            if(level == item.level()) {
                if(key == item.key()) {
                    const int displayIndex = displayValues.size() == identityValues.size() ? index : 0;
                    setNodeScriptSource(item, track, displayIndex, displayValues.value(displayIndex).split(u"||"_s));
                    return;
                }
                break;
            }
            ++level;
        }
        ++index;
    }
}

QString LibraryTreePopulatorPrivate::evaluateNodeTitle(LibraryTreeItem& item)
{
    if(item.scriptSource().scriptIndex < 0 || item.tracks().empty()) {
        return item.titleSource();
    }

    const auto& track = item.tracks().front();
    if(item.scriptSource().valueIndex < 0) {
        refreshNodeScriptSource(item, track);
    }

    const auto& source = item.scriptSource();
    if(source.valueIndex < 0) {
        return {};
    }

    if(item.trackCount() == 1 && item.scriptChildCount() == 0) {
        return item.titleSource();
    }

    const auto nodeValues = libraryTreeScriptValues(item.trackCount(), item.scriptChildCount());
    auto context{m_scriptContext};
    context.values           = nodeValues;
    const auto displayValues = m_parser.evaluate(m_displayScript, track, context, m_options)
                                   .split(QLatin1StringView{Constants::UnitSeparator}, Qt::SkipEmptyParts);
    return displayValues.value(source.valueIndex).section(u"||"_s, source.level, source.level);
}

void LibraryTreePopulatorPrivate::updateRichTitle(LibraryTreeItem& item)
{
    const QString title = evaluateNodeTitle(item);
    item.setRichTitle(trimRichText(m_formatter.evaluate(title)));
}

PendingTreeData LibraryTreePopulatorPrivate::buildBatchData()
{
    PendingTreeData data;
    data.generation   = m_context.generation;
    data.nodes        = m_data.nodes;
    data.trackParents = m_data.trackParents;

    for(const auto& key : m_touchedItems) {
        auto itemIt = m_items.find(key);
        if(itemIt == m_items.end()) {
            continue;
        }

        itemIt->second.setScriptChildCount(static_cast<int>(m_childKeys[key].size()));
        updateRichTitle(itemIt->second);

        if(auto batchIt = m_data.items.find(key); batchIt != m_data.items.end()) {
            batchIt->second.setScriptChildCount(itemIt->second.scriptChildCount());
            batchIt->second.setRichTitles(itemIt->second.richTitle(), itemIt->second.rightRichTitle());
            data.items.emplace(key, batchIt->second);
        }
        else if(m_emittedItems.contains(key)) {
            data.updatedItems.emplace(key, itemIt->second);
        }
    }

    return data;
}

PendingTreeData LibraryTreePopulatorPrivate::buildCompleteData()
{
    for(auto& [key, item] : m_items) {
        item.setScriptChildCount(static_cast<int>(m_childKeys[key].size()));
        updateRichTitle(item);
        item.setPending(true);
    }

    PendingTreeData data;
    data.generation   = m_context.generation;
    data.items        = std::move(m_items);
    data.nodes        = std::move(m_data.nodes);
    data.trackParents = std::move(m_data.trackParents);
    return data;
}

void LibraryTreePopulatorPrivate::clearBatchData()
{
    for(const auto& key : m_touchedItems) {
        m_emittedItems.insert(key);
    }

    m_data.clear();
    m_touchedItems.clear();
}

void LibraryTreePopulatorPrivate::iterateTrack(const Track& track)
{
    const QString identityField = m_parser.evaluate(m_groupScript, track, m_scriptContext, m_options);
    if(identityField.isNull()) {
        return;
    }
    const QString displayField   = m_currentGrouping.groupingScript.isEmpty()
                                     ? identityField
                                     : m_parser.evaluate(m_displayScript, track, m_scriptContext, m_options);
    const QString sortField      = m_sortScript.input.isEmpty()
                                     ? identityField
                                     : m_parser.evaluate(m_sortScript, track, m_scriptContext, m_options);
    const auto identityValues    = identityField.split(QLatin1StringView{Constants::UnitSeparator}, Qt::SkipEmptyParts);
    const auto displayValues     = m_currentGrouping.groupingScript.isEmpty()
                                     ? identityValues
                                     : displayField.split(QLatin1StringView{Constants::UnitSeparator});
    const auto sortValues        = m_sortScript.input.isEmpty()
                                     ? identityValues
                                     : sortField.split(QLatin1StringView{Constants::UnitSeparator}, Qt::SkipEmptyParts);
    const bool pairDisplayValues = displayValues.size() == identityValues.size();
    const bool pairSortValues    = sortValues.size() == identityValues.size();

    for(int valueIndex{0}; const auto& identityValue : identityValues) {
        const LibraryTreeItem* parent = &m_root;
        const auto identityItems      = identityValue.split(u"||"_s);
        const int displayIndex        = pairDisplayValues ? valueIndex : 0;
        const auto displayItems       = m_currentGrouping.groupingScript.isEmpty()
                                          ? identityItems
                                          : displayValues.value(displayIndex).split(u"||"_s);
        const auto sortItems          = m_sortScript.input.isEmpty() ? identityItems
                                      : pairSortValues               ? sortValues.value(valueIndex).split(u"||"_s)
                                                                     : QStringList{};
        const bool pairSortItems      = identityItems.size() == sortItems.size();

        for(int level{0}; const auto& identityItem : identityItems) {
            const auto identityRichTitle = trimRichText(m_formatter.evaluate(identityItem));
            const QString groupingTitle  = identityText(identityRichTitle);
            const auto key               = nodeKey(parent->key(), groupingTitle);
            const auto richTitle         = m_currentGrouping.groupingScript.isEmpty()
                                             ? identityRichTitle
                                             : trimRichText(m_formatter.evaluate(displayItems.value(level)));
            const QString title          = identityText(richTitle);
            const QString sortTitle      = pairSortItems
                                             ? identityText(trimRichText(m_formatter.evaluate(sortItems.value(level))))
                                             : groupingTitle;
            auto* node = getOrInsertItem(key, parent, title, richTitle, sortTitle.isEmpty() ? groupingTitle : sortTitle,
                                         level);
            if(node->tracks().empty()) {
                setNodeScriptSource(*node, track, displayIndex, displayItems);
            }
            if(node->scriptSource().scriptIndex < 0) {
                node->setTitleSource(displayItems.value(level));
            }
            node->addTrack(track);

            if(m_populationMode == LibraryTreePopulator::PopulationMode::Incremental) {
                auto [batchNode, inserted] = m_data.items.try_emplace(key, LibraryTreeItem{title, nullptr, level});
                if(inserted) {
                    batchNode->second.setKey(key);
                    batchNode->second.setPending(true);
                    setNodeScriptSource(batchNode->second, track, displayIndex, displayItems);
                    batchNode->second.setRichTitle(richTitle);
                    batchNode->second.setSortTitle(sortTitle.isEmpty() ? groupingTitle : sortTitle);
                }
                batchNode->second.addTrack(track);
            }
            m_data.trackParents[track.id()].push_back(node->key());
            parent = node;
            ++level;
        }
        ++valueIndex;
    }
}

bool LibraryTreePopulatorPrivate::runBatch(int size)
{
    if(size <= 0) {
        return true;
    }

    const size_t batchEnd = std::min(m_pendingTrackIndex + static_cast<size_t>(size), m_pendingTracks.size());

    for(size_t index{m_pendingTrackIndex}; index < batchEnd; ++index) {
        if(!mayRun()) {
            return false;
        }

        const Track& track = m_pendingTracks.at(index);
        if(track.isInLibrary()) {
            iterateTrack(track);
        }
    }

    if(!mayRun()) {
        return false;
    }

    m_pendingTrackIndex = batchEnd;

    const auto remaining = static_cast<int>(m_pendingTracks.size() - m_pendingTrackIndex);

    if(m_populationMode == LibraryTreePopulator::PopulationMode::Incremental) {
        Q_EMIT m_self->populated(std::make_shared<PendingTreeData>(buildBatchData()));
        clearBatchData();
    }
    else if(remaining == 0) {
        Q_EMIT m_self->populated(std::make_shared<PendingTreeData>(buildCompleteData()));
    }

    return runBatch(std::min(remaining, BatchSize));
}

LibraryTreePopulator::LibraryTreePopulator(QObject* parent)
    : Worker{parent}
    , p{std::make_unique<LibraryTreePopulatorPrivate>(this)}
{ }

LibraryTreePopulator::~LibraryTreePopulator() = default;

void LibraryTreePopulator::run(const LibraryTreeGrouping& grouping, const TrackList& tracks,
                               LibraryTreePopulationContext context, PopulationMode mode)
{
    if(!startWork(context.stopToken)) {
        return;
    }

    p->setContext(std::move(context));

    p->m_data.clear();
    p->m_items.clear();
    p->m_touchedItems.clear();
    p->m_emittedItems.clear();
    p->m_childKeys.clear();
    p->m_nodeKeyCache.clear();

    if(std::exchange(p->m_currentGrouping, grouping) != grouping) {
        p->m_displayScript  = p->m_parser.parse(p->m_currentGrouping.script);
        p->m_sortScript     = p->m_currentGrouping.sortScript.isEmpty()
                                ? ParsedScript{}
                                : p->m_parser.parse(p->m_currentGrouping.sortScript);
        p->m_groupScript    = p->m_currentGrouping.groupingScript.isEmpty()
                                ? p->m_displayScript
                                : p->m_parser.parse(p->m_currentGrouping.groupingScript);
        p->m_usesNodeCounts = usesLibraryTreeNodeVariables(p->m_displayScript.input);
    }

    p->m_pendingTracks     = tracks;
    p->m_pendingTrackIndex = 0;
    p->m_populationMode    = mode;
    const bool success     = p->runBatch(InitialBatchSize);

    setState(Idle);

    if(success) {
        Q_EMIT populationFinished(p->m_context.generation, p->m_context.requestId);
    }
}

void LibraryTreePopulator::updateItems(ItemKeyMap items, LibraryTreePopulationContext context)
{
    if(!startWork(context.stopToken)) {
        return;
    }

    p->setContext(std::move(context));

    PendingTreeData data;
    data.generation = p->m_context.generation;

    for(auto& [key, item] : items) {
        if(!p->mayRun()) {
            setState(Idle);
            return;
        }
        p->updateRichTitle(item);
        data.updatedItems.emplace(key, std::move(item));
    }

    Q_EMIT populated(std::make_shared<PendingTreeData>(std::move(data)));
    Q_EMIT populationFinished(p->m_context.generation, p->m_context.requestId);

    setState(Idle);
}
} // namespace Fooyin

#include "moc_librarytreepopulator.cpp"
