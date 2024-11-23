/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "alsaplugin.h"

#include "alsaoutput.h"
#include "alsasettings.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Alsa {
QString AlsaPlugin::name() const
{
    return u"ALSA"_s;
}

OutputCreator AlsaPlugin::creator() const
{
    return []() {
        return std::make_unique<AlsaOutput>();
    };
}

bool AlsaPlugin::hasSettings() const
{
    return true;
}

void AlsaPlugin::showSettings(QWidget* parent)
{
    auto* dialog = new AlsaSettings(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
} // namespace Fooyin::Alsa

#include "moc_alsaplugin.cpp"
