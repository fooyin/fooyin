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

#include "librarytreescriptregistry.h"

#include <core/constants.h>

namespace Fooyin {
bool LibraryTreeScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    if(var == u"frontcover") {
        return true;
    }
    if(var == u"backcover") {
        return true;
    }
    if(var == u"artistpicture") {
        return true;
    }
    return ScriptRegistry::isVariable(var, track);
}

ScriptResult LibraryTreeScriptRegistry::value(const QString& var, const Track& track) const
{
    if(var == u"frontcover") {
        return {.value = QLatin1String{Constants::FrontCover}, .cond = true};
    }
    if(var == u"backcover") {
        return {.value = QLatin1String{Constants::BackCover}, .cond = true};
    }
    if(var == u"artistpicture") {
        return {.value = QLatin1String{Constants::ArtistPicture}, .cond = true};
    }
    return ScriptRegistry::value(var, track);
}
} // namespace Fooyin
