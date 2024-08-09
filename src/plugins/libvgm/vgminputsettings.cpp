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

#include "vgminputsettings.h"

#include "vgminputdefs.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace Fooyin::VgmInput {
VgmInputSettings::VgmInputSettings(QWidget* parent)
    : QDialog{parent}
    , m_loopCount{new QSpinBox(this)}
    , m_fadeLength{new QSpinBox(this)}
    , m_silenceLength{new QSpinBox(this)}
    , m_guessTrack{new QCheckBox(tr("Guess track number from filename"), this)}
    , m_romLocation{new QLineEdit(this)}
{
    setWindowTitle(tr("VGM Input Settings"));
    setModal(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &VgmInputSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &VgmInputSettings::reject);

    auto* lengthGroup  = new QGroupBox(tr("Length"), this);
    auto* lengthLayout = new QGridLayout(lengthGroup);

    auto* loopLabel     = new QLabel(tr("Loop count") + QStringLiteral(":"), this);
    auto* loopHintLabel = new QLabel(tr("(0 = infinite)"), this);

    m_loopCount->setRange(0, 16);
    m_loopCount->setSingleStep(1);
    m_loopCount->setSuffix(tr(" times"));

    auto* fadeLabel = new QLabel(tr("Fade length") + QStringLiteral(":"), this);

    m_fadeLength->setRange(0, 10000);
    m_fadeLength->setSingleStep(500);
    m_fadeLength->setSuffix(tr(" ms"));

    auto* silenceLabel = new QLabel(tr("End silence length") + QStringLiteral(":"), this);

    m_silenceLength->setRange(0, 10000);
    m_silenceLength->setSingleStep(500);
    m_silenceLength->setSuffix(tr(" ms"));

    int row{0};
    lengthLayout->addWidget(loopLabel, row, 0);
    lengthLayout->addWidget(m_loopCount, row, 1);
    lengthLayout->addWidget(loopHintLabel, row++, 2);
    lengthLayout->addWidget(fadeLabel, row, 0);
    lengthLayout->addWidget(m_fadeLength, row++, 1);
    lengthLayout->addWidget(silenceLabel, row, 0);
    lengthLayout->addWidget(m_silenceLength, row++, 1);
    lengthLayout->setColumnStretch(3, 1);
    lengthLayout->setRowStretch(row++, 1);

    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    auto* romPathLabel = new QLabel(tr("ROM location") + QStringLiteral(":"), this);
    auto* romHintLabel = new QLabel(QStringLiteral("🛈 ")
                                        + tr("Certain files %1 require their system's ROM to play %2. "
                                             "Provide a directory where these can be found here.")
                                              .arg(QStringLiteral("(OPL-4 VGM)"), QStringLiteral("(yrw801.rom)")),
                                    this);
    romHintLabel->setWordWrap(true);

    auto* browseButton = new QPushButton(tr("&Browse…"), this);
    QObject::connect(browseButton, &QPushButton::pressed, this, &VgmInputSettings::getRomPath);

    m_romLocation->setMinimumWidth(200);

    row = 0;
    generalLayout->addWidget(m_guessTrack, row++, 0, 1, 3);
    generalLayout->addWidget(romPathLabel, row, 0);
    generalLayout->addWidget(m_romLocation, row, 1);
    generalLayout->addWidget(browseButton, row++, 2);
    generalLayout->addWidget(romHintLabel, row++, 0, 1, 3);
    generalLayout->setColumnStretch(1, 1);
    generalLayout->setRowStretch(row++, 1);

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    row = 0;
    layout->addWidget(lengthGroup, row++, 0, 1, 4);
    layout->addWidget(generalGroup, row++, 0, 1, 4);
    layout->addWidget(buttons, row++, 0, 1, 4, Qt::AlignBottom);
    layout->setColumnStretch(2, 1);

    m_loopCount->setValue(m_settings.value(LoopCountSetting, DefaultLoopCount).toInt());
    m_fadeLength->setValue(m_settings.value(FadeLengthSetting, DefaultFadeLength).toInt());
    m_silenceLength->setValue(m_settings.value(SilenceLengthSetting, DefaultSilenceLength).toInt());
    m_guessTrack->setChecked(m_settings.value(GuessTrackSetting, DefaultGuessTrack).toBool());
    m_romLocation->setText(m_settings.value(RomPathSetting).toString());
}

void VgmInputSettings::accept()
{
    m_settings.setValue(LoopCountSetting, m_loopCount->value());
    m_settings.setValue(FadeLengthSetting, m_fadeLength->value());
    m_settings.setValue(SilenceLengthSetting, m_silenceLength->value());
    m_settings.setValue(GuessTrackSetting, m_guessTrack->isChecked());
    m_settings.setValue(RomPathSetting, m_romLocation->text());

    done(Accepted);
}

void VgmInputSettings::getRomPath()
{
    const QString romPath = QFileDialog::getExistingDirectory(this, tr("Select ROM path"), QDir::homePath());
    if(romPath.isEmpty()) {
        return;
    }

    m_romLocation->setText(romPath);
}
} // namespace Fooyin::VgmInput
