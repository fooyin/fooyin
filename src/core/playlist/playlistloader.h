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

#pragma once

#include "fycore_export.h"

#include <memory>
#include <shared_mutex>

#include <QStringList>

namespace Fooyin {
class PlaylistParser;

class FYCORE_EXPORT PlaylistLoader
{
public:
    PlaylistParser* addParser(std::unique_ptr<PlaylistParser> parser);

    [[nodiscard]] QStringList supportedExtensions() const;
    [[nodiscard]] QStringList supportedSaveExtensions() const;
    [[nodiscard]] PlaylistParser* parserForExtension(const QString& extension) const;

private:
    std::unordered_map<QString, std::unique_ptr<PlaylistParser>> m_parsers;
    mutable std::shared_mutex m_parserMutex;
};
} // namespace Fooyin
