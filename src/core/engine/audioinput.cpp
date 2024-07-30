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

#include <core/engine/audioinput.h>

namespace Fooyin {
bool AudioDecoder::trackHasChanged() const
{
    return false;
}

Track AudioDecoder::changedTrack() const
{
    return {};
}

void AudioDecoder::start() { }

int AudioReader::subsongCount() const
{
    return 1;
}

bool AudioReader::init(const QString& /*file*/)
{
    return true;
}

QByteArray AudioReader::readCover(const Track& /*track*/, Track::Cover /*cover*/)
{
    return {};
}

bool AudioReader::writeTrack(const Track& /*track*/, WriteOptions /*options*/)
{
    return false;
}
} // namespace Fooyin
