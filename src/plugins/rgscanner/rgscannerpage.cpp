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

#include "rgscannerpage.h"

#include "rgscanner.h"
#include "rgscannerdefs.h"

#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>

#include <QButtonGroup>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>

using namespace Qt::StringLiterals;

namespace Fooyin::RGScanner {
class RGScannerPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit RGScannerPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    std::map<QString, QRadioButton*> m_scanners;
    QRadioButton* m_samplePeak;
    QRadioButton* m_truePeak;
    ScriptLineEdit* m_albumGroupScript;
};

RGScannerPageWidget::RGScannerPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_samplePeak{new QRadioButton(tr("Use sample peak for calculating peaks"), this)}
    , m_truePeak{new QRadioButton(tr("Use true peak for calculating peaks"), this)}
    , m_albumGroupScript{new ScriptLineEdit(this)}
{
    auto* scannerGroup  = new QGroupBox(tr("Scanner"), this);
    auto* scannerLayout = new QGridLayout(scannerGroup);

    int row{0};

    const auto scanners = RGScanner::scannerNames();
    for(const QString& name : scanners) {
        auto* scannerOption = new QRadioButton(name, scannerGroup);
        m_scanners.emplace(name, scannerOption);
        scannerLayout->addWidget(scannerOption, row++, 0);
    }

    auto* optionsGroup  = new QGroupBox(tr("Options"), this);
    auto* optionsLayout = new QGridLayout(optionsGroup);

    auto* peakGroup = new QButtonGroup(this);
    peakGroup->addButton(m_samplePeak);
    peakGroup->addButton(m_truePeak);

    auto* albumGroupLabel = new QLabel(tr("Album grouping pattern") + ":"_L1, this);

    const auto albumGroupToolTip
        = tr("Used with the %1 action").arg("'"_L1 + tr("Calculate as albums (by tags)") + "'"_L1);
    albumGroupLabel->setToolTip(albumGroupToolTip);
    m_albumGroupScript->setToolTip(albumGroupToolTip);

    row = 0;
    optionsLayout->addWidget(m_truePeak, row++, 0, 1, 2);
    optionsLayout->addWidget(m_samplePeak, row++, 0, 1, 2);
    optionsLayout->addWidget(albumGroupLabel, row, 0);
    optionsLayout->addWidget(m_albumGroupScript, row++, 1);

    auto* layout = new QGridLayout(this);

    row = 0;
    layout->addWidget(scannerGroup, row++, 0, 1, 2);
    layout->addWidget(optionsGroup, row++, 0, 1, 2);
    layout->setColumnStretch(1, 1);
    layout->setRowStretch(layout->rowCount(), 1);
}

void RGScannerPageWidget::load()
{
    const auto scanner = m_settings->fileValue(ScannerOption, u"libebur128"_s).toString();
    if(m_scanners.contains(scanner)) {
        m_scanners.at(scanner)->setChecked(true);
    }
    else {
        m_scanners.cbegin()->second->setChecked(true);
    }

    m_truePeak->setChecked(m_settings->fileValue(TruePeakSetting, false).toBool());
    m_samplePeak->setChecked(!m_truePeak->isChecked());
    m_albumGroupScript->setText(
        m_settings->fileValue(AlbumGroupScriptSetting, QString::fromLatin1(DefaultAlbumGroupScript)).toString());
}

void RGScannerPageWidget::apply()
{
    for(const auto& [name, option] : m_scanners) {
        if(option->isChecked()) {
            m_settings->fileSet(QLatin1String{ScannerOption}, name);
        }
    }

    m_settings->fileSet(TruePeakSetting, m_truePeak->isChecked());
    m_settings->fileSet(AlbumGroupScriptSetting, m_albumGroupScript->text());
}

void RGScannerPageWidget::reset()
{
    m_settings->fileRemove(ScannerOption);
    m_settings->fileRemove(TruePeakSetting);
    m_settings->fileRemove(AlbumGroupScriptSetting);
}

RGScannerPage::RGScannerPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(ScannerPage);
    setName(tr("Calculation"));
    setCategory({tr("Playback"), tr("ReplayGain")});
    setWidgetCreator([settings] { return new RGScannerPageWidget(settings); });
}
} // namespace Fooyin::RGScanner

#include "moc_rgscannerpage.cpp"
#include "rgscannerpage.moc"
