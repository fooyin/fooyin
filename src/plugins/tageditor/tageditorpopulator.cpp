/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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
 */

#include "tageditorpopulator.h"

#include <core/constants.h>
#include <core/trackmetadatastore.h>

#include <ranges>
#include <set>

namespace Fooyin::TagEditor {
namespace {
bool hasDefaultField(const std::vector<TagEditorField>& fields, const QString& field)
{
    return std::ranges::any_of(fields, [&field](const TagEditorField& editorField) {
        return editorField.scriptField.compare(field, Qt::CaseInsensitive) == 0;
    });
}
} // namespace

TagEditorPopulator::TagEditorPopulator(QObject* parent)
    : Worker{parent}
    , m_latestRequest{0}
{ }

void TagEditorPopulator::request(uint64_t requestId)
{
    m_latestRequest.store(requestId, std::memory_order_release);
    stopThread();
}

void TagEditorPopulator::run(uint64_t requestId, const TrackList& tracks, const std::vector<TagEditorField>& fields,
                             const QStringList& multiValueSeparators)
{
    if(requestId != m_latestRequest.load(std::memory_order_acquire)) {
        return;
    }

    setState(Running);

    auto data       = std::make_shared<TagEditorData>();
    data->requestId = requestId;
    data->tracks    = tracks;
    data->fields    = fields;

    for(const auto& field : fields) {
        if(!field.enabled) {
            continue;
        }

        const QString key = field.scriptField.toUpper();
        if(auto [it, inserted] = data->tags.emplace(key, TagEditorItem{field, nullptr}); inserted) {
            it->second.setMultiValueSeparators(multiValueSeparators);
            data->tagOrder.emplace_back(key);
        }
    }

    std::array<std::set<QString>, TagEditorCompletionDomainCount> completionSets;
    std::set<const TrackMetadataStore*> seenStores;

    const auto addCustomTags = [&data, &fields, &multiValueSeparators](const auto& tags, int previousTrackCount) {
        for(const auto& [field, value] : tags) {
            if(value.isEmpty() || hasDefaultField(fields, field)) {
                continue;
            }

            if(!data->tags.contains(field)) {
                TagEditorField editorField;
                editorField.name        = field;
                editorField.scriptField = field;
                editorField.multivalue  = Track::isMultiValueTag(field) || value.size() > 1;

                if(auto [it, inserted] = data->tags.emplace(field, TagEditorItem{editorField, nullptr}); inserted) {
                    it->second.setMultiValueSeparators(multiValueSeparators);
                    it->second.addTrack(previousTrackCount);
                    data->tagOrder.emplace_back(field);
                }
            }

            if(value.size() > 1) {
                data->tags.at(field).setFieldMultiValue(true);
            }
        }
    };

    for(int trackIndex{0}; const Track& track : tracks) {
        if(!mayRunRequest(requestId)) {
            break;
        }

        addCustomTags(track.extraTags(), trackIndex);

        for(auto& node : data->tags | std::views::values) {
            const auto result = track.metaValue(node.field().scriptField);
            node.addTrackValue(result.split(QLatin1StringView{Constants::UnitSeparator}, Qt::SkipEmptyParts));
        }

        const auto store = track.metadataStore();
        if(store && seenStores.emplace(store.get()).second) {
            for(size_t index{0}; index < TagEditorCompletionDomainCount; ++index) {
                const auto values = store->values(static_cast<StringPool::Domain>(index));
                completionSets.at(index).insert(values.cbegin(), values.cend());
            }
        }

        ++trackIndex;
    }

    if(mayRunRequest(requestId)) {
        for(size_t index{0}; index < TagEditorCompletionDomainCount; ++index) {
            auto& values = data->completionValues.at(index);
            values.reserve(static_cast<qsizetype>(completionSets.at(index).size()));
            for(const auto& value : completionSets.at(index)) {
                values.append(value);
            }
            values.sort(Qt::CaseInsensitive);
        }

        Q_EMIT populated(std::move(data));
        Q_EMIT finished();
    }

    setState(Idle);
}

bool TagEditorPopulator::mayRunRequest(uint64_t requestId) const
{
    return mayRun() && requestId == m_latestRequest.load(std::memory_order_acquire);
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorpopulator.cpp"
