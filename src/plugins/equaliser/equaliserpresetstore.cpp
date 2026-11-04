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

#include "equaliserpresetstore.h"

#include "equaliserdsp.h"

#include <core/coresettings.h>
#include <utils/helpers.h>

#include <QDataStream>

#include <array>
#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto PresetStoreVersion = 2;
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
        m_presets.push_back({.name = std::move(name), .settings = std::move(settings), .builtInId = {}});
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

void EqualiserPresetStore::restoreBuiltInPresets()
{
    auto presets = builtInPresets();
    for(auto& preset : presets) {
        const auto existing = std::ranges::find(m_presets, preset.builtInId, &EqualiserPreset::builtInId);
        if(existing != m_presets.end()) {
            // Keep the existing name so restoring a preset can't conflict with a user preset
            existing->settings = std::move(preset.settings);
        }
        else {
            preset.name = Utils::findUniqueString(
                preset.name, m_presets, [](const EqualiserPreset& existingPreset) { return existingPreset.name; });
            m_presets.push_back(std::move(preset));
        }
    }

    save();
    Q_EMIT presetsChanged();
}

std::vector<EqualiserPreset> EqualiserPresetStore::builtInPresets()
{
    std::vector<EqualiserPreset> presets;

    const auto addPreset
        = [&presets](QString id, QString name, const std::array<double, EqualiserDsp::BandCount>& bands) {
              EqualiserDsp dsp;
              dsp.setPreampDb(-std::max(0.0, *std::ranges::max_element(bands)));
              for(int i{0}; std::cmp_less(i, bands.size()); ++i) {
                  dsp.setBandDb(i, bands[i]);
              }
              presets.push_back({.name = std::move(name), .settings = dsp.saveSettings(), .builtInId = std::move(id)});
          };

    addPreset(u"bass-boost"_s, tr("Bass boost"),
              {3.0, 3.0, 2.5, 2.0, 1.5, 1.0, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    addPreset(u"bass-reduction"_s, tr("Bass reduction"),
              {-3.0, -3.0, -2.5, -2.0, -1.5, -1.0, -0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    addPreset(u"dance"_s, tr("Dance"),
              {2.0, 1.5, 1.5, 1.5, 1.0, 0.5, 0.0, 0.0, 0.0, -0.5, -1.0, -2.0, -2.5, -3.0, -3.0, -2.0, -1.0, 0.0});
    addPreset(u"pop"_s, tr("Pop"),
              {-1.5, -0.5, 0.5, 1.5, 2.0, 2.5, 3.0, 3.0, 2.0, 1.5, 0.5, -0.5, -1.0, -2.0, -2.0, -2.0, -1.5, -1.5});
    addPreset(u"rock"_s, tr("Rock"),
              {1.5, 1.5, 1.0, 1.0, -0.5, -2.5, -2.5, -3.0, -2.0, -1.0, -0.5, 0.5, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0});
    addPreset(u"treble-boost"_s, tr("Treble boost"),
              {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.0, 3.0});
    addPreset(u"treble-reduction"_s, tr("Treble reduction"),
              {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -0.5, -1.0, -1.5, -2.0, -2.5, -3.0, -3.0, -3.0});
    addPreset(u"vocal-presence"_s, tr("Vocal presence"),
              {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 2.5, 1.5, 0.5, 0.0, 0.0, 0.0});

    return presets;
}

void EqualiserPresetStore::load()
{
    m_presets = builtInPresets();

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
    if(stream.status() != QDataStream::Ok || (version != 1 && version != PresetStoreVersion) || presetCount < 0) {
        return;
    }

    std::vector<EqualiserPreset> presets;
    for(qint32 i{0}; i < presetCount; ++i) {
        EqualiserPreset preset;
        stream >> preset.name;
        stream >> preset.settings;
        if(version >= 2) {
            stream >> preset.builtInId;
        }
        preset.name = preset.name.trimmed();

        if(stream.status() != QDataStream::Ok || preset.name.isEmpty() || preset.settings.isEmpty()) {
            return;
        }

        if(std::ranges::find(presets, preset.name, &EqualiserPreset::name) == presets.end()) {
            presets.push_back(std::move(preset));
        }
    }

    m_presets = std::move(presets);

    if(version == 1) {
        restoreBuiltInPresets();
    }
}

void EqualiserPresetStore::save() const
{
    QByteArray serializedData;
    QDataStream stream{&serializedData, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(PresetStoreVersion);
    stream << static_cast<qint32>(m_presets.size());

    for(const auto& preset : m_presets) {
        stream << preset.name;
        stream << preset.settings;
        stream << preset.builtInId;
    }

    FySettings settings;
    settings.setValue(PresetsSettingKey, qCompress(serializedData, 9));
}
} // namespace Fooyin::Equaliser
