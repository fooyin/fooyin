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

#include "gmesettings.h"

#include "gmedefs.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

namespace Fooyin::Gme {
GmeSettings::GmeSettings(QWidget* parent)
    : QDialog{parent}
    , m_maxLength{new QDoubleSpinBox(this)}
    , m_loopCount{new QSpinBox(this)}
    , m_fadeLength{new QSpinBox(this)}
{
    setWindowTitle(tr("GME Settings"));
    setModal(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &GmeSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &GmeSettings::reject);

    auto* lengthGroup  = new QGroupBox(tr("Length"), this);
    auto* lengthLayout = new QGridLayout(lengthGroup);

    auto* maxLengthLabel = new QLabel(tr("Maximum length") + QStringLiteral(":"), this);

    m_maxLength->setRange(1.0, 60.0);
    m_maxLength->setSingleStep(0.5);
    m_maxLength->setSuffix(tr(" minutes"));

    auto* loopLabel     = new QLabel(tr("Loop count") + QStringLiteral(":"), this);
    auto* loopHintLabel = new QLabel(tr("(0 = infinite)"), this);

    m_loopCount->setRange(0, 16);
    m_loopCount->setSingleStep(1);
    m_loopCount->setSuffix(tr(" times"));

    auto* fadeLabel = new QLabel(tr("Fade length") + QStringLiteral(":"), this);

    m_fadeLength->setRange(0, 10000);
    m_fadeLength->setSingleStep(500);
    m_fadeLength->setSuffix(tr(" ms"));

    int row{0};
    lengthLayout->addWidget(maxLengthLabel, row, 0);
    lengthLayout->addWidget(m_maxLength, row++, 1);
    lengthLayout->addWidget(loopLabel, row, 0);
    lengthLayout->addWidget(m_loopCount, row, 1);
    lengthLayout->addWidget(loopHintLabel, row++, 2);
    lengthLayout->addWidget(fadeLabel, row, 0);
    lengthLayout->addWidget(m_fadeLength, row++, 1);
    lengthLayout->setColumnStretch(3, 1);
    lengthLayout->setRowStretch(row++, 1);

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    row = 0;
    layout->addWidget(lengthGroup, row++, 0, 1, 4);
    layout->addWidget(buttons, row++, 0, 1, 4, Qt::AlignBottom);
    layout->setColumnStretch(2, 1);

    m_maxLength->setValue(m_settings.value(QLatin1String{MaxLength}, DefaultMaxLength).toInt());
    m_loopCount->setValue(m_settings.value(QLatin1String{LoopCount}, DefaultLoopCount).toInt());
    m_fadeLength->setValue(m_settings.value(QLatin1String{FadeLength}, DefaultFadeLength).toInt());
}

void GmeSettings::accept()
{
    m_settings.setValue(QLatin1String{MaxLength}, m_maxLength->value());
    m_settings.setValue(QLatin1String{LoopCount}, m_loopCount->value());
    m_settings.setValue(QLatin1String{FadeLength}, m_fadeLength->value());

    done(Accepted);
}
} // namespace Fooyin::Gme
