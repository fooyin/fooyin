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

#include "fyutils_export.h"

#include <QVariant>

namespace Fooyin {
namespace Settings {
enum Type : uint32_t
{
    Variant   = 0 << 28,
    Bool      = 1 << 28,
    Int       = 2 << 28,
    Double    = 3 << 28,
    String    = 4 << 28,
    ByteArray = 5 << 28,
};
}

class FYUTILS_EXPORT SettingsEntry : public QObject
{
    Q_OBJECT

public:
    SettingsEntry(QString name, const QVariant& value, QObject* parent = nullptr);
    SettingsEntry(QString name, const QVariant& value, Settings::Type type, QObject* parent = nullptr);

    [[nodiscard]] QString key() const;
    [[nodiscard]] Settings::Type type() const;
    [[nodiscard]] QVariant value() const;
    [[nodiscard]] QVariant defaultValue() const;
    [[nodiscard]] bool isTemporary() const;

    bool setValue(const QVariant& value);
    void setIsTemporary(bool isTemporary);

    bool reset();

signals:
    void settingChangedVariant(const QVariant& value);
    void settingChangedBool(bool value);
    void settingChangedInt(int value);
    void settingChangedDouble(double value);
    void settingChangedString(const QString& value);
    void settingChangedByteArray(const QByteArray& value);

private:
    QString m_key;
    Settings::Type m_type;
    QVariant m_value;
    QVariant m_defaultValue;
    bool m_isTemporary;
};
} // namespace Fooyin
