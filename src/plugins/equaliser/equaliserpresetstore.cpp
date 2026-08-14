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
 */

#include "equaliserpresetstore.h"

#include <core/coresettings.h>

#include <QDataStream>

#include <algorithm>

using namespace Qt::StringLiterals;

constexpr auto PresetStoreVersion = 1;
constexpr auto PresetsSettingKey  = "DSP/EqualiserPresets"_L1;

namespace Fooyin::Equaliser {
EqualiserPresetStore::EqualiserPresetStore(QObject* parent)
    : QObject{parent}
{
    load();
}

const std::vector<EqualiserPreset>& EqualiserPresetStore::presets() const
{
    return m_presets;
}

int EqualiserPresetStore::indexByName(const QString& name) const
{
    const QString trimmedName = name.trimmed();
    if(trimmedName.isEmpty()) {
        return -1;
    }

    const auto preset = std::ranges::find(m_presets, trimmedName, &EqualiserPreset::name);
    return preset == m_presets.end() ? -1 : static_cast<int>(std::distance(m_presets.begin(), preset));
}

void EqualiserPresetStore::setPreset(QString name, QByteArray settings)
{
    name = name.trimmed();
    if(name.isEmpty() || settings.isEmpty()) {
        return;
    }

    const int index = indexByName(name);
    if(index >= 0) {
        m_presets[index].settings = std::move(settings);
    }
    else {
        m_presets.push_back({.name = std::move(name), .settings = std::move(settings)});
    }

    save();
    Q_EMIT presetsChanged();
}

void EqualiserPresetStore::removePreset(const QString& name)
{
    const int index = indexByName(name);
    if(index < 0) {
        return;
    }

    m_presets.erase(m_presets.begin() + index);
    save();
    Q_EMIT presetsChanged();
}

void EqualiserPresetStore::load()
{
    m_presets.clear();

    const FySettings settings;
    auto serializedData = settings.value(PresetsSettingKey).toByteArray();
    if(serializedData.isEmpty()) {
        return;
    }

    serializedData = qUncompress(serializedData);
    if(serializedData.isEmpty()) {
        return;
    }

    QDataStream stream{&serializedData, QIODevice::ReadOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 version{0};
    qint32 presetCount{0};
    stream >> version;
    stream >> presetCount;
    if(stream.status() != QDataStream::Ok || version != PresetStoreVersion || presetCount < 0) {
        return;
    }

    for(qint32 i{0}; i < presetCount; ++i) {
        EqualiserPreset preset;
        stream >> preset.name;
        stream >> preset.settings;
        preset.name = preset.name.trimmed();

        if(stream.status() != QDataStream::Ok || preset.name.isEmpty() || preset.settings.isEmpty()) {
            m_presets.clear();
            return;
        }

        if(indexByName(preset.name) < 0) {
            m_presets.push_back(std::move(preset));
        }
    }
}

void EqualiserPresetStore::save() const
{
    FySettings settings;
    if(m_presets.empty()) {
        settings.remove(PresetsSettingKey);
        return;
    }

    QByteArray serializedData;
    QDataStream stream{&serializedData, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(PresetStoreVersion);
    stream << static_cast<qint32>(m_presets.size());

    for(const auto& preset : m_presets) {
        stream << preset.name;
        stream << preset.settings;
    }

    settings.setValue(PresetsSettingKey, qCompress(serializedData, 9));
}
} // namespace Fooyin::Equaliser
