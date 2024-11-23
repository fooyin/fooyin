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

#include "rawaudioplugin.h"

#include "rawaudioinput.h"

using namespace Qt::StringLiterals;

namespace Fooyin::RawAudio {
QString RawAudioPlugin::inputName() const
{
    return u"RawAudio"_s;
}

InputCreator RawAudioPlugin::inputCreator() const
{
    InputCreator creator;
    creator.decoder = []() {
        return std::make_unique<RawAudioDecoder>();
    };
    creator.reader = []() {
        return std::make_unique<RawAudioReader>();
    };
    return creator;
}
} // namespace Fooyin::RawAudio

#include "moc_rawaudioplugin.cpp"
