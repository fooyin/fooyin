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

#include "playlistloader.h"

#include <core/playlist/playlistparser.h>

#include <mutex>

namespace Fooyin {
PlaylistParser* PlaylistLoader::addParser(std::unique_ptr<PlaylistParser> parser)
{
    const std::unique_lock lock{m_parserMutex};

    const QString name = parser->name();
    if(m_parsers.contains(name)) {
        return m_parsers.at(name).get();
    }

    return m_parsers.emplace(name, std::move(parser)).first->second.get();
}

QStringList PlaylistLoader::supportedExtensions() const
{
    const std::shared_lock lock{m_parserMutex};

    QStringList extensions;

    for(const auto& [_, parser] : m_parsers) {
        extensions.append(parser->supportedExtensions());
    }

    return extensions;
}

QStringList PlaylistLoader::supportedSaveExtensions() const
{
    const std::shared_lock lock{m_parserMutex};

    QStringList extensions;

    for(const auto& [_, parser] : m_parsers) {
        if(parser->saveIsSupported()) {
            extensions.append(parser->supportedExtensions());
        }
    }

    return extensions;
}

PlaylistParser* PlaylistLoader::parserForExtension(const QString& extension) const
{
    const std::shared_lock lock{m_parserMutex};

    for(const auto& [_, parser] : m_parsers) {
        if(parser->supportedExtensions().contains(extension)) {
            return parser.get();
        }
    }

    return nullptr;
}
} // namespace Fooyin
