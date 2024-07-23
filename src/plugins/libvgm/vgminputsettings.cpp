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
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

namespace Fooyin::VgmInput {
VgmInputSettings::VgmInputSettings(QWidget* parent)
    : QDialog{parent}
    , m_loopCount{new QSpinBox(this)}
    , m_guessTrack{new QCheckBox(tr("Guess track number from filename"), this)}
{
    setWindowTitle(tr("VGM Input Settings"));
    setModal(true);

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &VgmInputSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &VgmInputSettings::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this,
                     &VgmInputSettings::apply);

    auto* lengthGroup  = new QGroupBox(tr("Length"), this);
    auto* lengthLayout = new QGridLayout(lengthGroup);

    auto* loopLabel     = new QLabel(tr("Loop count") + QStringLiteral(":"), this);
    auto* loopHintLabel = new QLabel(tr("(0 = infinite)"), this);

    m_loopCount->setSuffix(tr(" times"));

    int row{0};
    lengthLayout->addWidget(loopLabel, row, 0);
    lengthLayout->addWidget(m_loopCount, row, 1);
    lengthLayout->addWidget(loopHintLabel, row++, 2);
    lengthLayout->setRowStretch(row++, 1);

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    row = 0;
    layout->addWidget(lengthGroup, row++, 0, 1, 3);
    layout->addWidget(m_guessTrack, row++, 0, 1, 3);
    layout->addWidget(buttons, row++, 0, 1, 3, Qt::AlignBottom);
    layout->setColumnStretch(2, 1);

    m_loopCount->setValue(m_settings.value(QLatin1String{LoopCountSetting}, DefaultLoopCount).toInt());
    m_guessTrack->setChecked(m_settings.value(QLatin1String{GuessTrackSetting}, DefaultGuessTrack).toBool());
}

void VgmInputSettings::accept()
{
    apply();
    done(Accepted);
}

void VgmInputSettings::apply()
{
    m_settings.setValue(QLatin1String{LoopCountSetting}, m_loopCount->value());
    m_settings.setValue(QLatin1String{GuessTrackSetting}, m_guessTrack->isChecked());
}
} // namespace Fooyin::VgmInput
