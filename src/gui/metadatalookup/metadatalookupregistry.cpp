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
 *
 */

#include "metadatalookupregistry.h"

#include "sources/musicbrainzmetadata.h"

namespace Fooyin {
MetadataLookupRegistry::MetadataLookupRegistry(QNetworkAccessManager* network)
{
    addSource(std::make_unique<MusicBrainzMetadata>(network));
}

MetadataLookupRegistry::~MetadataLookupRegistry() = default;

std::vector<MetadataLookupSource*> MetadataLookupRegistry::sources() const
{
    std::vector<MetadataLookupSource*> result;
    result.reserve(m_sources.size());

    for(const auto& registeredSource : m_sources) {
        result.push_back(registeredSource.get());
    }

    return result;
}

MetadataLookupSource* MetadataLookupRegistry::source(const QString& id) const
{
    const auto it = std::ranges::find(m_sources, id, &MetadataLookupSource::id);
    return it != m_sources.cend() ? it->get() : nullptr;
}

MetadataLookupSource* MetadataLookupRegistry::addSource(std::unique_ptr<MetadataLookupSource> source)
{
    if(!source || source->id().isEmpty() || this->source(source->id())) {
        return nullptr;
    }

    auto* result = source.get();
    m_sources.push_back(std::move(source));
    return result;
}
} // namespace Fooyin
