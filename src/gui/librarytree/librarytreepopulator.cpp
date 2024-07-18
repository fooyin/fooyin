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

#include "librarytreepopulator.h"

#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>

constexpr int InitialBatchSize = 3000;
constexpr int BatchSize        = 4000;

namespace Fooyin {
class LibraryTreePopulatorPrivate
{
public:
    explicit LibraryTreePopulatorPrivate(LibraryTreePopulator* self, LibraryManager* libraryManager)
        : m_self{self}
        , m_registry{libraryManager}
        , m_parser{&m_registry}
        , m_data{}
    { }

    LibraryTreeItem* getOrInsertItem(const Md5Hash& key, const LibraryTreeItem* parent, const QString& title,
                                     int level);
    void iterateTrack(const Track& track);
    bool runBatch(int size);

    LibraryTreePopulator* m_self;

    ScriptRegistry m_registry;
    ScriptParser m_parser;

    QString m_currentGrouping;
    ParsedScript m_script;

    LibraryTreeItem m_root;
    PendingTreeData m_data;
    TrackList m_pendingTracks;
};

LibraryTreeItem* LibraryTreePopulatorPrivate::getOrInsertItem(const Md5Hash& key, const LibraryTreeItem* parent,
                                                              const QString& title, int level)
{
    auto [node, inserted] = m_data.items.try_emplace(key, LibraryTreeItem{title, nullptr, level});
    if(inserted) {
        node->second.setKey(key);
    }
    LibraryTreeItem* child = &node->second;

    if(!child->pending()) {
        child->setPending(true);
        m_data.nodes[parent->key()].push_back(key);
    }
    return child;
}

void LibraryTreePopulatorPrivate::iterateTrack(const Track& track)
{
    const QString field = m_parser.evaluate(m_script, track);
    if(field.isNull()) {
        return;
    }

    const QStringList values = field.split(u'\037', Qt::SkipEmptyParts);
    for(const QString& value : values) {
        if(value.isNull()) {
            continue;
        }

        LibraryTreeItem* parent = &m_root;
        const QStringList items = value.split(QStringLiteral("||"));

        for(int level{0}; const QString& item : items) {
            const QString title = item.trimmed();
            const auto key      = Utils::generateMd5Hash(parent->key(), title);

            auto* node = getOrInsertItem(key, parent, title, level);

            node->addTrack(track);
            m_data.trackParents[track.id()].push_back(node->key());

            parent = node;
            ++level;
        }
    }
}

bool LibraryTreePopulatorPrivate::runBatch(int size)
{
    if(size <= 0) {
        return true;
    }

    auto tracksBatch = std::ranges::views::take(m_pendingTracks, size);

    for(const Track& track : tracksBatch) {
        if(!m_self->mayRun()) {
            return false;
        }

        if(track.isInLibrary()) {
            iterateTrack(track);
        }
    }

    if(!m_self->mayRun()) {
        return false;
    }

    emit m_self->populated(m_data);

    auto tracksToKeep = std::ranges::views::drop(m_pendingTracks, size);
    TrackList tempTracks;
    std::ranges::copy(tracksToKeep, std::back_inserter(tempTracks));
    m_pendingTracks = std::move(tempTracks);

    m_data.clear();

    const auto remaining = static_cast<int>(m_pendingTracks.size());
    return runBatch(std::min(remaining, BatchSize));
}

LibraryTreePopulator::LibraryTreePopulator(LibraryManager* libraryManager, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<LibraryTreePopulatorPrivate>(this, libraryManager)}
{ }

LibraryTreePopulator::~LibraryTreePopulator() = default;

void LibraryTreePopulator::run(const QString& grouping, const TrackList& tracks)
{
    setState(Running);

    p->m_data.clear();

    if(std::exchange(p->m_currentGrouping, grouping) != grouping) {
        p->m_script = p->m_parser.parse(p->m_currentGrouping);
    }

    p->m_pendingTracks = tracks;
    const bool success = p->runBatch(InitialBatchSize);

    setState(Idle);

    if(success) {
        emit finished();
    }
}
} // namespace Fooyin

#include "moc_librarytreepopulator.cpp"
