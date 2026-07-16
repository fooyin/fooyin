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

#include <core/engine/audioencoder.h>

#include <QDialog>

#include <vector>

class QComboBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;

namespace Fooyin {
class DoubleSliderEditor;
class SliderEditor;

class EncoderProfileDialog : public QDialog
{
    Q_OBJECT

public:
    EncoderProfileDialog(std::vector<AudioEncoderInfo> availableEncoders, const AudioEncoderInfo& initial,
                         bool allowEncoderChange, QWidget* parent = nullptr);

    [[nodiscard]] AudioEncoderInfo encoderInfo() const;

private:
    void encoderChanged(int index);
    void updateMode();
    void updateBitrateEstimate();
    void updateAcceptState() const;

    std::vector<AudioEncoderInfo> m_availableEncoders;
    QComboBox* m_encoder;
    QLineEdit* m_name;
    QGroupBox* m_options;
    QGridLayout* m_optionsLayout;
    QLabel* m_modeLabel;
    QLabel* m_qualityLabel;
    QLabel* m_bitrateEstimateLabel;
    QLabel* m_bitrateEstimate;
    QLabel* m_bitrateLabel;
    QLabel* m_levelLabel;
    QComboBox* m_mode;
    DoubleSliderEditor* m_quality;
    SliderEditor* m_bitrate;
    SliderEditor* m_level;
    QPushButton* m_okButton;
};
} // namespace Fooyin
