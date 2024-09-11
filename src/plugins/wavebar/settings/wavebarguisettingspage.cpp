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

#include <gui/guisettings.h>
#include <gui/widgets/colourbutton.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
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

    QGroupBox* m_colourGroup;
    ColourButton* m_bgUnplayed;
    ColourButton* m_bgPlayed;
    ColourButton* m_maxUnplayed;
    ColourButton* m_maxPlayed;
    ColourButton* m_maxBorder;
    ColourButton* m_minUnplayed;
    ColourButton* m_minPlayed;
    ColourButton* m_minBorder;
    ColourButton* m_rmsMaxUnplayed;
    ColourButton* m_rmsMaxPlayed;
    ColourButton* m_rmsMaxBorder;
    ColourButton* m_rmsMinUnplayed;
    ColourButton* m_rmsMinPlayed;
    ColourButton* m_rmsMinBorder;
    ColourButton* m_cursorColour;
    ColourButton* m_seekingCursorColour;
};

WaveBarGuiSettingsPageWidget::WaveBarGuiSettingsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_colourGroup{new QGroupBox(tr("Custom colours"), this)}
    , m_bgUnplayed{new ColourButton(this)}
    , m_bgPlayed{new ColourButton(this)}
    , m_maxUnplayed{new ColourButton(this)}
    , m_maxPlayed{new ColourButton(this)}
    , m_maxBorder{new ColourButton(this)}
    , m_minUnplayed{new ColourButton(this)}
    , m_minPlayed{new ColourButton(this)}
    , m_minBorder{new ColourButton(this)}
    , m_rmsMaxUnplayed{new ColourButton(this)}
    , m_rmsMaxPlayed{new ColourButton(this)}
    , m_rmsMaxBorder{new ColourButton(this)}
    , m_rmsMinUnplayed{new ColourButton(this)}
    , m_rmsMinPlayed{new ColourButton(this)}
    , m_rmsMinBorder{new ColourButton(this)}
    , m_cursorColour{new ColourButton(this)}
    , m_seekingCursorColour{new ColourButton(this)}
{
    auto* layout = new QGridLayout(this);

    m_colourGroup->setCheckable(true);

    auto* coloursLayout = new QGridLayout(m_colourGroup);

    auto* unPlayedLabel      = new QLabel(tr("Unplayed"), this);
    auto* playedLabel        = new QLabel(tr("Played"), this);
    auto* borderLabel        = new QLabel(tr("Border"), this);
    auto* bgLabel            = new QLabel(tr("Background"), this);
    auto* maxLabel           = new QLabel(tr("Max"), this);
    auto* minLabel           = new QLabel(tr("Min"), this);
    auto* rmsMaxLabel        = new QLabel(tr("RMS Max"), this);
    auto* rmsMinLabel        = new QLabel(tr("RMS Min"), this);
    auto* playingCursorLabel = new QLabel(tr("Playing"), this);
    auto* seekingCursorLabel = new QLabel(tr("Seeking"), this);
    auto* cursorLabel        = new QLabel(tr("Cursor"), this);

    int row{0};
    coloursLayout->addWidget(unPlayedLabel, row, 1, Qt::AlignCenter);
    coloursLayout->addWidget(playedLabel, row, 2, Qt::AlignCenter);
    coloursLayout->addWidget(borderLabel, row++, 3, Qt::AlignCenter);
    coloursLayout->addWidget(bgLabel, row, 0);
    coloursLayout->addWidget(m_bgUnplayed, row, 1);
    coloursLayout->addWidget(m_bgPlayed, row++, 2);
    coloursLayout->addWidget(maxLabel, row, 0);
    coloursLayout->addWidget(m_maxUnplayed, row, 1);
    coloursLayout->addWidget(m_maxPlayed, row, 2);
    coloursLayout->addWidget(m_maxBorder, row++, 3);
    coloursLayout->addWidget(minLabel, row, 0);
    coloursLayout->addWidget(m_minUnplayed, row, 1);
    coloursLayout->addWidget(m_minPlayed, row, 2);
    coloursLayout->addWidget(m_minBorder, row++, 3);
    coloursLayout->addWidget(rmsMaxLabel, row, 0);
    coloursLayout->addWidget(m_rmsMaxUnplayed, row, 1);
    coloursLayout->addWidget(m_rmsMaxPlayed, row, 2);
    coloursLayout->addWidget(m_rmsMaxBorder, row++, 3);
    coloursLayout->addWidget(rmsMinLabel, row, 0);
    coloursLayout->addWidget(m_rmsMinUnplayed, row, 1);
    coloursLayout->addWidget(m_rmsMinPlayed, row, 2);
    coloursLayout->addWidget(m_rmsMinBorder, row++, 3);
    coloursLayout->addWidget(playingCursorLabel, row, 1, Qt::AlignCenter);
    coloursLayout->addWidget(seekingCursorLabel, row++, 2, Qt::AlignCenter);
    coloursLayout->addWidget(cursorLabel, row, 0);
    coloursLayout->addWidget(m_cursorColour, row, 1);
    coloursLayout->addWidget(m_seekingCursorColour, row, 2);

    coloursLayout->setColumnStretch(1, 1);
    coloursLayout->setColumnStretch(2, 1);
    coloursLayout->setColumnStretch(3, 1);

    layout->addWidget(m_colourGroup, 0, 0, 1, 3);

    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(2, 1);

    m_settings->subscribe<Settings::Gui::Theme>(this, &SettingsPageWidget::load);
    m_settings->subscribe<Settings::Gui::Style>(this, &SettingsPageWidget::load);
}

void WaveBarGuiSettingsPageWidget::load()
{
    const auto currentColours = m_settings->value<Settings::WaveBar::ColourOptions>().value<Colours>();
    m_colourGroup->setChecked(currentColours != Colours{});

    m_bgUnplayed->setColour(currentColours.bgUnplayed);
    m_bgPlayed->setColour(currentColours.bgPlayed);
    m_maxUnplayed->setColour(currentColours.maxUnplayed);
    m_maxPlayed->setColour(currentColours.maxPlayed);
    m_maxBorder->setColour(currentColours.maxBorder);
    m_minUnplayed->setColour(currentColours.minUnplayed);
    m_minPlayed->setColour(currentColours.minPlayed);
    m_minBorder->setColour(currentColours.minBorder);
    m_rmsMaxUnplayed->setColour(currentColours.rmsMaxUnplayed);
    m_rmsMaxPlayed->setColour(currentColours.rmsMaxPlayed);
    m_rmsMaxBorder->setColour(currentColours.rmsMaxBorder);
    m_rmsMinUnplayed->setColour(currentColours.rmsMinUnplayed);
    m_rmsMinPlayed->setColour(currentColours.rmsMinPlayed);
    m_rmsMinBorder->setColour(currentColours.rmsMinBorder);
    m_cursorColour->setColour(currentColours.cursor);
    m_seekingCursorColour->setColour(currentColours.seekingCursor);
}

void WaveBarGuiSettingsPageWidget::apply()
{
    Colours colours;

    if(m_colourGroup->isChecked()) {
        colours.bgUnplayed     = m_bgUnplayed->colour();
        colours.bgPlayed       = m_bgPlayed->colour();
        colours.maxUnplayed    = m_maxUnplayed->colour();
        colours.maxPlayed      = m_maxPlayed->colour();
        colours.maxBorder      = m_maxBorder->colour();
        colours.minUnplayed    = m_minUnplayed->colour();
        colours.minPlayed      = m_minPlayed->colour();
        colours.minBorder      = m_minBorder->colour();
        colours.rmsMaxUnplayed = m_rmsMaxUnplayed->colour();
        colours.rmsMaxPlayed   = m_rmsMaxPlayed->colour();
        colours.rmsMaxBorder   = m_rmsMaxBorder->colour();
        colours.rmsMinUnplayed = m_rmsMinUnplayed->colour();
        colours.rmsMinPlayed   = m_rmsMinPlayed->colour();
        colours.rmsMinBorder   = m_rmsMinBorder->colour();
        colours.cursor         = m_cursorColour->colour();
        colours.seekingCursor  = m_seekingCursorColour->colour();
        m_settings->set<Settings::WaveBar::ColourOptions>(QVariant::fromValue(colours));
    }
    else {
        m_settings->set<Settings::WaveBar::ColourOptions>(QVariant{});
        load();
    }
}

void WaveBarGuiSettingsPageWidget::reset()
{
    m_settings->set<Settings::WaveBar::ColourOptions>(QVariant::fromValue(Colours{}));
}

WaveBarGuiSettingsPage::WaveBarGuiSettingsPage(SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::WaveBarColours);
    setName(tr("Colours"));
    setCategory({tr("Widgets"), tr("WaveBar")});
    setWidgetCreator([settings] { return new WaveBarGuiSettingsPageWidget(settings); });
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarguisettingspage.cpp"
#include "wavebarguisettingspage.moc"
