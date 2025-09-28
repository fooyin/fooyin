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

#include "openmptplugin.h"

#include "openmptdefs.h"
#include "openmptinput.h"
#include "openmptsettings.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin::OpenMpt {
void OpenMptPlugin::initialise(const CorePluginContext& context)
{
    m_settings = context.settingsManager;

    m_settings->createSetting<Settings::OpenMpt::Gain>(0.0, u"OpenMpt/Gain"_s);
    m_settings->createSetting<Settings::OpenMpt::Separation>(100, u"OpenMpt/Separation"_s);
    m_settings->createSetting<Settings::OpenMpt::VolumeRamping>(-1, u"OpenMpt/VolumeRamping"_s);
    m_settings->createSetting<Settings::OpenMpt::InterpolationFilter>(0, u"OpenMpt/InterpolationFilter"_s);
    m_settings->createSetting<Settings::OpenMpt::EmulateAmiga>(true, u"OpenMpt/EmulateAmiga"_s);
    m_settings->createSetting<Settings::OpenMpt::LoopCount>(0, u"OpenMpt/LoopCount"_s);
}

QString OpenMptPlugin::inputName() const
{
    return u"OpenMpt"_s;
}

InputCreator OpenMptPlugin::inputCreator() const
{
    InputCreator creator;
    creator.decoder = [this]() {
        return std::make_unique<OpenMptDecoder>(m_settings);
    };
    creator.reader = [this]() {
        return std::make_unique<OpenMptReader>(m_settings);
    };
    return creator;
}

bool OpenMptPlugin::hasSettings() const
{
    return true;
}

void OpenMptPlugin::showSettings(QWidget* parent)
{
    auto* dialog = new OpenMptSettings(m_settings, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
} // namespace Fooyin::OpenMpt

#include "moc_openmptplugin.cpp"
