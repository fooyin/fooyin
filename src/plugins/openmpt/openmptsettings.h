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

#include <QDialog>
#include <QSpinBox>

class QCheckBox;
class QComboBox;

namespace Fooyin {
class DoubleSliderEditor;
class SettingsManager;
class SliderEditor;

namespace OpenMpt {
class OpenMptSettings : public QDialog
{
    Q_OBJECT

public:
    explicit OpenMptSettings(SettingsManager* settings, QWidget* parent = nullptr);

    void accept() override;

private:
    void load();
    void reset();

    SettingsManager* m_settings;

    DoubleSliderEditor* m_gain;
    SliderEditor* m_separation;
    SliderEditor* m_volRamping;
    QCheckBox* m_amigaResampler;
    QComboBox* m_interpolationFilter;
    QSpinBox* m_loopCount;
};
} // namespace OpenMpt
} // namespace Fooyin
