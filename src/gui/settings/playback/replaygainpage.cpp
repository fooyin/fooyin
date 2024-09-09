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
#include <utils/widgets/doubleslidereditor.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>
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
    DoubleSliderEditor* m_rgPreAmp;
    DoubleSliderEditor* m_preAmp;
};

ReplayGainWidget::ReplayGainWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_enabled{new QCheckBox(tr("Enable ReplayGain"), this)}
    , m_trackGain{new QRadioButton(tr("Use track-based gain"), this)}
    , m_albumGain{new QRadioButton(tr("Use album-based gain"), this)}
    , m_rgPreAmp{new DoubleSliderEditor(this)}
    , m_preAmp{new DoubleSliderEditor(this)}
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

    auto* preAmpGroup  = new QGroupBox(tr("Pre-amplification"), this);
    auto* preAmpLayout = new QGridLayout(preAmpGroup);

    auto* rgPreAmpLabel = new QLabel(tr("With RG info") + u":", this);
    auto* preAmpLabel   = new QLabel(tr("Without RG info") + u":", this);

    m_rgPreAmp->setRange(-20, 20);
    m_rgPreAmp->setSingleStep(0.5);
    m_rgPreAmp->setSuffix(QStringLiteral(" dB"));

    m_preAmp->setRange(-20, 20);
    m_preAmp->setSingleStep(0.5);
    m_preAmp->setSuffix(QStringLiteral(" dB"));

    const auto rgPreAmpToolTip = tr("Amount of gain to apply in combination with ReplayGain");
    m_rgPreAmp->setToolTip(rgPreAmpToolTip);
    preAmpLabel->setToolTip(rgPreAmpToolTip);
    const auto preAmpToolTip = tr("Amount of gain to apply for tracks without ReplayGain info");
    m_preAmp->setToolTip(preAmpToolTip);
    preAmpLabel->setToolTip(preAmpToolTip);

    preAmpLayout->addWidget(rgPreAmpLabel, 0, 0);
    preAmpLayout->addWidget(m_rgPreAmp, 0, 1);
    preAmpLayout->addWidget(preAmpLabel, 1, 0);
    preAmpLayout->addWidget(m_preAmp, 1, 1);
    preAmpLayout->setColumnStretch(1, 1);

    layout->addWidget(m_enabled, 0, 0);
    layout->addWidget(typeGroupBox, 1, 0);
    layout->addWidget(preAmpGroup, 2, 0);

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

    const auto rgPreAmp = static_cast<double>(m_settings->value<Settings::Core::ReplayGainPreAmp>());
    m_rgPreAmp->setValue(rgPreAmp);
    const auto preAmp = static_cast<double>(m_settings->value<Settings::Core::NonRGPreAmp>());
    m_preAmp->setValue(preAmp);
}

void ReplayGainWidget::apply()
{
    m_settings->set<Settings::Core::ReplayGainEnabled>(m_enabled->isChecked());
    m_settings->set<Settings::Core::ReplayGainType>(
        static_cast<int>(m_trackGain->isChecked() ? ReplayGainType::Track : ReplayGainType::Album));
    m_settings->set<Settings::Core::ReplayGainPreAmp>(static_cast<float>(m_rgPreAmp->value()));
    m_settings->set<Settings::Core::NonRGPreAmp>(static_cast<float>(m_preAmp->value()));
}

void ReplayGainWidget::reset()
{
    m_settings->reset<Settings::Core::ReplayGainEnabled>();
    m_settings->reset<Settings::Core::ReplayGainPreAmp>();
    m_settings->reset<Settings::Core::NonRGPreAmp>();
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
