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
#include <gui/widgets/doubleslidereditor.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSlider>

namespace Fooyin {
class ReplayGainPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit ReplayGainPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QRadioButton* m_disabled;
    QRadioButton* m_applyGain;
    QRadioButton* m_applyGainClipping;
    QRadioButton* m_clipping;
    QRadioButton* m_trackGain;
    QRadioButton* m_albumGain;
    QRadioButton* m_orderGain;
    DoubleSliderEditor* m_rgPreAmp;
    DoubleSliderEditor* m_preAmp;
};

ReplayGainPageWidget::ReplayGainPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_disabled{new QRadioButton(tr("Disabled"), this)}
    , m_applyGain{new QRadioButton(tr("Apply gain"), this)}
    , m_applyGainClipping{new QRadioButton(tr("Apply gain and prevent clipping according to peak"), this)}
    , m_clipping{new QRadioButton(tr("Only prevent clipping according to peak"), this)}
    , m_trackGain{new QRadioButton(tr("Use track-based gain"), this)}
    , m_albumGain{new QRadioButton(tr("Use album-based gain"), this)}
    , m_orderGain{new QRadioButton(tr("Use gain based on playback order"), this)}
    , m_rgPreAmp{new DoubleSliderEditor(this)}
    , m_preAmp{new DoubleSliderEditor(this)}
{
    m_trackGain->setToolTip(tr("Base normalisation on track loudness"));
    m_albumGain->setToolTip(tr("Base normalisation on album loudness"));
    m_orderGain->setToolTip(tr("Base normalisation on track loudness if shuffling tracks, else album loudness"));

    auto* layout = new QGridLayout(this);

    auto* modeGroupBox    = new QGroupBox(tr("Mode"), this);
    auto* modeButtonGroup = new QButtonGroup(this);
    auto* modeBoxLayout   = new QGridLayout(modeGroupBox);

    modeButtonGroup->addButton(m_disabled);
    modeButtonGroup->addButton(m_applyGain);
    modeButtonGroup->addButton(m_applyGainClipping);
    modeButtonGroup->addButton(m_clipping);

    modeBoxLayout->addWidget(m_disabled);
    modeBoxLayout->addWidget(m_applyGain);
    modeBoxLayout->addWidget(m_applyGainClipping);
    modeBoxLayout->addWidget(m_clipping);

    auto* typeGroupBox    = new QGroupBox(tr("Type"), this);
    auto* typeButtonGroup = new QButtonGroup(this);
    auto* typeBoxLayout   = new QVBoxLayout(typeGroupBox);

    typeButtonGroup->addButton(m_trackGain);
    typeButtonGroup->addButton(m_albumGain);
    typeButtonGroup->addButton(m_orderGain);

    typeBoxLayout->addWidget(m_trackGain);
    typeBoxLayout->addWidget(m_albumGain);
    typeBoxLayout->addWidget(m_orderGain);

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

    layout->addWidget(modeGroupBox, 0, 0);
    layout->addWidget(typeGroupBox, 1, 0);
    layout->addWidget(preAmpGroup, 2, 0);

    layout->setRowStretch(layout->rowCount(), 1);
}

void ReplayGainPageWidget::load()
{
    const auto mode = static_cast<AudioEngine::RGProcessing>(m_settings->value<Settings::Core::RGMode>());
    m_disabled->setChecked(mode == AudioEngine::NoProcessing);
    m_applyGain->setChecked(mode == AudioEngine::ApplyGain);
    m_applyGainClipping->setChecked(
        mode == static_cast<AudioEngine::RGProcessing>(AudioEngine::ApplyGain | AudioEngine::PreventClipping));
    m_clipping->setChecked(mode == AudioEngine::PreventClipping);

    const auto gainType = static_cast<ReplayGainType>(m_settings->value<Settings::Core::RGType>());
    m_trackGain->setChecked(gainType == ReplayGainType::Track);
    m_albumGain->setChecked(gainType == ReplayGainType::Album);
    m_orderGain->setChecked(gainType == ReplayGainType::PlaybackOrder);

    const auto rgPreAmp = static_cast<double>(m_settings->value<Settings::Core::RGPreAmp>());
    m_rgPreAmp->setValue(rgPreAmp);
    const auto preAmp = static_cast<double>(m_settings->value<Settings::Core::NonRGPreAmp>());
    m_preAmp->setValue(preAmp);
}

void ReplayGainPageWidget::apply()
{
    int mode{AudioEngine::NoProcessing};

    if(m_disabled->isChecked()) {
        mode = AudioEngine::NoProcessing;
    }
    else if(m_applyGain->isChecked()) {
        mode = AudioEngine::ApplyGain;
    }
    else if(m_applyGainClipping->isChecked()) {
        mode = AudioEngine::ApplyGain | AudioEngine::PreventClipping;
    }
    else if(m_clipping->isChecked()) {
        mode = AudioEngine::PreventClipping;
    }

    m_settings->set<Settings::Core::RGMode>(mode);

    if(m_trackGain->isChecked()) {
        m_settings->set<Settings::Core::RGType>(static_cast<int>(ReplayGainType::Track));
    }
    else if(m_albumGain->isChecked()) {
        m_settings->set<Settings::Core::RGType>(static_cast<int>(ReplayGainType::Album));
    }
    else if(m_orderGain->isChecked()) {
        m_settings->set<Settings::Core::RGType>(static_cast<int>(ReplayGainType::PlaybackOrder));
    }

    m_settings->set<Settings::Core::RGPreAmp>(static_cast<float>(m_rgPreAmp->value()));
    m_settings->set<Settings::Core::NonRGPreAmp>(static_cast<float>(m_preAmp->value()));
}

void ReplayGainPageWidget::reset()
{
    m_settings->reset<Settings::Core::RGMode>();
    m_settings->reset<Settings::Core::RGType>();
    m_settings->reset<Settings::Core::RGPreAmp>();
    m_settings->reset<Settings::Core::NonRGPreAmp>();
}

ReplayGainPage::ReplayGainPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::ReplayGain);
    setName(tr("General"));
    setCategory({tr("Playback"), tr("ReplayGain")});
    setWidgetCreator([settings] { return new ReplayGainPageWidget(settings); });
}
} // namespace Fooyin

#include "moc_replaygainpage.cpp"
#include "replaygainpage.moc"
