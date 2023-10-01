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

#pragma once

#include <QVariant>

namespace Fy::Utils {
class SettingsEntry : public QObject
{
    Q_OBJECT

public:
    explicit SettingsEntry(QString name, const QVariant& value, bool writeToDisk, QString group = {});

    SettingsEntry(const SettingsEntry& other);

    [[nodiscard]] QString name() const;
    [[nodiscard]] QVariant value() const;
    [[nodiscard]] QVariant defaultValue() const;
    [[nodiscard]] QString group() const;
    [[nodiscard]] bool writeToDisk() const;

    bool setValue(const QVariant& value);
    bool reset();

signals:
    void settingChanged();
    void settingChangedNone(QVariant value);
    void settingChangedBool(bool value);
    void settingChangedInt(int value);
    void settingChangedDouble(double value);
    void settingChangedString(QString value);
    void settingChangedByteArray(QByteArray value);

private:
    QString m_name;
    QVariant m_value;
    QVariant m_defaultValue;
    QString m_group;
    bool m_writeToDisk;
};
} // namespace Fy::Utils
