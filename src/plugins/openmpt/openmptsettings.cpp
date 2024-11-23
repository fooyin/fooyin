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

#include "openmptsettings.h"

#include "openmptdefs.h"

#include <gui/widgets/doubleslidereditor.h>
#include <gui/widgets/slidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace Fooyin::OpenMpt {
OpenMptSettings::OpenMptSettings(SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_settings{settings}
    , m_gain{new DoubleSliderEditor(tr("Gain"), this)}
    , m_separation{new SliderEditor(tr("Separation"), this)}
    , m_volRamping{new SliderEditor(tr("Volume ramping"), this)}
    , m_amigaResampler{new QCheckBox(tr("Use Amiga resampler"), this)}
    , m_interpolationFilter{new QComboBox(this)}
{
    setWindowTitle(tr("%1 Settings").arg(u"OpenMPT"_s));
    setModal(true);

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &OpenMptSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &OpenMptSettings::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, this,
                     &OpenMptSettings::reset);

    m_gain->setRange(-12, 12);
    m_gain->setSuffix(u" dB"_s);

    m_separation->setRange(0, 200);
    m_separation->setSuffix(u"%"_s);

    m_volRamping->setRange(-1, 10);
    m_volRamping->setSuffix(u" ms"_s);
    m_volRamping->addSpecialValue(-1, tr("Default"));
    m_volRamping->addSpecialValue(0, tr("Off"));

    auto* filterLabel = new QLabel(tr("Interpolation") + ":"_L1, this);

    m_interpolationFilter->addItem(tr("Default"), 0);
    m_interpolationFilter->addItem(tr("None"), 1);
    m_interpolationFilter->addItem(tr("Linear"), 2);
    m_interpolationFilter->addItem(tr("Cubic"), 4);
    m_interpolationFilter->addItem(tr("Sinc"), 8);

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    int row{0};
    layout->addWidget(m_gain, row++, 0, 1, 4);
    layout->addWidget(m_separation, row++, 0, 1, 4);
    layout->addWidget(m_volRamping, row++, 0, 1, 4);
    layout->addWidget(filterLabel, row, 0);
    layout->addWidget(m_interpolationFilter, row++, 1, 1, 4);
    layout->addWidget(m_amigaResampler, row++, 0, 1, 4);
    layout->addWidget(buttons, row++, 0, 1, 4, Qt::AlignBottom);
    layout->setColumnStretch(2, 1);

    load();
}

void OpenMptSettings::accept()
{
    m_settings->set<Settings::OpenMpt::Gain>(m_gain->value());
    m_settings->set<Settings::OpenMpt::Separation>(m_separation->value());
    m_settings->set<Settings::OpenMpt::VolumeRamping>(m_volRamping->value());
    m_settings->set<Settings::OpenMpt::InterpolationFilter>(m_interpolationFilter->currentData().toInt());
    m_settings->set<Settings::OpenMpt::EmulateAmiga>(m_amigaResampler->isChecked());

    done(Accepted);
}

void OpenMptSettings::load()
{
    m_gain->setValue(m_settings->value<Settings::OpenMpt::Gain>());
    m_separation->setValue(m_settings->value<Settings::OpenMpt::Separation>());
    m_volRamping->setValue(m_settings->value<Settings::OpenMpt::VolumeRamping>());
    m_interpolationFilter->setCurrentIndex(
        m_interpolationFilter->findData(m_settings->value<Settings::OpenMpt::InterpolationFilter>()));
    m_amigaResampler->setChecked(m_settings->value<Settings::OpenMpt::EmulateAmiga>());
}

void OpenMptSettings::reset()
{
    m_settings->reset<Settings::OpenMpt::Gain>();
    m_settings->reset<Settings::OpenMpt::Separation>();
    m_settings->reset<Settings::OpenMpt::VolumeRamping>();
    m_settings->reset<Settings::OpenMpt::InterpolationFilter>();
    m_settings->reset<Settings::OpenMpt::EmulateAmiga>();

    load();
}
} // namespace Fooyin::OpenMpt
