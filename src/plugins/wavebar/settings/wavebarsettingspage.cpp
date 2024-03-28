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
#include "wavebarcolours.h"
#include "wavebarconstants.h"

#include <utils/settings/settingsmanager.h>
#include <utils/widgets/colourbutton.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

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

    QCheckBox* m_showCursor;
    QDoubleSpinBox* m_cursorWidth;
    QDoubleSpinBox* m_channelHeightScale;

    QComboBox* m_drawValues;
    QSpinBox* m_sampleValues;
    QComboBox* m_downmix;
    QCheckBox* m_antialiasing;

    QGroupBox* m_colourGroup;
    ColourButton* m_bgUnplayed;
    ColourButton* m_bgPlayed;
    ColourButton* m_fgUnplayed;
    ColourButton* m_fgPlayed;
    ColourButton* m_rmsUnplayed;
    ColourButton* m_rmsPlayed;
    ColourButton* m_cursorColour;
    ColourButton* m_seekingCursorColour;
};

WaveBarSettingsPageWidget::WaveBarSettingsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_showCursor{new QCheckBox(tr("Show Cursor"), this)}
    , m_cursorWidth{new QDoubleSpinBox(this)}
    , m_channelHeightScale{new QDoubleSpinBox(this)}
    , m_drawValues{new QComboBox(this)}
    , m_sampleValues{new QSpinBox(this)}
    , m_downmix{new QComboBox(this)}
    , m_antialiasing{new QCheckBox(tr("Anti-aliasing"), this)}
    , m_colourGroup{new QGroupBox(tr("Custom colours"), this)}
    , m_bgUnplayed{new ColourButton(this)}
    , m_bgPlayed{new ColourButton(this)}
    , m_fgUnplayed{new ColourButton(this)}
    , m_fgPlayed{new ColourButton(this)}
    , m_rmsUnplayed{new ColourButton(this)}
    , m_rmsPlayed{new ColourButton(this)}
    , m_cursorColour{new ColourButton(this)}
    , m_seekingCursorColour{new ColourButton(this)}
{
    auto* layout = new QGridLayout(this);

    auto* appearanceGroup  = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearanceGroup);

    auto* channelHeightLabel = new QLabel(tr("Channel Scale") + QStringLiteral(":"), this);
    auto* cursorWidthLabel   = new QLabel(tr("Cursor Width") + QStringLiteral(":"), this);
    auto* drawValuesLabel    = new QLabel(tr("Draw Values") + QStringLiteral(":"), this);
    auto* sampleValuesLabel  = new QLabel(tr("Sample Pixel Ratio") + QStringLiteral(":"), this);
    auto* downMixLabel       = new QLabel(tr("Downmix") + QStringLiteral(":"), this);

    m_colourGroup->setCheckable(true);

    auto* coloursLayout = new QGridLayout(m_colourGroup);

    auto* unPlayedLabel      = new QLabel(tr("Unplayed"), this);
    auto* playedLabel        = new QLabel(tr("Played"), this);
    auto* bgLabel            = new QLabel(tr("Background"), this);
    auto* fgLabel            = new QLabel(tr("Foreground"), this);
    auto* rmsLabel           = new QLabel(tr("RMS"), this);
    auto* playingCursorLabel = new QLabel(tr("Playing"), this);
    auto* seekingCursorLabel = new QLabel(tr("Seeking"), this);
    auto* cursorLabel        = new QLabel(tr("Cursor"), this);

    coloursLayout->addWidget(unPlayedLabel, 0, 1, Qt::AlignCenter);
    coloursLayout->addWidget(playedLabel, 0, 2, Qt::AlignCenter);
    coloursLayout->addWidget(bgLabel, 1, 0);
    coloursLayout->addWidget(fgLabel, 2, 0);
    coloursLayout->addWidget(rmsLabel, 3, 0);
    coloursLayout->addWidget(playingCursorLabel, 4, 1, Qt::AlignCenter);
    coloursLayout->addWidget(seekingCursorLabel, 4, 2, Qt::AlignCenter);
    coloursLayout->addWidget(cursorLabel, 5, 0);

    coloursLayout->addWidget(m_bgUnplayed, 1, 1);
    coloursLayout->addWidget(m_bgPlayed, 1, 2);
    coloursLayout->addWidget(m_fgUnplayed, 2, 1);
    coloursLayout->addWidget(m_fgPlayed, 2, 2);
    coloursLayout->addWidget(m_rmsUnplayed, 3, 1);
    coloursLayout->addWidget(m_rmsPlayed, 3, 2);
    coloursLayout->addWidget(m_cursorColour, 5, 1);
    coloursLayout->addWidget(m_seekingCursorColour, 5, 2);

    coloursLayout->setColumnStretch(1, 1);
    coloursLayout->setColumnStretch(2, 1);

    m_cursorWidth->setMinimum(1.0);
    m_cursorWidth->setMaximum(20.0);
    m_cursorWidth->setSingleStep(0.1);

    m_channelHeightScale->setMinimum(0.1);
    m_channelHeightScale->setMaximum(1.0);
    m_channelHeightScale->setSingleStep(0.1);

    m_sampleValues->setMinimum(1);
    m_sampleValues->setMaximum(100);
    m_sampleValues->setSingleStep(1);

    appearanceLayout->addWidget(m_antialiasing, 0, 0, 1, 2);
    appearanceLayout->addWidget(m_showCursor, 1, 0, 1, 2);
    appearanceLayout->addWidget(cursorWidthLabel, 2, 0);
    appearanceLayout->addWidget(m_cursorWidth, 2, 1);
    appearanceLayout->addWidget(channelHeightLabel, 3, 0);
    appearanceLayout->addWidget(m_channelHeightScale, 3, 1);
    appearanceLayout->addWidget(drawValuesLabel, 4, 0);
    appearanceLayout->addWidget(m_drawValues, 4, 1);
    appearanceLayout->addWidget(sampleValuesLabel, 5, 0);
    appearanceLayout->addWidget(m_sampleValues, 5, 1);
    appearanceLayout->addWidget(downMixLabel, 6, 0);
    appearanceLayout->addWidget(m_downmix, 6, 1);

    layout->addWidget(appearanceGroup, 0, 1);
    layout->addWidget(m_colourGroup, 0, 2);

    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(2, 1);

    QObject::connect(m_showCursor, &QCheckBox::stateChanged, this,
                     [this]() { m_cursorWidth->setEnabled(m_showCursor->isChecked()); });
}

void WaveBarSettingsPageWidget::load()
{
    m_showCursor->setChecked(m_settings->value<Settings::WaveBar::ShowCursor>());
    m_cursorWidth->setValue(m_settings->value<Settings::WaveBar::CursorWidth>());
    m_channelHeightScale->setValue(m_settings->value<Settings::WaveBar::ChannelHeightScale>());

    m_drawValues->clear();

    m_drawValues->addItem(tr("All"));
    m_drawValues->addItem(tr("MinMax"));
    m_drawValues->addItem(tr("RMS"));

    const auto valuesOption = static_cast<ValueOptions>(m_settings->value<Settings::WaveBar::DrawValues>());
    if(valuesOption == ValueOptions::All) {
        m_drawValues->setCurrentIndex(0);
    }
    else if(valuesOption == ValueOptions::MinMax) {
        m_drawValues->setCurrentIndex(1);
    }
    else {
        m_drawValues->setCurrentIndex(2);
    }

    m_sampleValues->setValue(m_settings->value<Settings::WaveBar::SamplePixelRatio>());

    m_downmix->clear();

    m_downmix->addItem(tr("Off"));
    m_downmix->addItem(tr("Stereo"));
    m_downmix->addItem(tr("Mono"));

    const auto downMixOption = static_cast<DownmixOption>(m_settings->value<Settings::WaveBar::Downmix>());
    if(downMixOption == DownmixOption::Off) {
        m_downmix->setCurrentIndex(0);
    }
    else if(downMixOption == DownmixOption::Stereo) {
        m_downmix->setCurrentIndex(1);
    }
    else {
        m_downmix->setCurrentIndex(2);
    }

    m_antialiasing->setChecked(m_settings->value<Settings::WaveBar::Antialiasing>());

    const auto currentColours = m_settings->value<Settings::WaveBar::ColourOptions>().value<Colours>();
    m_colourGroup->setChecked(currentColours != Colours{});

    m_bgUnplayed->setColour(currentColours.bgUnplayed);
    m_bgPlayed->setColour(currentColours.bgPlayed);
    m_fgUnplayed->setColour(currentColours.fgUnplayed);
    m_fgPlayed->setColour(currentColours.fgPlayed);
    m_rmsUnplayed->setColour(currentColours.rmsUnplayed);
    m_rmsPlayed->setColour(currentColours.rmsPlayed);
    m_cursorColour->setColour(currentColours.cursor);
    m_seekingCursorColour->setColour(currentColours.seekingCursor);
}

void WaveBarSettingsPageWidget::apply()
{
    m_settings->set<Settings::WaveBar::ShowCursor>(m_showCursor->isChecked());
    m_settings->set<Settings::WaveBar::CursorWidth>(m_cursorWidth->value());
    m_settings->set<Settings::WaveBar::ChannelHeightScale>(m_channelHeightScale->value());
    m_settings->set<Settings::WaveBar::Downmix>(m_downmix->currentIndex());
    m_settings->set<Settings::WaveBar::DrawValues>(m_drawValues->currentIndex());
    m_settings->set<Settings::WaveBar::SamplePixelRatio>(m_sampleValues->value());
    m_settings->set<Settings::WaveBar::Antialiasing>(m_antialiasing->isChecked());

    Colours colours;

    if(m_colourGroup->isChecked()) {
        colours.bgUnplayed    = m_bgUnplayed->colour();
        colours.bgPlayed      = m_bgPlayed->colour();
        colours.fgUnplayed    = m_fgUnplayed->colour();
        colours.fgPlayed      = m_fgPlayed->colour();
        colours.rmsUnplayed   = m_rmsUnplayed->colour();
        colours.rmsPlayed     = m_rmsPlayed->colour();
        colours.cursor        = m_cursorColour->colour();
        colours.seekingCursor = m_seekingCursorColour->colour();
        m_settings->set<Settings::WaveBar::ColourOptions>(QVariant::fromValue(colours));
    }
    else {
        m_settings->set<Settings::WaveBar::ColourOptions>(QVariant{});
    }
}

void WaveBarSettingsPageWidget::reset()
{
    m_settings->reset<Settings::WaveBar::Downmix>();
    m_settings->reset<Settings::WaveBar::ChannelHeightScale>();
    m_settings->reset<Settings::WaveBar::ShowCursor>();
    m_settings->reset<Settings::WaveBar::CursorWidth>();
    m_settings->reset<Settings::WaveBar::ColourOptions>();
    m_settings->reset<Settings::WaveBar::DrawValues>();
    m_settings->reset<Settings::WaveBar::SamplePixelRatio>();
    m_settings->reset<Settings::WaveBar::Antialiasing>();
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
