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

#include "wavebarsettingspage.h"

#include "settings/wavebarsettings.h"
#include "wavebarconstants.h"

#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>

namespace Fooyin::WaveBar {
class WaveBarSettingsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit WaveBarSettingsPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_minMax;
    QCheckBox* m_rms;

    QRadioButton* m_downmixOff;
    QRadioButton* m_downmixStereo;
    QRadioButton* m_downmixMono;

    QCheckBox* m_showCursor;
    QDoubleSpinBox* m_channelScale;
    QSpinBox* m_cursorWidth;
    QSpinBox* m_barWidth;
    QSpinBox* m_barGap;
    QDoubleSpinBox* m_maxScale;
    QSpinBox* m_centreGap;
};

WaveBarSettingsPageWidget::WaveBarSettingsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_minMax{new QCheckBox(tr("MinMax"), this)}
    , m_rms{new QCheckBox(tr("RMS"), this)}
    , m_downmixOff{new QRadioButton(tr("Off"), this)}
    , m_downmixStereo{new QRadioButton(tr("Stereo"), this)}
    , m_downmixMono{new QRadioButton(tr("Mono"), this)}
    , m_showCursor{new QCheckBox(tr("Show Progress Cursor"), this)}
    , m_channelScale{new QDoubleSpinBox(this)}
    , m_cursorWidth{new QSpinBox(this)}
    , m_barWidth{new QSpinBox(this)}
    , m_barGap{new QSpinBox(this)}
    , m_maxScale{new QDoubleSpinBox(this)}
    , m_centreGap{new QSpinBox(this)}
{
    auto* layout = new QGridLayout(this);

    auto* cursorWidthLabel = new QLabel(tr("Cursor Width") + QStringLiteral(":"), this);
    auto* barWidthLabel    = new QLabel(tr("Bar Width") + QStringLiteral(":"), this);
    auto* barGapLabel      = new QLabel(tr("Bar Gap") + QStringLiteral(":"), this);
    auto* maxScaleLabel    = new QLabel(tr("Max Scale") + QStringLiteral(":"), this);
    auto* centreGapLabel   = new QLabel(tr("Centre Gap") + QStringLiteral(":"), this);

    m_cursorWidth->setMinimum(1);
    m_cursorWidth->setMaximum(20);
    m_cursorWidth->setSuffix(QStringLiteral(" px"));

    m_channelScale->setMinimum(0.0);
    m_channelScale->setMaximum(1.0);
    m_channelScale->setSingleStep(0.1);
    m_channelScale->setPrefix(QStringLiteral("x"));

    m_barWidth->setMinimum(1);
    m_barWidth->setMaximum(50);
    m_barWidth->setSingleStep(1);
    m_barWidth->setSuffix(QStringLiteral(" px"));

    m_barGap->setMinimum(0);
    m_barGap->setMaximum(50);
    m_barGap->setSingleStep(1);
    m_barGap->setSuffix(QStringLiteral(" px"));

    m_centreGap->setMinimum(0);
    m_centreGap->setMaximum(10);
    m_centreGap->setSuffix(QStringLiteral(" px"));

    m_maxScale->setMinimum(0.0);
    m_maxScale->setMaximum(2.0);
    m_maxScale->setSingleStep(0.25);
    m_maxScale->setPrefix(QStringLiteral("x"));

    auto* modeGroup  = new QGroupBox(tr("Mode"), this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    modeLayout->addWidget(m_minMax);
    modeLayout->addWidget(m_rms);

    auto* channelScaleLabel = new QLabel(tr("Channel Scale") + QStringLiteral(":"), this);

    auto* downmixGroupBox = new QGroupBox(tr("Downmix"), this);
    auto* downmixGroup    = new QButtonGroup(this);
    auto* downmixLayout   = new QVBoxLayout(downmixGroupBox);

    downmixGroup->addButton(m_downmixOff);
    downmixGroup->addButton(m_downmixStereo);
    downmixGroup->addButton(m_downmixMono);

    downmixLayout->addWidget(m_downmixOff);
    downmixLayout->addWidget(m_downmixStereo);
    downmixLayout->addWidget(m_downmixMono);

    auto* cursorGroup       = new QGroupBox(tr("Cursor"), this);
    auto* cursorGroupLayout = new QGridLayout(cursorGroup);

    cursorGroupLayout->addWidget(m_showCursor, 0, 0, 1, 2);
    cursorGroupLayout->addWidget(cursorWidthLabel, 1, 0);
    cursorGroupLayout->addWidget(m_cursorWidth, 1, 1);

    auto* scaleGroup       = new QGroupBox(tr("Scale"), this);
    auto* scaleGroupLayout = new QGridLayout(scaleGroup);

    int row{0};
    scaleGroupLayout->addWidget(channelScaleLabel, row, 0);
    scaleGroupLayout->addWidget(m_channelScale, row++, 1);
    scaleGroupLayout->addWidget(maxScaleLabel, row, 0);
    scaleGroupLayout->addWidget(m_maxScale, row++, 1);

    auto* dimensionGroup       = new QGroupBox(tr("Dimension"), this);
    auto* dimensionGroupLayout = new QGridLayout(dimensionGroup);

    row = 0;
    dimensionGroupLayout->addWidget(barWidthLabel, row, 0);
    dimensionGroupLayout->addWidget(m_barWidth, row++, 1);
    dimensionGroupLayout->addWidget(barGapLabel, row, 0);
    dimensionGroupLayout->addWidget(m_barGap, row++, 1);
    dimensionGroupLayout->addWidget(centreGapLabel, row, 0);
    dimensionGroupLayout->addWidget(m_centreGap, row++, 1);

    row = 0;
    layout->addWidget(modeGroup, row, 0);
    layout->addWidget(downmixGroupBox, row++, 1);
    layout->addWidget(dimensionGroup, row, 0);
    layout->addWidget(scaleGroup, row++, 1);
    layout->addWidget(cursorGroup, row, 0);

    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(3, 1);
}

void WaveBarSettingsPageWidget::load()
{
    m_showCursor->setChecked(m_settings->value<Settings::WaveBar::ShowCursor>());
    m_cursorWidth->setValue(m_settings->value<Settings::WaveBar::CursorWidth>());
    m_barWidth->setValue(m_settings->value<Settings::WaveBar::BarWidth>());
    m_barGap->setValue(m_settings->value<Settings::WaveBar::BarGap>());
    m_maxScale->setValue(m_settings->value<Settings::WaveBar::MaxScale>());
    m_centreGap->setValue(m_settings->value<Settings::WaveBar::CentreGap>());
    m_channelScale->setValue(m_settings->value<Settings::WaveBar::ChannelScale>());

    const auto mode = static_cast<WaveModes>(m_settings->value<Settings::WaveBar::Mode>());
    m_minMax->setChecked(mode & WaveMode::MinMax);
    m_rms->setChecked(mode & WaveMode::Rms);

    const auto downMixOption = static_cast<DownmixOption>(m_settings->value<Settings::WaveBar::Downmix>());
    if(downMixOption == DownmixOption::Off) {
        m_downmixOff->setChecked(true);
    }
    else if(downMixOption == DownmixOption::Stereo) {
        m_downmixStereo->setChecked(true);
    }
    else {
        m_downmixMono->setChecked(true);
    }
}

void WaveBarSettingsPageWidget::apply()
{
    m_settings->set<Settings::WaveBar::ShowCursor>(m_showCursor->isChecked());
    m_settings->set<Settings::WaveBar::CursorWidth>(m_cursorWidth->value());
    m_settings->set<Settings::WaveBar::BarWidth>(m_barWidth->value());
    m_settings->set<Settings::WaveBar::BarGap>(m_barGap->value());
    m_settings->set<Settings::WaveBar::MaxScale>(m_maxScale->value());
    m_settings->set<Settings::WaveBar::CentreGap>(m_centreGap->value());
    m_settings->set<Settings::WaveBar::ChannelScale>(m_channelScale->value());

    DownmixOption downMixOption;
    if(m_downmixOff->isChecked()) {
        downMixOption = DownmixOption::Off;
    }
    else if(m_downmixStereo->isChecked()) {
        downMixOption = DownmixOption::Stereo;
    }
    else {
        downMixOption = DownmixOption::Mono;
    }
    m_settings->set<Settings::WaveBar::Downmix>(static_cast<int>(downMixOption));

    WaveModes mode;
    if(m_minMax->isChecked()) {
        mode |= WaveMode::MinMax;
    }
    if(m_rms->isChecked()) {
        mode |= WaveMode::Rms;
    }
    m_settings->set<Settings::WaveBar::Mode>(static_cast<int>(mode));
}

void WaveBarSettingsPageWidget::reset()
{
    m_settings->reset<Settings::WaveBar::ShowCursor>();
    m_settings->reset<Settings::WaveBar::CursorWidth>();
    m_settings->reset<Settings::WaveBar::BarWidth>();
    m_settings->reset<Settings::WaveBar::BarGap>();
    m_settings->reset<Settings::WaveBar::MaxScale>();
    m_settings->reset<Settings::WaveBar::CentreGap>();
    m_settings->reset<Settings::WaveBar::Downmix>();
    m_settings->reset<Settings::WaveBar::ChannelScale>();
    m_settings->reset<Settings::WaveBar::Mode>();
}

WaveBarSettingsPage::WaveBarSettingsPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::WaveBarGeneral);
    setName(tr("General"));
    setCategory({tr("Plugins"), tr("WaveBar")});
    setWidgetCreator([settings] { return new WaveBarSettingsPageWidget(settings); });
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarsettingspage.cpp"
#include "wavebarsettingspage.moc"
