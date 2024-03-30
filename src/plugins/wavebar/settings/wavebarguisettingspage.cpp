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

#include "wavebarguisettingspage.h"

#include "settings/wavebarsettings.h"
#include "wavebarcolours.h"
#include "wavebarconstants.h"

#include <utils/settings/settingsmanager.h>
#include <utils/widgets/colourbutton.h>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

namespace Fooyin::WaveBar {
class WaveBarGuiSettingsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit WaveBarGuiSettingsPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QDoubleSpinBox* m_cursorWidth;
    QSpinBox* m_barWidth;
    QSpinBox* m_barGap;

    QGroupBox* m_colourGroup;
    ColourButton* m_bgUnplayed;
    ColourButton* m_bgPlayed;
    ColourButton* m_fgUnplayed;
    ColourButton* m_fgPlayed;
    ColourButton* m_fgBorder;
    ColourButton* m_rmsUnplayed;
    ColourButton* m_rmsPlayed;
    ColourButton* m_rmsBorder;
    ColourButton* m_cursorColour;
    ColourButton* m_seekingCursorColour;
};

WaveBarGuiSettingsPageWidget::WaveBarGuiSettingsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_cursorWidth{new QDoubleSpinBox(this)}
    , m_barWidth{new QSpinBox(this)}
    , m_barGap{new QSpinBox(this)}
    , m_colourGroup{new QGroupBox(tr("Custom colours"), this)}
    , m_bgUnplayed{new ColourButton(this)}
    , m_bgPlayed{new ColourButton(this)}
    , m_fgUnplayed{new ColourButton(this)}
    , m_fgPlayed{new ColourButton(this)}
    , m_fgBorder{new ColourButton(this)}
    , m_rmsUnplayed{new ColourButton(this)}
    , m_rmsPlayed{new ColourButton(this)}
    , m_rmsBorder{new ColourButton(this)}
    , m_cursorColour{new ColourButton(this)}
    , m_seekingCursorColour{new ColourButton(this)}
{
    auto* layout = new QGridLayout(this);

    auto* cursorWidthLabel = new QLabel(tr("Cursor Width") + QStringLiteral(":"), this);
    auto* barWidthLabel    = new QLabel(tr("Bar Width") + QStringLiteral(":"), this);
    auto* barGapLabel      = new QLabel(tr("Bar Gap") + QStringLiteral(":"), this);

    m_cursorWidth->setMinimum(0.0);
    m_cursorWidth->setMaximum(20.0);
    m_cursorWidth->setSingleStep(0.5);

    m_barWidth->setMinimum(1);
    m_barWidth->setMaximum(50);
    m_barWidth->setSingleStep(1);

    m_barGap->setMinimum(0);
    m_barGap->setMaximum(50);
    m_barGap->setSingleStep(1);

    m_colourGroup->setCheckable(true);

    auto* coloursLayout = new QGridLayout(m_colourGroup);

    auto* unPlayedLabel      = new QLabel(tr("Unplayed"), this);
    auto* playedLabel        = new QLabel(tr("Played"), this);
    auto* borderLabel        = new QLabel(tr("Border"), this);
    auto* bgLabel            = new QLabel(tr("Background"), this);
    auto* fgLabel            = new QLabel(tr("Foreground"), this);
    auto* rmsLabel           = new QLabel(tr("RMS"), this);
    auto* playingCursorLabel = new QLabel(tr("Playing"), this);
    auto* seekingCursorLabel = new QLabel(tr("Seeking"), this);
    auto* cursorLabel        = new QLabel(tr("Cursor"), this);

    coloursLayout->addWidget(unPlayedLabel, 0, 1, Qt::AlignCenter);
    coloursLayout->addWidget(playedLabel, 0, 2, Qt::AlignCenter);
    coloursLayout->addWidget(borderLabel, 0, 3, Qt::AlignCenter);
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
    coloursLayout->addWidget(m_fgBorder, 2, 3);
    coloursLayout->addWidget(m_rmsUnplayed, 3, 1);
    coloursLayout->addWidget(m_rmsPlayed, 3, 2);
    coloursLayout->addWidget(m_rmsBorder, 3, 3);
    coloursLayout->addWidget(m_cursorColour, 5, 1);
    coloursLayout->addWidget(m_seekingCursorColour, 5, 2);

    coloursLayout->setColumnStretch(1, 1);
    coloursLayout->setColumnStretch(2, 1);
    coloursLayout->setColumnStretch(3, 1);

    int row{0};
    layout->addWidget(cursorWidthLabel, row, 0);
    layout->addWidget(m_cursorWidth, row++, 1);
    layout->addWidget(barWidthLabel, row, 0);
    layout->addWidget(m_barWidth, row++, 1);
    layout->addWidget(barGapLabel, row, 0);
    layout->addWidget(m_barGap, row++, 1);
    layout->addWidget(m_colourGroup, row, 0, 1, 3);

    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(2, 1);
}

void WaveBarGuiSettingsPageWidget::load()
{
    m_cursorWidth->setValue(m_settings->value<Settings::WaveBar::CursorWidth>());
    m_barWidth->setValue(m_settings->value<Settings::WaveBar::BarWidth>());
    m_barGap->setValue(m_settings->value<Settings::WaveBar::BarGap>());

    const auto currentColours = m_settings->value<Settings::WaveBar::ColourOptions>().value<Colours>();
    m_colourGroup->setChecked(currentColours != Colours{});

    m_bgUnplayed->setColour(currentColours.bgUnplayed);
    m_bgPlayed->setColour(currentColours.bgPlayed);
    m_fgUnplayed->setColour(currentColours.fgUnplayed);
    m_fgPlayed->setColour(currentColours.fgPlayed);
    m_fgBorder->setColour(currentColours.fgBorder);
    m_rmsUnplayed->setColour(currentColours.rmsUnplayed);
    m_rmsPlayed->setColour(currentColours.rmsPlayed);
    m_rmsBorder->setColour(currentColours.rmsBorder);
    m_cursorColour->setColour(currentColours.cursor);
    m_seekingCursorColour->setColour(currentColours.seekingCursor);
}

void WaveBarGuiSettingsPageWidget::apply()
{
    m_settings->set<Settings::WaveBar::CursorWidth>(m_cursorWidth->value());
    m_settings->set<Settings::WaveBar::BarWidth>(m_barWidth->value());
    m_settings->set<Settings::WaveBar::BarGap>(m_barGap->value());

    Colours colours;

    if(m_colourGroup->isChecked()) {
        colours.bgUnplayed    = m_bgUnplayed->colour();
        colours.bgPlayed      = m_bgPlayed->colour();
        colours.fgUnplayed    = m_fgUnplayed->colour();
        colours.fgPlayed      = m_fgPlayed->colour();
        colours.fgBorder      = m_fgBorder->colour();
        colours.rmsUnplayed   = m_rmsUnplayed->colour();
        colours.rmsPlayed     = m_rmsPlayed->colour();
        colours.rmsBorder     = m_rmsBorder->colour();
        colours.cursor        = m_cursorColour->colour();
        colours.seekingCursor = m_seekingCursorColour->colour();
        m_settings->set<Settings::WaveBar::ColourOptions>(QVariant::fromValue(colours));
    }
    else {
        m_settings->set<Settings::WaveBar::ColourOptions>(QVariant{});
    }
}

void WaveBarGuiSettingsPageWidget::reset()
{
    m_settings->reset<Settings::WaveBar::CursorWidth>();
    m_settings->reset<Settings::WaveBar::BarWidth>();
    m_settings->reset<Settings::WaveBar::BarGap>();
    m_settings->reset<Settings::WaveBar::ColourOptions>();
}

WaveBarGuiSettingsPage::WaveBarGuiSettingsPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::WaveBarColours);
    setName(tr("Appearance"));
    setCategory({tr("Plugins"), tr("WaveBar")});
    setWidgetCreator([settings] { return new WaveBarGuiSettingsPageWidget(settings); });
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarguisettingspage.cpp"
#include "wavebarguisettingspage.moc"
