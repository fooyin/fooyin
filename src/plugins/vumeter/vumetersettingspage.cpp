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

#include "vumetersettingspage.h"

#include "vumetercolours.h"
#include "vumetersettings.h"

#include <gui/guisettings.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/doubleslidereditor.h>
#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>

namespace Fooyin::VuMeter {
class VuMeterSettingsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit VuMeterSettingsPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QDoubleSpinBox* m_peakHold;
    QDoubleSpinBox* m_falloff;
    QSpinBox* m_channelSpacing;
    QSpinBox* m_barSize;
    QSpinBox* m_barSpacing;
    QSpinBox* m_barSections;
    QSpinBox* m_sectionSpacing;

    QGroupBox* m_colourGroup;
    ColourButton* m_bgColour;
    ColourButton* m_peakColour;
    ColourButton* m_leftColour;
    ColourButton* m_rightColour;
};

VuMeterSettingsPageWidget::VuMeterSettingsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_peakHold{new QDoubleSpinBox(this)}
    , m_falloff{new QDoubleSpinBox(this)}
    , m_channelSpacing{new QSpinBox(this)}
    , m_barSize{new QSpinBox(this)}
    , m_barSpacing{new QSpinBox(this)}
    , m_barSections{new QSpinBox(this)}
    , m_sectionSpacing{new QSpinBox(this)}
    , m_colourGroup{new QGroupBox(tr("Custom colours"), this)}
    , m_bgColour{new ColourButton(this)}
    , m_peakColour{new ColourButton(this)}
    , m_leftColour{new ColourButton(this)}
    , m_rightColour{new ColourButton(this)}
{
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    m_peakHold->setRange(0.1, 30);
    m_peakHold->setSuffix(u" " + tr("seconds"));
    m_falloff->setRange(0.1, 96);
    m_falloff->setSuffix(u" " + tr("dB per second"));

    int row{0};
    generalLayout->addWidget(new QLabel(tr("Peak hold time") + u":", this), row, 0);
    generalLayout->addWidget(m_peakHold, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Falloff time") + u":", this), row, 0);
    generalLayout->addWidget(m_falloff, row++, 1);
    generalLayout->setColumnStretch(2, 1);

    auto* dimensionGroup  = new QGroupBox(tr("Dimension"), this);
    auto* dimensionLayout = new QGridLayout(dimensionGroup);

    m_channelSpacing->setRange(0, 20);
    m_channelSpacing->setSuffix(QStringLiteral(" px"));

    m_barSize->setRange(0, 50);
    m_barSize->setSuffix(QStringLiteral(" px"));

    m_barSections->setRange(1, 20);

    m_barSpacing->setSuffix(QStringLiteral(" px"));
    m_barSpacing->setRange(1, 20);

    m_sectionSpacing->setSuffix(QStringLiteral(" px"));
    m_sectionSpacing->setRange(1, 20);

    row = 0;
    dimensionLayout->addWidget(new QLabel(tr("Channel spacing") + u":", this), row, 0);
    dimensionLayout->addWidget(m_channelSpacing, row++, 1);
    dimensionLayout->addWidget(new QLabel(tr("Bar size") + u":", this), row, 0);
    dimensionLayout->addWidget(m_barSize, row++, 1);
    dimensionLayout->addWidget(new QLabel(tr("Bar spacing") + u":", this), row, 0);
    dimensionLayout->addWidget(m_barSpacing, row++, 1);

    row = 0;
    dimensionLayout->addWidget(new QLabel(tr("Sections") + u":", this), row, 2);
    dimensionLayout->addWidget(m_barSections, row++, 3);
    dimensionLayout->addWidget(new QLabel(tr("Section spacing") + u":", this), row, 2);
    dimensionLayout->addWidget(m_sectionSpacing, row++, 3);
    dimensionLayout->setColumnStretch(4, 1);

    m_colourGroup->setCheckable(true);
    auto* coloursLayout = new QGridLayout(m_colourGroup);

    row = 0;
    coloursLayout->addWidget(new QLabel(tr("Background colour") + u":", this), row, 0);
    coloursLayout->addWidget(m_bgColour, row++, 1);
    coloursLayout->addWidget(new QLabel(tr("Peak colour") + u":", this), row, 0);
    coloursLayout->addWidget(m_peakColour, row++, 1);
    coloursLayout->addWidget(new QLabel(tr("Bar colours") + u":", this), row, 0);
    coloursLayout->addWidget(m_leftColour, row, 1);
    coloursLayout->addWidget(m_rightColour, row++, 2);
    coloursLayout->setColumnStretch(1, 1);
    coloursLayout->setColumnStretch(2, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(generalGroup, row++, 0);
    layout->addWidget(dimensionGroup, row++, 0);
    layout->addWidget(m_colourGroup, row++, 0);
    layout->setRowStretch(layout->rowCount(), 1);

    m_settings->subscribe<Settings::Gui::Theme>(this, &SettingsPageWidget::load);
    m_settings->subscribe<Settings::Gui::Style>(this, &SettingsPageWidget::load);
}

void VuMeterSettingsPageWidget::load()
{
    m_peakHold->setValue(m_settings->value<Settings::VuMeter::PeakHoldTime>());
    m_falloff->setValue(m_settings->value<Settings::VuMeter::FalloffTime>());
    m_channelSpacing->setValue(m_settings->value<Settings::VuMeter::ChannelSpacing>());
    m_barSize->setValue(m_settings->value<Settings::VuMeter::BarSize>());
    m_barSpacing->setValue(m_settings->value<Settings::VuMeter::BarSpacing>());
    m_barSections->setValue(m_settings->value<Settings::VuMeter::BarSections>());
    m_sectionSpacing->setValue(m_settings->value<Settings::VuMeter::SectionSpacing>());

    const auto currentColours = m_settings->value<Settings::VuMeter::MeterColours>().value<Colours>();
    m_colourGroup->setChecked(currentColours != Colours{});

    m_bgColour->setColour(currentColours.colour(Colours::Type::Background));
    m_peakColour->setColour(currentColours.colour(Colours::Type::Peak));
    m_leftColour->setColour(currentColours.colour(Colours::Type::Gradient1));
    m_rightColour->setColour(currentColours.colour(Colours::Type::Gradient2));
}

void VuMeterSettingsPageWidget::apply()
{
    m_settings->set<Settings::VuMeter::PeakHoldTime>(m_peakHold->value());
    m_settings->set<Settings::VuMeter::FalloffTime>(m_falloff->value());
    m_settings->set<Settings::VuMeter::ChannelSpacing>(m_channelSpacing->value());
    m_settings->set<Settings::VuMeter::BarSize>(m_barSize->value());
    m_settings->set<Settings::VuMeter::BarSpacing>(m_barSpacing->value());
    m_settings->set<Settings::VuMeter::BarSections>(m_barSections->value());
    m_settings->set<Settings::VuMeter::SectionSpacing>(m_sectionSpacing->value());

    Colours colours;

    if(m_colourGroup->isChecked()) {
        colours.setColour(Colours::Type::Background, m_bgColour->colour());
        colours.setColour(Colours::Type::Peak, m_peakColour->colour());
        colours.setColour(Colours::Type::Gradient1, m_leftColour->colour());
        colours.setColour(Colours::Type::Gradient2, m_rightColour->colour());
        m_settings->set<Settings::VuMeter::MeterColours>(QVariant::fromValue(colours));
    }
    else {
        m_settings->set<Settings::VuMeter::MeterColours>(QVariant{});
        load();
    }
}

void VuMeterSettingsPageWidget::reset()
{
    m_settings->reset<Settings::VuMeter::PeakHoldTime>();
    m_settings->reset<Settings::VuMeter::FalloffTime>();
    m_settings->reset<Settings::VuMeter::ChannelSpacing>();
    m_settings->reset<Settings::VuMeter::BarSize>();
    m_settings->reset<Settings::VuMeter::BarSpacing>();
    m_settings->reset<Settings::VuMeter::BarSections>();
    m_settings->reset<Settings::VuMeter::SectionSpacing>();
    m_settings->reset<Settings::VuMeter::MeterColours>();
}

VuMeterSettingsPage::VuMeterSettingsPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(VuMeterPage);
    setName(tr("General"));
    setCategory({tr("Widgets"), tr("VU Meter")});
    setWidgetCreator([settings] { return new VuMeterSettingsPageWidget(settings); });
}
} // namespace Fooyin::VuMeter

#include "moc_vumetersettingspage.cpp"
#include "vumetersettingspage.moc"
