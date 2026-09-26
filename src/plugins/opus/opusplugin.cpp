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

#include "opusplugin.h"

#include "opusdecoder.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Opus {
QString OpusPlugin::inputName() const
{
    return u"Opus"_s;
}

InputCreator OpusPlugin::inputCreator() const
{
    InputCreator creator;
    creator.priority = 10;
    creator.decoder  = [] {
        return std::make_unique<OpusDecoder>();
    };
    return creator;
}
} // namespace Fooyin::Opus
