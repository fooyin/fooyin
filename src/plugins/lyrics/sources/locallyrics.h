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

#pragma once

#include "lyricsource.h"

#include <core/scripting/scriptparser.h>

#include <stop_token>

namespace Fooyin {
class SettingsManager;

namespace Lyrics {
class LocalLyrics : public LyricSource
{
    Q_OBJECT

public:
    using LyricSource::LyricSource;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isLocal() const override;

    void search(const SearchParams& params) override;
    void cancel() override;

private:
    ScriptParser m_parser;
    std::stop_source m_stopSource;
};
} // namespace Lyrics
} // namespace Fooyin
