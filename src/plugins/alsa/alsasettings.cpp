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

#include "alsasettings.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>

namespace Fooyin {
AlsaSettings::AlsaSettings(QWidget* parent)
    : QDialog{parent}
    , m_bufferLength{new QSpinBox(this)}
    , m_periodLength{new QSpinBox(this)}
{
    setWindowTitle(tr("%1 Settings").arg(QStringLiteral("ALSA")));
    setModal(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &AlsaSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &AlsaSettings::reject);

    auto* bufferLabel = new QLabel(tr("Buffer length") + u":", this);
    auto* periodLabel = new QLabel(tr("Period length") + u":", this);

    m_bufferLength->setRange(200, 10000);
    m_bufferLength->setSuffix(QStringLiteral(" ms"));
    m_periodLength->setRange(20, 5000);
    m_periodLength->setSuffix(QStringLiteral(" ms"));

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    int row{0};
    layout->addWidget(bufferLabel, row, 0);
    layout->addWidget(m_bufferLength, row++, 1);
    layout->addWidget(periodLabel, row, 0);
    layout->addWidget(m_periodLength, row++, 1);
    layout->addWidget(buttons, row++, 0, 1, 2, Qt::AlignBottom);

    m_bufferLength->setValue(m_settings.value(QLatin1String{BufferLengthSetting}, DefaultBufferLength).toInt());
    m_periodLength->setValue(m_settings.value(QLatin1String{PeriodLengthSetting}, DefaultPeriodLength).toInt());
}

void AlsaSettings::accept()
{
    m_settings.setValue(QLatin1String{BufferLengthSetting}, m_bufferLength->value());
    m_settings.setValue(QLatin1String{PeriodLengthSetting}, m_periodLength->value());

    done(Accepted);
}
} // namespace Fooyin
