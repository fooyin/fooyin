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

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
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

    QDoubleSpinBox* m_channelHeightScale;
    QComboBox* m_drawValues;
    QComboBox* m_downmix;
};

WaveBarSettingsPageWidget::WaveBarSettingsPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_channelHeightScale{new QDoubleSpinBox(this)}
    , m_drawValues{new QComboBox(this)}
    , m_downmix{new QComboBox(this)}
{
    auto* layout = new QGridLayout(this);

    auto* channelHeightLabel = new QLabel(tr("Channel Scale") + QStringLiteral(":"), this);
    auto* drawValuesLabel    = new QLabel(tr("Draw Values") + QStringLiteral(":"), this);
    auto* downMixLabel       = new QLabel(tr("Downmix") + QStringLiteral(":"), this);

    m_channelHeightScale->setMinimum(0.1);
    m_channelHeightScale->setMaximum(1.0);
    m_channelHeightScale->setSingleStep(0.1);

    int row{0};
    layout->addWidget(drawValuesLabel, row, 0);
    layout->addWidget(m_drawValues, row++, 1);
    layout->addWidget(downMixLabel, row, 0);
    layout->addWidget(m_downmix, row++, 1);
    layout->addWidget(channelHeightLabel, row, 0);
    layout->addWidget(m_channelHeightScale, row++, 1);

    layout->setRowStretch(layout->rowCount(), 1);
    layout->setColumnStretch(2, 1);
}

void WaveBarSettingsPageWidget::load()
{
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
}

void WaveBarSettingsPageWidget::apply()
{
    m_settings->set<Settings::WaveBar::ChannelHeightScale>(m_channelHeightScale->value());
    m_settings->set<Settings::WaveBar::Downmix>(m_downmix->currentIndex());
    m_settings->set<Settings::WaveBar::DrawValues>(m_drawValues->currentIndex());
}

void WaveBarSettingsPageWidget::reset()
{
    m_settings->reset<Settings::WaveBar::Downmix>();
    m_settings->reset<Settings::WaveBar::ChannelHeightScale>();
    m_settings->reset<Settings::WaveBar::ColourOptions>();
    m_settings->reset<Settings::WaveBar::DrawValues>();
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
