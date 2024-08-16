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

#pragma once

#include <core/coresettings.h>

#include <QDialog>

class QSpinBox;

namespace Fooyin {
constexpr auto BufferLengthSetting = "ALSA/BufferLength";
constexpr auto DefaultBufferLength = 200;
constexpr auto PeriodLengthSetting = "ALSA/PeriodLength";
constexpr auto DefaultPeriodLength = 40;

class AlsaSettings : public QDialog
{
    Q_OBJECT

public:
    explicit AlsaSettings(QWidget* parent = nullptr);

    void accept() override;

private:
    FySettings m_settings;
    QSpinBox* m_bufferLength;
    QSpinBox* m_periodLength;
};
} // namespace Fooyin
