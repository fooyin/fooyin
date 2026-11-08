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

#include "mprispage.h"

#include "mprissettings.h"

#include <utils/audioutils.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

using namespace Qt::StringLiterals;

namespace Fooyin::Mpris {
namespace {
class MprisPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit MprisPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;
    QCheckBox* m_enable;
    QWidget* m_volumeControls;
    QComboBox* m_volumeCurve;
};

MprisPageWidget::MprisPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_enable{new QCheckBox(tr("Enable MPRIS"), this)}
    , m_volumeControls{new QWidget(this)}
    , m_volumeCurve{new QComboBox(this)}
{
    using Audio::VolumeCurve;

    m_volumeCurve->addItem(tr("Logarithmic dB"), static_cast<int>(VolumeCurve::LogarithmicDb));
    m_volumeCurve->addItem(tr("Linear dB"), static_cast<int>(VolumeCurve::LinearDb));
    m_volumeCurve->addItem(tr("Linear amplitude"), static_cast<int>(VolumeCurve::LinearAmplitude));
    m_volumeCurve->addItem(tr("Quadratic"), static_cast<int>(VolumeCurve::Quadratic));
    m_volumeCurve->addItem(tr("Cubic"), static_cast<int>(VolumeCurve::Cubic));

    auto* description = new QLabel(tr("Controls how MPRIS volume values map to the audio level. "
                                      "This applies to all MPRIS clients and combines with any curve "
                                      "the client already uses."),
                                   this);
    description->setWordWrap(true);

    auto* volumeLayout = new QGridLayout(m_volumeControls);
    volumeLayout->addWidget(new QLabel(tr("Volume curve") + ":"_L1, this), 0, 0);
    volumeLayout->addWidget(m_volumeCurve, 0, 1);
    volumeLayout->addWidget(description, 1, 0, 1, 2);
    volumeLayout->setColumnStretch(1, 1);

    auto* group       = new QGroupBox(tr("MPRIS"), this);
    auto* groupLayout = new QGridLayout(group);
    groupLayout->addWidget(m_enable, 0, 0);
    groupLayout->addWidget(m_volumeControls, 1, 0);

    auto* layout = new QGridLayout(this);
    layout->addWidget(group, 0, 0);
    layout->setRowStretch(1, 1);

    QObject::connect(m_enable, &QCheckBox::toggled, m_volumeControls, &QWidget::setEnabled);
}

void MprisPageWidget::load()
{
    m_enable->setChecked(m_settings->value<Settings::Mpris::Enabled>());
    m_volumeControls->setEnabled(m_enable->isChecked());

    const int curve = m_settings->value<Settings::Mpris::VolumeCurve>();
    const int index = m_volumeCurve->findData(curve);
    m_volumeCurve->setCurrentIndex(
        index >= 0 ? index : m_volumeCurve->findData(static_cast<int>(Audio::VolumeCurve::LinearAmplitude)));
}

void MprisPageWidget::apply()
{
    m_settings->set<Settings::Mpris::VolumeCurve>(m_volumeCurve->currentData().toInt());
    m_settings->set<Settings::Mpris::Enabled>(m_enable->isChecked());
}

void MprisPageWidget::reset()
{
    m_settings->reset<Settings::Mpris::VolumeCurve>();
    m_settings->reset<Settings::Mpris::Enabled>();
}
} // namespace

MprisPage::MprisPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId("Fooyin.Page.Integrations.Mpris");
    setName(tr("General"));
    setCategory({tr("Integrations"), tr("MPRIS")});
    setWidgetCreator([settings] { return new MprisPageWidget(settings); });
}
} // namespace Fooyin::Mpris

#include "moc_mprispage.cpp"
#include "mprispage.moc"
