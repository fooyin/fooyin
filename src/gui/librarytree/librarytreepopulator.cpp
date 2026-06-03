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

#include "librarytreescriptenvironment.h"
#include "scripting/scriptvariableproviders.h"

#include <core/constants.h>
#include <core/scripting/scriptparser.h>
#include <gui/guiutils.h>
#include <gui/scripting/richtextutils.h>
#include <gui/scripting/scriptformatter.h>
#include <utils/settings/settingsmanager.h>

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
    explicit LibraryTreePopulatorPrivate(LibraryTreePopulator* self, LibraryManager* libraryManager,
                                         SettingsManager* settings)
        : m_self{self}
        , m_settings{settings}
        , m_scriptEnvironment{libraryManager}
    {
        m_scriptContext.environment = &m_scriptEnvironment;
        m_parser.addProvider(artworkMarkerVariableProvider());
        m_parser.addProvider(libraryTreeNodeVariableProvider());
    }

    LibraryTreeItem* getOrInsertItem(const Md5Hash& key, const LibraryTreeItem* parent, const QString& title,
                                     const RichText& richTitle, const QString& sortTitle, int level);
    void updateRichTitle(LibraryTreeItem& item);
    PendingTreeData buildBatchData();
    void clearBatchData();
    void iterateTrack(const Track& track);
    bool runBatch(int size);

    LibraryTreePopulator* m_self;
    SettingsManager* m_settings;

    ScriptParser m_parser;
    ScriptFormatter m_formatter;
    LibraryTreeScriptEnvironment m_scriptEnvironment;
    ScriptContext m_scriptContext;
    ScriptContext m_identityScriptContext;

    LibraryTreeGrouping m_currentGrouping;
    ParsedScript m_displayScript;
    ParsedScript m_sortScript;

    LibraryTreeItem m_root;
    ItemKeyMap m_items;
    PendingTreeData m_data;
    std::unordered_set<Md5Hash> m_touchedItems;
    std::unordered_set<Md5Hash> m_emittedItems;
    std::unordered_map<Md5Hash, std::unordered_set<Md5Hash>> m_childKeys;
    TrackList m_pendingTracks;
    size_t m_pendingTrackIndex{0};
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

    if(!m_emittedItems.contains(key) && !m_data.items.contains(key)) {
        m_data.nodes[parent->key()].push_back(key);
    }
    return child;
}

void LibraryTreePopulatorPrivate::updateRichTitle(LibraryTreeItem& item)
{
    const QString title
        = resolveLibraryTreeNodeVariables(item.titleSource(), item.trackCount(), item.scriptChildCount());
    item.setRichTitle(trimRichText(m_formatter.evaluate(title)));
}

PendingTreeData LibraryTreePopulatorPrivate::buildBatchData()
{
    PendingTreeData data;
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
    const QString displayField = m_parser.evaluate(m_displayScript, track, m_scriptContext);
    if(displayField.isNull()) {
        return;
    }
    const QString sortField
        = m_sortScript.input.isEmpty() ? displayField : m_parser.evaluate(m_sortScript, track, m_scriptContext);

    const QStringList displayValues = displayField.split(QLatin1String{Constants::UnitSeparator}, Qt::SkipEmptyParts);
    const QStringList sortValues    = sortField.split(QLatin1String{Constants::UnitSeparator}, Qt::SkipEmptyParts);
    const bool pairSortValues       = displayValues.size() == sortValues.size();

    for(int valueIndex{0}; const QString& displayValue : displayValues) {
        if(displayValue.isNull()) {
            continue;
        }

        const LibraryTreeItem* parent  = &m_root;
        const QStringList displayItems = displayValue.split(u"||"_s);
        const QStringList sortItems    = pairSortValues ? sortValues.value(valueIndex).split(u"||"_s) : QStringList{};
        const bool pairSortItems       = displayItems.size() == sortItems.size();

        for(int level{0}; const QString& item : displayItems) {
            const QString identityItem = resolveLibraryTreeNodeVariables(item, 0, 0);
            QString title              = identityText(trimRichText(m_formatter.evaluate(identityItem)));
            const RichText richTitle   = trimRichText(m_formatter.evaluate(identityItem));

            const auto key          = Utils::generateMd5Hash(parent->key(), title);
            const QString sortTitle = [&] {
                if(!pairSortItems) {
                    return title;
                }
                const QString sortItem = resolveLibraryTreeNodeVariables(sortItems.value(level), 0, 0);
                return identityText(trimRichText(m_formatter.evaluate(sortItem)));
            }();

            auto* node = getOrInsertItem(key, parent, title, richTitle, sortTitle.isEmpty() ? title : sortTitle, level);
            node->setTitleSource(item);

            node->addTrack(track);

            auto [batchNode, inserted] = m_data.items.try_emplace(key, LibraryTreeItem{title, nullptr, level});
            if(inserted) {
                batchNode->second.setKey(key);
                batchNode->second.setPending(true);
                batchNode->second.setTitleSource(item);
                batchNode->second.setRichTitle(richTitle);
                batchNode->second.setSortTitle(sortTitle.isEmpty() ? title : sortTitle);
            }
            batchNode->second.addTrack(track);

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
        if(!m_self->mayRun()) {
            return false;
        }

        const Track& track = m_pendingTracks.at(index);
        if(track.isInLibrary()) {
            iterateTrack(track);
        }
    }

    if(!m_self->mayRun()) {
        return false;
    }

    Q_EMIT m_self->populated(std::make_shared<PendingTreeData>(buildBatchData()));

    clearBatchData();
    m_pendingTrackIndex = batchEnd;

    const auto remaining = static_cast<int>(m_pendingTracks.size() - m_pendingTrackIndex);
    return runBatch(std::min(remaining, BatchSize));
}

LibraryTreePopulator::LibraryTreePopulator(LibraryManager* libraryManager, SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<LibraryTreePopulatorPrivate>(this, libraryManager, settings)}
{ }

LibraryTreePopulator::~LibraryTreePopulator() = default;

void LibraryTreePopulator::setFont(const QFont& font)
{
    p->m_formatter.setBaseFont(font);
}

void LibraryTreePopulator::run(const LibraryTreeGrouping& grouping, const TrackList& tracks, bool useVarious)
{
    setState(Running);

    p->m_data.clear();
    p->m_items.clear();
    p->m_touchedItems.clear();
    p->m_emittedItems.clear();
    p->m_childKeys.clear();

    p->m_scriptEnvironment.setRatingStarSymbols(Gui::ratingStarSymbols(*p->m_settings));
    p->m_scriptEnvironment.setEvaluationPolicy(TrackListContextPolicy::Unresolved, {}, true, useVarious);
    p->m_scriptEnvironment.setNodeVariablePolicy(LibraryTreeNodePolicy::Unresolved);

    if(std::exchange(p->m_currentGrouping, grouping) != grouping) {
        p->m_displayScript = p->m_parser.parse(p->m_currentGrouping.script);
        p->m_sortScript    = p->m_currentGrouping.sortScript.isEmpty()
                               ? ParsedScript{}
                               : p->m_parser.parse(p->m_currentGrouping.sortScript);
    }

    p->m_pendingTracks     = tracks;
    p->m_pendingTrackIndex = 0;
    const bool success     = p->runBatch(InitialBatchSize);

    setState(Idle);

    if(success) {
        Q_EMIT finished();
    }
}

void LibraryTreePopulator::updateItems(ItemKeyMap items, bool useVarious)
{
    setState(Running);

    p->m_scriptEnvironment.setRatingStarSymbols(Gui::ratingStarSymbols(*p->m_settings));
    p->m_scriptEnvironment.setEvaluationPolicy(TrackListContextPolicy::Unresolved, {}, true, useVarious);
    p->m_scriptEnvironment.setNodeVariablePolicy(LibraryTreeNodePolicy::Unresolved);

    PendingTreeData data;
    for(auto& [key, item] : items) {
        if(!mayRun()) {
            setState(Idle);
            return;
        }
        p->updateRichTitle(item);
        data.updatedItems.emplace(key, std::move(item));
    }

    Q_EMIT populated(std::make_shared<PendingTreeData>(std::move(data)));
    Q_EMIT finished();

    setState(Idle);
}
} // namespace Fooyin

#include "moc_librarytreepopulator.cpp"
