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

#include "vorbisplugin.h"

#include "vorbisdecoder.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Vorbis {
QString VorbisPlugin::inputName() const
{
    return u"Vorbis"_s;
}

InputCreator VorbisPlugin::inputCreator() const
{
    InputCreator creator;
    creator.priority = 10;
    creator.decoder  = [] {
        return std::make_unique<VorbisDecoder>();
    };
    return creator;
}
} // namespace Fooyin::Vorbis
