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

#pragma once

#include <gui/dsp/dspsettingsprovider.h>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;

namespace Fooyin::Soxr {
class SoxResamplerSettingsWidget : public DspSettingsDialog
{
public:
    explicit SoxResamplerSettingsWidget(QWidget* parent = nullptr);

    void loadSettings(const QByteArray& settings) override;
    [[nodiscard]] QByteArray saveSettings() const override;

protected:
    void restoreDefaults() override;

private:
    void updatePassbandLabel();

    QComboBox* m_rateCombo;
    QComboBox* m_qualityCombo;
    QSlider* m_passbandSlider;
    QLabel* m_passbandValueLabel;
    QCheckBox* m_fullBandwidthCheck;
    QComboBox* m_phaseResponseCombo;
    QComboBox* m_filterModeCombo;
    QLineEdit* m_filteredRatesEdit;
};

class SoxResamplerSettingsProvider : public DspSettingsProvider
{
public:
    [[nodiscard]] QString id() const override;
    DspSettingsDialog* createSettingsWidget(QWidget* parent) override;
};
} // namespace Fooyin::Soxr
