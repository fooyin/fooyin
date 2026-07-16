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

#include "encoderprofiledialog.h"

#include <gui/widgets/doubleslidereditor.h>
#include <gui/widgets/slidereditor.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString modeName(EncoderMode mode)
{
    switch(mode) {
        case EncoderMode::Default:
            return EncoderProfileDialog::tr("Default");
        case EncoderMode::VariableBitrate:
            return EncoderProfileDialog::tr("Variable bitrate (VBR)");
        case EncoderMode::ConstrainedVariableBitrate:
            return EncoderProfileDialog::tr("Constrained variable bitrate");
        case EncoderMode::ConstantQuality:
            return EncoderProfileDialog::tr("Constant quality (VBR)");
        case EncoderMode::AverageBitrate:
            return EncoderProfileDialog::tr("Average bitrate (ABR)");
        case EncoderMode::ConstantBitrate:
            return EncoderProfileDialog::tr("Constant bitrate (CBR)");
        case EncoderMode::LosslessCompression:
            return EncoderProfileDialog::tr("Lossless compression");
    }
    return {};
}
} // namespace

EncoderProfileDialog::EncoderProfileDialog(std::vector<AudioEncoderInfo> availableEncoders,
                                           const AudioEncoderInfo& initial, bool allowEncoderChange, QWidget* parent)
    : QDialog{parent}
    , m_availableEncoders{std::move(availableEncoders)}
    , m_encoder{new QComboBox(this)}
    , m_name{new QLineEdit(this)}
    , m_options{new QGroupBox(tr("Options"), this)}
    , m_optionsLayout{new QGridLayout(m_options)}
    , m_modeLabel{new QLabel(tr("Mode") + ":"_L1, m_options)}
    , m_qualityLabel{new QLabel(tr("Quality") + ":"_L1, m_options)}
    , m_bitrateEstimateLabel{new QLabel(tr("Estimated bitrate") + ":"_L1, m_options)}
    , m_bitrateEstimate{new QLabel(m_options)}
    , m_bitrateLabel{new QLabel(tr("Bitrate") + ":"_L1, m_options)}
    , m_levelLabel{new QLabel(tr("Level") + ":"_L1, m_options)}
    , m_mode{new QComboBox(this)}
    , m_quality{new DoubleSliderEditor(this)}
    , m_bitrate{new SliderEditor(this)}
    , m_level{new SliderEditor(this)}
    , m_okButton{nullptr}
{
    setWindowTitle(tr("Encoder Profile"));
    setModal(true);
    resize(520, 300);

    for(const AudioEncoderInfo& encoder : std::as_const(m_availableEncoders)) {
        m_encoder->addItem(encoder.name);
    }
    m_encoder->setEnabled(allowEncoderChange);

    m_bitrate->setSuffix(u" kbps"_s);
    m_quality->setTicksVisible(true);
    m_bitrate->setTicksVisible(true);
    m_level->setTicksVisible(true);

    int row{0};
    m_optionsLayout->addWidget(m_modeLabel, row, 0);
    m_optionsLayout->addWidget(m_mode, row++, 1);
    m_optionsLayout->addWidget(m_qualityLabel, row, 0);
    m_optionsLayout->addWidget(m_quality, row++, 1);
    m_optionsLayout->addWidget(m_bitrateEstimateLabel, row, 0);
    m_optionsLayout->addWidget(m_bitrateEstimate, row++, 1);
    m_optionsLayout->addWidget(m_bitrateLabel, row, 0);
    m_optionsLayout->addWidget(m_bitrate, row++, 1);
    m_optionsLayout->addWidget(m_levelLabel, row, 0);
    m_optionsLayout->addWidget(m_level, row++, 1);
    m_optionsLayout->setColumnStretch(1, 1);

    auto* encoderGroup  = new QGroupBox(tr("Encoder"), this);
    auto* encoderLayout = new QGridLayout(encoderGroup);

    row = 0;
    encoderLayout->addWidget(new QLabel(tr("Format") + ":"_L1, encoderGroup), row, 0);
    encoderLayout->addWidget(m_encoder, row++, 1);
    encoderLayout->addWidget(new QLabel(tr("Name") + ":"_L1, encoderGroup), row, 0);
    encoderLayout->addWidget(m_name, row++, 1);
    encoderLayout->setColumnStretch(1, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton      = buttonBox->button(QDialogButtonBox::Ok);

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_encoder, &QComboBox::currentIndexChanged, this, &EncoderProfileDialog::encoderChanged);
    QObject::connect(m_name, &QLineEdit::textChanged, this, &EncoderProfileDialog::updateAcceptState);
    QObject::connect(m_mode, &QComboBox::currentIndexChanged, this, &EncoderProfileDialog::updateMode);
    QObject::connect(m_quality, &DoubleSliderEditor::valueChanged, this, &EncoderProfileDialog::updateBitrateEstimate);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(encoderGroup);
    layout->addWidget(m_options);
    layout->addStretch();
    layout->addWidget(buttonBox);

    int initialIndex{0};
    for(int i{0}; std::cmp_less(i, m_availableEncoders.size()); ++i) {
        if(m_availableEncoders.at(i).id == initial.id) {
            initialIndex = i;
            break;
        }
    }
    m_encoder->setCurrentIndex(initialIndex);
    encoderChanged(initialIndex);

    m_name->setText(initial.profile.name.isEmpty() ? initial.name : initial.profile.name);
    const int modeIndex = m_mode->findData(static_cast<int>(initial.profile.mode));
    if(modeIndex >= 0) {
        m_mode->setCurrentIndex(modeIndex);
    }

    m_quality->setValue(initial.profile.quality);
    m_bitrate->setValue(initial.profile.bitrateKbps);
    m_level->setValue(std::max(0, initial.profile.compressionLevel));

    updateMode();
    updateAcceptState();
}

void EncoderProfileDialog::encoderChanged(int index)
{
    if(index < 0 || std::cmp_greater_equal(index, m_availableEncoders.size())) {
        m_options->hide();
        updateAcceptState();
        return;
    }

    const AudioEncoderInfo& encoder = m_availableEncoders.at(index);
    m_mode->clear();
    for(const EncoderMode mode : encoder.capabilities.modes) {
        m_mode->addItem(modeName(mode), static_cast<int>(mode));
    }
    const int modeIndex = m_mode->findData(static_cast<int>(encoder.profile.mode));
    m_mode->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);

    const auto configureRange = [](auto* editor, const EncoderParameterRange& range) {
        editor->setRange(range.minimum, range.maximum);
        editor->setSingleStep(range.step);
    };
    if(encoder.capabilities.quality.isValid()) {
        configureRange(m_quality, encoder.capabilities.quality);
    }
    else {
        m_quality->setRange(0.0, 1.0);
    }
    if(encoder.capabilities.bitrateKbps.isValid()) {
        configureRange(m_bitrate, encoder.capabilities.bitrateKbps);
    }
    else {
        m_bitrate->setRange(0, 0);
    }
    if(encoder.capabilities.compressionLevel.isValid()) {
        configureRange(m_level, encoder.capabilities.compressionLevel);
    }
    else {
        m_level->setRange(0, 0);
    }

    m_quality->setValue(encoder.profile.quality);
    m_bitrate->setValue(encoder.profile.bitrateKbps);
    m_level->setValue(std::max(0, encoder.profile.compressionLevel));
    updateMode();

    m_name->setText(encoder.profile.name.isEmpty() ? encoder.name : encoder.profile.name);
    updateAcceptState();
}

void EncoderProfileDialog::updateMode()
{
    const auto mode        = static_cast<EncoderMode>(m_mode->currentData().toInt());
    const bool bitrate     = mode == EncoderMode::VariableBitrate || mode == EncoderMode::ConstrainedVariableBitrate
                          || mode == EncoderMode::AverageBitrate || mode == EncoderMode::ConstantBitrate;
    const bool showMode    = m_mode->count() > 1;
    const bool showQuality = mode == EncoderMode::ConstantQuality && m_quality->maximum() > m_quality->minimum();
    const bool showBitrate = bitrate && m_bitrate->maximum() > m_bitrate->minimum();
    const bool showLevel   = m_level->maximum() > m_level->minimum();

    const auto setRowVisible = [](QWidget* label, QWidget* field, bool visible) {
        label->setVisible(visible);
        field->setVisible(visible);
    };

    setRowVisible(m_modeLabel, m_mode, showMode);
    setRowVisible(m_qualityLabel, m_quality, showQuality);
    setRowVisible(m_bitrateLabel, m_bitrate, showBitrate);
    setRowVisible(m_levelLabel, m_level, showLevel);
    m_options->setVisible(showMode || showQuality || showBitrate || showLevel);

    m_levelLabel->setText(mode == EncoderMode::LosslessCompression ? tr("Compression level") + ":"_L1
                                                                   : tr("Encoding effort") + ":"_L1);
    updateBitrateEstimate();
}

void EncoderProfileDialog::updateBitrateEstimate()
{
    const int index = m_encoder->currentIndex();
    if(index < 0 || std::cmp_greater_equal(index, m_availableEncoders.size())) {
        m_bitrateEstimateLabel->hide();
        m_bitrateEstimate->hide();
        return;
    }

    const AudioEncoderInfo& encoder = m_availableEncoders.at(index);
    EncoderProfile profile{encoder.profile};
    profile.mode    = static_cast<EncoderMode>(m_mode->currentData().toInt());
    profile.quality = m_quality->value();

    const int estimate = encoder.estimateBitrateKbps && profile.mode == EncoderMode::ConstantQuality
                           ? encoder.estimateBitrateKbps(profile)
                           : 0;
    m_bitrateEstimate->setText(estimate > 0 ? u"~%1 kbps"_s.arg(estimate) : QString{});
    m_bitrateEstimateLabel->setVisible(estimate > 0);
    m_bitrateEstimate->setVisible(estimate > 0);
}

void EncoderProfileDialog::updateAcceptState() const
{
    m_okButton->setEnabled(m_encoder->currentIndex() >= 0 && !m_name->text().trimmed().isEmpty());
}

AudioEncoderInfo EncoderProfileDialog::encoderInfo() const
{
    const int index = m_encoder->currentIndex();
    if(index < 0 || std::cmp_greater_equal(index, m_availableEncoders.size())) {
        return {};
    }

    AudioEncoderInfo result         = m_availableEncoders.at(index);
    result.name                     = m_name->text().trimmed();
    result.profile.name             = result.name;
    result.profile.mode             = static_cast<EncoderMode>(m_mode->currentData().toInt());
    result.profile.quality          = m_quality->value();
    result.profile.bitrateKbps      = m_bitrate->value();
    result.profile.compressionLevel = result.capabilities.compressionLevel.isValid() ? m_level->value() : -1;
    return result;
}
} // namespace Fooyin

#include "moc_encoderprofiledialog.cpp"
