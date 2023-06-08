/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "presetregistry.h"

#include "gui/guisettings.h"

#include <utils/settings/settingsmanager.h>

#include <QIODevice>

namespace Fy::Gui::Widgets::Playlist {
int find(const IndexPresetMap& presets, const QString& name)
{
    return static_cast<int>(std::count_if(presets.cbegin(), presets.cend(), [name](const auto& preset) {
        return preset.second.name == name;
    }));
}

PlaylistPreset loadDeault()
{
    PlaylistPreset preset;
    preset.name = "Default";

    preset.header.rowHeight = 76;
    preset.header.title     = "$if(%albumartist%,%albumartist%,Unknown Artist)";
    preset.header.subtitle  = "$if(%album%,%album%,Unknown Album)";
    preset.header.sideText  = "%year%";
    preset.header.titleFont.setPointSize(13);
    preset.header.subtitleFont.setPointSize(12);
    preset.header.sideTextFont.setPointSize(12);

    preset.subHeader.rowHeight = 18;
    preset.subHeader.title     = "$ifgreater(%disctotal%,1,Disc #%disc%)";
    preset.subHeader.titleFont.setPointSize(10);

    preset.track.rowHeight = 21;
    preset.track.leftText  = "%track%.  %title%";
    preset.track.rightText = "$ifgreater(%playcount%,1,|%playcount%)  $timems(%duration%)";
    preset.track.leftTextFont.setPointSize(10);
    preset.track.rightTextFont.setPointSize(10);

    return preset;
}

PresetRegistry::PresetRegistry(Utils::SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{ }

const Playlist::IndexPresetMap& PresetRegistry::presets() const
{
    return m_presets;
}

PlaylistPreset PresetRegistry::addPreset(const PlaylistPreset& preset)
{
    if(!preset.isValid()) {
        return {};
    }

    PlaylistPreset newPreset{preset};
    if(find(m_presets, newPreset.name)) {
        auto count = find(m_presets, newPreset.name + " (");
        newPreset.name += QString{" (%1)"}.arg(count);
    }
    newPreset.index = static_cast<int>(m_presets.size());

    return m_presets.emplace(newPreset.index, newPreset).first->second;
}

bool PresetRegistry::changePreset(const PlaylistPreset& preset)
{
    auto filterIt = std::find_if(m_presets.begin(), m_presets.end(), [preset](const auto& cntrPreset) {
        return cntrPreset.first == preset.index;
    });
    if(filterIt != m_presets.end()) {
        filterIt->second = preset;
        emit presetChanged(preset);
        return true;
    }
    return false;
}

PlaylistPreset PresetRegistry::presetByIndex(int index) const
{
    if(!m_presets.count(index)) {
        return {};
    }
    return m_presets.at(index);
}

PlaylistPreset PresetRegistry::presetByName(const QString& name) const
{
    if(m_presets.empty()) {
        return {};
    }
    auto it = std::find_if(m_presets.cbegin(), m_presets.cend(), [name](const auto& preset) {
        return preset.second.name == name;
    });
    if(it == m_presets.end()) {
        return m_presets.at(0);
    }
    return it->second;
}

bool PresetRegistry::removeByIndex(int index)
{
    if(!m_presets.count(index)) {
        return false;
    }
    m_presets.erase(index);
    return true;
}

void PresetRegistry::savePresets()
{
    QByteArray byteArray;
    QDataStream out(&byteArray, QIODevice::WriteOnly);

    out << m_presets;

    byteArray = byteArray.toBase64();
    m_settings->set<Settings::PlaylistPresets>(byteArray);
}

void PresetRegistry::loadPresets()
{
    QByteArray fields = m_settings->value<Settings::PlaylistPresets>();
    fields            = QByteArray::fromBase64(fields);

    QDataStream in(&fields, QIODevice::ReadOnly);

    in >> m_presets;

    if(m_presets.empty()) {
        addPreset(loadDeault());
    }
}

QDataStream& operator<<(QDataStream& stream, const IndexPresetMap& presetMap)
{
    stream << static_cast<int>(presetMap.size());
    for(const auto& [index, preset] : presetMap) {
        stream << index;
        stream << preset;
    }
    return stream;
}

QDataStream& operator>>(QDataStream& stream, IndexPresetMap& presetMap)
{
    int size;
    stream >> size;

    while(size > 0) {
        --size;

        PlaylistPreset preset;
        int index;
        stream >> index;
        stream >> preset;
        presetMap.emplace(index, preset);
    }
    return stream;
}

} // namespace Fy::Gui::Widgets::Playlist
