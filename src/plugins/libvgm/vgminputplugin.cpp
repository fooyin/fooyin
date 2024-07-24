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

#include "vgminputplugin.h"

#include "vgminput.h"
#include "vgminputsettings.h"

namespace Fooyin::VgmInput {
QString VgmInputPlugin::name() const
{
    return QStringLiteral("VGM Input");
}

InputCreator VgmInputPlugin::inputCreator() const
{
    return []() {
        return std::make_unique<VgmInput>();
    };
}

bool VgmInputPlugin::hasSettings() const
{
    return true;
}

void VgmInputPlugin::showSettings(QWidget* parent)
{
    auto* dialog = new VgmInputSettings(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
} // namespace Fooyin::VgmInput

#include "moc_vgminputplugin.cpp"
