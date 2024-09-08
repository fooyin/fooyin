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

#include "replaygainpage.h"

#include <core/coresettings.h>
#include <core/engine/enginecontroller.h>
#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Fooyin {
class ReplayGainWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ReplayGainWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_enabled;
    QRadioButton* m_trackGain;
    QRadioButton* m_albumGain;
    QSlider* m_preAmpSlider;
    QSpinBox* m_preAmpSpinBox;
};

ReplayGainWidget::ReplayGainWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_enabled{new QCheckBox(tr("Enable ReplayGain"), this)}
    , m_trackGain{new QRadioButton(tr("Use track-based gain"), this)}
    , m_albumGain{new QRadioButton(tr("Use album-based gain"), this)}
    , m_preAmpSlider{new QSlider(Qt::Horizontal, this)}
    , m_preAmpSpinBox{new QSpinBox(this)}
{
    m_enabled->setToolTip(tr("Normalize loudness for tracks or albums"));
    m_trackGain->setToolTip(tr("Base normalization on track loudness"));
    m_albumGain->setToolTip(tr("Base normalization on album loudness"));

    auto* layout = new QGridLayout(this);

    auto* typeGroupBox    = new QGroupBox(tr("Type"), this);
    auto* typeButtonGroup = new QButtonGroup(this);
    auto* typeBoxLayout   = new QVBoxLayout(typeGroupBox);

    typeButtonGroup->addButton(m_trackGain);
    typeButtonGroup->addButton(m_albumGain);

    typeBoxLayout->addWidget(m_trackGain);
    typeBoxLayout->addWidget(m_albumGain);

    auto* preAmpLabel = new QLabel(tr("Pre-amplification") + QStringLiteral(":"), this);

    m_preAmpSlider->setRange(-20, 20);
    m_preAmpSlider->setSingleStep(1);

    m_preAmpSpinBox->setRange(-20, 20);
    m_preAmpSpinBox->setSingleStep(1);
    m_preAmpSpinBox->setSuffix(QStringLiteral(" dB"));

    const auto preAmpToolTip = tr("Amount of gain to apply in combination with ReplayGain");
    m_preAmpSpinBox->setToolTip(preAmpToolTip);
    m_preAmpSlider->setToolTip(preAmpToolTip);
    preAmpLabel->setToolTip(preAmpToolTip);

    QObject::connect(m_preAmpSlider, &QSlider::valueChanged, this,
                     [this](int value) { m_preAmpSpinBox->setValue(value); });
    QObject::connect(m_preAmpSpinBox, &QSpinBox::valueChanged, this,
                     [this](int value) { m_preAmpSlider->setValue(value); });

    auto* preAmpLayout = new QGridLayout();
    preAmpLayout->addWidget(preAmpLabel, 0, 0);
    preAmpLayout->addWidget(m_preAmpSlider, 0, 1);
    preAmpLayout->addWidget(m_preAmpSpinBox, 0, 2);
    preAmpLayout->setColumnStretch(1, 1);

    layout->addWidget(m_enabled, 0, 0);
    layout->addWidget(typeGroupBox, 1, 0);
    layout->addLayout(preAmpLayout, 2, 0);

    layout->setRowStretch(layout->rowCount(), 1);
}

void ReplayGainWidget::load()
{
    m_enabled->setChecked(m_settings->value<Settings::Core::ReplayGainEnabled>());
    const auto gainType = static_cast<ReplayGainType>(m_settings->value<Settings::Core::ReplayGainType>());
    if(gainType == ReplayGainType::Track) {
        m_trackGain->setChecked(true);
    }
    else {
        m_albumGain->setChecked(true);
    }
    const auto preAmp = m_settings->value<Settings::Core::ReplayGainPreAmp>();
    m_preAmpSlider->setValue(preAmp);
    m_preAmpSpinBox->setValue(preAmp);
}

void ReplayGainWidget::apply()
{
    m_settings->set<Settings::Core::ReplayGainEnabled>(m_enabled->isChecked());
    m_settings->set<Settings::Core::ReplayGainType>(
        static_cast<int>(m_trackGain->isChecked() ? ReplayGainType::Track : ReplayGainType::Album));
    m_settings->set<Settings::Core::ReplayGainPreAmp>(m_preAmpSpinBox->value());
}

void ReplayGainWidget::reset()
{
    m_settings->reset<Settings::Core::ReplayGainEnabled>();
    m_settings->reset<Settings::Core::ReplayGainPreAmp>();
}

ReplayGainPage::ReplayGainPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::ReplayGain);
    setName(tr("General"));
    setCategory({tr("Playback"), tr("ReplayGain")});
    setWidgetCreator([settings] { return new ReplayGainWidget(settings); });
}
} // namespace Fooyin

#include "moc_replaygainpage.cpp"
#include "replaygainpage.moc"
