/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlistloader.h>

#include <core/playlist/playlistparser.h>
#include <core/track.h>

#include <QBuffer>
#include <QUrl>

#include <mutex>
#include <ranges>

namespace Fooyin {
namespace {
QStringList trackUrls(const TrackList& tracks)
{
    QStringList urls;
    for(const Track& track : tracks) {
        if(!track.filepath().isEmpty()) {
            urls.emplace_back(track.filepath());
        }
    }
    return urls;
}
} // namespace

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

    for(const auto& parser : m_parsers | std::views::values) {
        extensions.append(parser->supportedExtensions());
    }

    return extensions;
}

QStringList PlaylistLoader::supportedSaveExtensions() const
{
    const std::shared_lock lock{m_parserMutex};

    QStringList extensions;

    for(const auto& parser : m_parsers | std::views::values) {
        if(parser->saveIsSupported()) {
            extensions.append(parser->supportedExtensions());
        }
    }

    return extensions;
}

PlaylistParser* PlaylistLoader::parserForExtension(const QString& extension) const
{
    const std::shared_lock lock{m_parserMutex};

    for(const auto& parser : m_parsers | std::views::values) {
        if(parser->supportedExtensions().contains(extension)) {
            return parser.get();
        }
    }

    return nullptr;
}

PlaylistParser* PlaylistLoader::parserForData(const QByteArray& data, const QString& contentType, const QUrl& url) const
{
    const std::shared_lock lock{m_parserMutex};

    for(const auto& parser : m_parsers | std::views::values) {
        if(parser->canParse(data, contentType, url)) {
            return parser.get();
        }
    }

    return nullptr;
}

QStringList PlaylistLoader::resolveUrls(const QUrl& baseUrl, const QByteArray& data, const QString& contentType) const
{
    if(data.isEmpty()) {
        return {};
    }

    const std::shared_lock lock{m_parserMutex};

    PlaylistParser* parser{nullptr};
    for(const auto& candidate : m_parsers | std::views::values) {
        if(candidate->canParse(data, contentType, baseUrl)) {
            parser = candidate.get();
            break;
        }
    }

    if(!parser) {
        return {};
    }

    QByteArray bytes{data};
    QBuffer buffer{&bytes};
    if(!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [](const Track& track) {
        return track;
    };

    return trackUrls(parser->readPlaylist(&buffer, baseUrl.toString(), {}, readEntry, false));
}
} // namespace Fooyin
