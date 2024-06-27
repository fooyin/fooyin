/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/tagging/tagloader.h>

#include <mutex>
#include <shared_mutex>

namespace Fooyin {
struct TagLoader::Private
{
    std::unordered_map<QString, std::unique_ptr<TagParser>> m_parsers;
    std::unordered_map<QString, std::vector<TagParser*>> m_extensionToParserMap;
    std::shared_mutex m_parserMutex;
};

TagLoader::TagLoader()
    : p{std::make_unique<Private>()}
{ }

TagLoader::~TagLoader() = default;

TagParser* TagLoader::parserForTrack(const Track& track) const
{
    const std::shared_lock lock{p->m_parserMutex};

    if(p->m_extensionToParserMap.contains(track.extension())) {
        return p->m_extensionToParserMap.at(track.extension()).front();
    }

    return nullptr;
}

void TagLoader::addParser(TagParserContext parserContext)
{
    const std::unique_lock lock{p->m_parserMutex};

    const auto& name = parserContext.name;

    if(p->m_parsers.contains(name)) {
        qDebug() << QStringLiteral("Tag Parser (%1) already registered").arg(name);
        return;
    }

    // TODO: Add order/priority to handle multiple parsers supporting same extensions
    auto extensions = parserContext.parser->supportedExtensions();
    for(const auto& extension : extensions) {
        p->m_extensionToParserMap[extension].emplace_back(parserContext.parser.get());
    }

    p->m_parsers.emplace(parserContext.name, std::move(parserContext.parser));
}
} // namespace Fooyin
