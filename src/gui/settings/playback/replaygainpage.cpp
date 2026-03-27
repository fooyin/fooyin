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
#include <core/track.h>
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

using namespace Qt::StringLiterals;

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
    QRadioButton* m_leaveHeaderNull;
    QRadioButton* m_useTrackHeader;
    QRadioButton* m_useAlbumHeader;
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
    , m_leaveHeaderNull{new QRadioButton(tr("Leave null"), this)}
    , m_useTrackHeader{new QRadioButton(tr("Use Track Gain"), this)}
    , m_useAlbumHeader{new QRadioButton(tr("Use Album Gain"), this)}
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

    auto* opusGroupBox    = new QGroupBox(tr("Opus Header Gain"), this);
    auto* opusButtonGroup = new QButtonGroup(this);
    auto* opusBoxLayout   = new QVBoxLayout(opusGroupBox);

    opusButtonGroup->addButton(m_leaveHeaderNull);
    opusButtonGroup->addButton(m_useTrackHeader);
    opusButtonGroup->addButton(m_useAlbumHeader);

    m_useTrackHeader->setToolTip(
        tr("Write track gain to the Opus header and keep album gain in the R128 comment field"));
    m_useAlbumHeader->setToolTip(
        tr("Write album gain to the Opus header and keep track gain in the R128 comment field"));
    m_leaveHeaderNull->setToolTip(
        tr("Do not write ReplayGain values to the Opus header unless changed explicitly elsewhere"));

    opusBoxLayout->addWidget(m_useTrackHeader);
    opusBoxLayout->addWidget(m_useAlbumHeader);
    opusBoxLayout->addWidget(m_leaveHeaderNull);

    auto* preAmpGroup  = new QGroupBox(tr("Pre-amplification"), this);
    auto* preAmpLayout = new QGridLayout(preAmpGroup);

    auto* rgPreAmpLabel = new QLabel(tr("With RG info") + ":"_L1, this);
    auto* preAmpLabel   = new QLabel(tr("Without RG info") + ":"_L1, this);

    m_rgPreAmp->setRange(-20, 20);
    m_rgPreAmp->setSingleStep(0.5);
    m_rgPreAmp->setSuffix(u" dB"_s);

    m_preAmp->setRange(-20, 20);
    m_preAmp->setSingleStep(0.5);
    m_preAmp->setSuffix(u" dB"_s);

    const auto rgPreAmpToolTip = tr("Amount of gain to apply in combination with ReplayGain");
    m_rgPreAmp->setToolTip(rgPreAmpToolTip);
    preAmpLabel->setToolTip(rgPreAmpToolTip);
    const auto preAmpToolTip = tr("Amount of gain to apply for tracks without ReplayGain info");
    m_preAmp->setToolTip(preAmpToolTip);
    preAmpLabel->setToolTip(preAmpToolTip);

    int row{0};
    preAmpLayout->addWidget(rgPreAmpLabel, row, 0);
    preAmpLayout->addWidget(m_rgPreAmp, row++, 1);
    preAmpLayout->addWidget(preAmpLabel, row, 0);
    preAmpLayout->addWidget(m_preAmp, row++, 1);
    preAmpLayout->setColumnStretch(1, 1);

    row = 0;
    layout->addWidget(modeGroupBox, row++, 0);
    layout->addWidget(typeGroupBox, row++, 0);
    layout->addWidget(preAmpGroup, row++, 0);
    layout->addWidget(opusGroupBox, row++, 0);

    layout->setRowStretch(layout->rowCount(), 1);
}

void ReplayGainPageWidget::load()
{
    const auto mode = static_cast<Engine::RGProcessing>(m_settings->value<Settings::Core::RGMode>());
    m_disabled->setChecked(mode == Engine::NoProcessing);
    m_applyGain->setChecked(mode == Engine::ApplyGain);
    m_applyGainClipping->setChecked(mode
                                    == static_cast<Engine::RGProcessing>(Engine::ApplyGain | Engine::PreventClipping));
    m_clipping->setChecked(mode == Engine::PreventClipping);

    const auto gainType = static_cast<ReplayGainType>(m_settings->value<Settings::Core::RGType>());
    m_trackGain->setChecked(gainType == ReplayGainType::Track);
    m_albumGain->setChecked(gainType == ReplayGainType::Album);
    m_orderGain->setChecked(gainType == ReplayGainType::PlaybackOrder);

    const auto opusMode
        = static_cast<OpusRGWriteMode>(m_settings->value<Settings::Core::Internal::OpusHeaderWriteMode>());
    m_useTrackHeader->setChecked(opusMode == OpusRGWriteMode::Track);
    m_useAlbumHeader->setChecked(opusMode == OpusRGWriteMode::Album);
    m_leaveHeaderNull->setChecked(opusMode == OpusRGWriteMode::LeaveNull);

    const auto rgPreAmp = static_cast<double>(m_settings->value<Settings::Core::RGPreAmp>());
    m_rgPreAmp->setValue(rgPreAmp);
    const auto preAmp = static_cast<double>(m_settings->value<Settings::Core::NonRGPreAmp>());
    m_preAmp->setValue(preAmp);
}

void ReplayGainPageWidget::apply()
{
    int mode{Engine::NoProcessing};

    if(m_disabled->isChecked()) {
        mode = Engine::NoProcessing;
    }
    else if(m_applyGain->isChecked()) {
        mode = Engine::ApplyGain;
    }
    else if(m_applyGainClipping->isChecked()) {
        mode = Engine::ApplyGain | Engine::PreventClipping;
    }
    else if(m_clipping->isChecked()) {
        mode = Engine::PreventClipping;
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

    const auto opusMode = m_useTrackHeader->isChecked() ? OpusRGWriteMode::Track
                        : m_useAlbumHeader->isChecked() ? OpusRGWriteMode::Album
                                                        : OpusRGWriteMode::LeaveNull;
    m_settings->set<Settings::Core::Internal::OpusHeaderWriteMode>(static_cast<int>(opusMode));

    m_settings->set<Settings::Core::RGPreAmp>(static_cast<float>(m_rgPreAmp->value()));
    m_settings->set<Settings::Core::NonRGPreAmp>(static_cast<float>(m_preAmp->value()));
}

void ReplayGainPageWidget::reset()
{
    m_settings->reset<Settings::Core::RGMode>();
    m_settings->reset<Settings::Core::RGType>();
    m_settings->reset<Settings::Core::Internal::OpusHeaderWriteMode>();
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
