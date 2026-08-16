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

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <vector>

namespace Fooyin::Equaliser {
struct EqualiserPreset
{
    QString name;
    QByteArray settings;
};

class EqualiserPresetStore : public QObject
{
    Q_OBJECT

public:
    explicit EqualiserPresetStore(QObject* parent = nullptr);

    [[nodiscard]] const std::vector<EqualiserPreset>& presets() const;
    [[nodiscard]] int indexByName(const QString& name) const;

    void setPreset(QString name, QByteArray settings);
    void removePreset(const QString& name);

Q_SIGNALS:
    void presetsChanged();

private:
    void load();
    void save() const;

    std::vector<EqualiserPreset> m_presets;
};
} // namespace Fooyin::Equaliser
