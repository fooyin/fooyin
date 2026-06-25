/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
 * Copyright © 2026, Gustav Oechler <gustavoechler@gmail.com>
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

#include "sleepinhibitorsettings.h"

#include <utils/settings/settingsmanager.h>

#include <QDialogButtonBox>
#include <QGridLayout>

using namespace Qt::StringLiterals;

namespace Fooyin {
SleepInhibitorSettings::SleepInhibitorSettings(QWidget* parent)
    : QDialog{parent}
    , m_enabled{new QCheckBox(tr("Inhibit system sleep"), this)}
    , m_onlyDuringPlayback{new QCheckBox(tr("Inhibit system sleep during playback only"), this)}
    , m_preventDisplaySleep{new QCheckBox(tr("Prevent display from turning off"), this)}
{
    namespace Settings = SleepInhibitor::Settings;

    setWindowTitle(tr("Sleep Inhibitor Settings"));
    setModal(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &SleepInhibitorSettings::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &SleepInhibitorSettings::reject);

    auto* layout = new QGridLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    int row{0};
    layout->addWidget(m_enabled, row++, 0);
    layout->addWidget(m_onlyDuringPlayback, row++, 0);
    layout->addWidget(m_preventDisplaySleep, row++, 0);
    layout->addWidget(buttons, row++, 0, 1, 2, Qt::AlignBottom);

    const auto enabled = m_settings.value(Settings::Enabled, true).toBool();
    m_enabled->setChecked(enabled);
    m_onlyDuringPlayback->setChecked(m_settings.value(Settings::OnlyDuringPlayback, true).toBool());
    m_onlyDuringPlayback->setEnabled(enabled);
    m_preventDisplaySleep->setChecked(m_settings.value(Settings::PreventDisplaySleep, false).toBool());
    m_preventDisplaySleep->setEnabled(enabled);

    QObject::connect(m_enabled, &QCheckBox::clicked, m_onlyDuringPlayback, &QCheckBox::setEnabled);
    QObject::connect(m_enabled, &QCheckBox::clicked, m_preventDisplaySleep, &QCheckBox::setEnabled);
}

void SleepInhibitorSettings::accept()
{
    namespace Settings = SleepInhibitor::Settings;

    m_settings.setValue(Settings::Enabled, m_enabled->isChecked());
    m_settings.setValue(Settings::OnlyDuringPlayback, m_onlyDuringPlayback->isChecked());
    m_settings.setValue(Settings::PreventDisplaySleep, m_preventDisplaySleep->isChecked());

    Q_EMIT settingsChanged();
    done(Accepted);
}
} // namespace Fooyin
