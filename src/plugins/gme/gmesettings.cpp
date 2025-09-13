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
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include <gme/gme.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Gme {
GmeSettings::GmeSettings(QWidget* parent)
    : QDialog{parent}
    , m_maxLength{new QDoubleSpinBox(this)}
    , m_loopCount{new QSpinBox(this)}
    , m_fadeLength{new QSpinBox(this)}
{
    setWindowTitle(tr("%1 Settings").arg(u"GME"_s));
    setModal(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &GmeSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &GmeSettings::reject);

    auto* lengthGroup  = new QGroupBox(tr("Length"), this);
    auto* lengthLayout = new QGridLayout(lengthGroup);

    auto* maxLengthLabel = new QLabel(tr("Maximum length") + u":"_s, this);

    m_maxLength->setRange(1.0, 60.0);
    m_maxLength->setSingleStep(0.5);
    m_maxLength->setSuffix(u" "_s + tr("minutes"));

    auto* loopLabel = new QLabel(tr("Loop count") + u":"_s, this);

    m_loopCount->setRange(1, 16);
    m_loopCount->setSingleStep(1);
    m_loopCount->setSuffix(u" "_s + tr("times"));

    auto* fadeLabel = new QLabel(tr("Fade length") + u":"_s, this);

    m_fadeLength->setRange(0, 10000);
    m_fadeLength->setSingleStep(500);
    m_fadeLength->setSuffix(u" "_s + tr("ms"));

    int row{0};
    lengthLayout->addWidget(maxLengthLabel, row, 0);
    lengthLayout->addWidget(m_maxLength, row++, 1);
    lengthLayout->addWidget(loopLabel, row, 0);
    lengthLayout->addWidget(m_loopCount, row++, 1);
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

    m_maxLength->setValue(m_settings.value(MaxLength, DefaultMaxLength).toInt());
    m_loopCount->setValue(m_settings.value(LoopCount, DefaultLoopCount).toInt());
    m_fadeLength->setValue(m_settings.value(FadeLength, DefaultFadeLength).toInt());

#if defined(GME_VERSION) && GME_VERSION < 0x000604
    fadeLabel->setVisible(false);
    m_fadeLength->setVisible(false);
#endif
}

void GmeSettings::accept()
{
    m_settings.setValue(MaxLength, m_maxLength->value());
    m_settings.setValue(LoopCount, m_loopCount->value());
    m_settings.setValue(FadeLength, m_fadeLength->value());

    done(Accepted);
}
} // namespace Fooyin::Gme
