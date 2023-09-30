/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "filtersappearancepage.h"

#include "constants.h"
#include "filtersettings.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Fy::Filters::Settings {
class FiltersAppearancePageWidget : public Utils::SettingsPageWidget
{
public:
    explicit FiltersAppearancePageWidget(Utils::SettingsManager* settings);

    void apply() override;

private:
    Utils::SettingsManager* m_settings;

    QCheckBox* m_filterHeaders;
    QCheckBox* m_filterScrollBars;
    QCheckBox* m_altRowColours;

    QSpinBox* m_fontSize;
    QSpinBox* m_rowHeight;
};

FiltersAppearancePageWidget::FiltersAppearancePageWidget(Utils::SettingsManager* settings)
    : m_settings{settings}
    , m_filterHeaders{new QCheckBox("Show Headers", this)}
    , m_filterScrollBars{new QCheckBox("Show Scrollbars", this)}
    , m_altRowColours{new QCheckBox("Alternating Row Colours", this)}
    , m_fontSize{new QSpinBox(this)}
    , m_rowHeight{new QSpinBox(this)}
{
    m_filterHeaders->setChecked(m_settings->value<Settings::FilterHeader>());
    m_filterScrollBars->setChecked(m_settings->value<Settings::FilterScrollBar>());
    m_altRowColours->setChecked(m_settings->value<Settings::FilterAltColours>());
    m_fontSize->setValue(m_settings->value<Settings::FilterFontSize>());
    m_rowHeight->setValue(m_settings->value<Settings::FilterRowHeight>());

    auto* fontSizeLabel  = new QLabel("Font Size", this);
    auto* rowHeightLabel = new QLabel("Row Height", this);

    auto* fontSizeBox = new QHBoxLayout();
    fontSizeBox->addWidget(fontSizeLabel);
    fontSizeBox->addWidget(m_fontSize);
    fontSizeBox->addStretch();

    auto* rowHeightBox = new QHBoxLayout();
    rowHeightBox->addWidget(rowHeightLabel);
    rowHeightBox->addWidget(m_rowHeight);
    rowHeightBox->addStretch();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_filterHeaders);
    mainLayout->addWidget(m_filterScrollBars);
    mainLayout->addWidget(m_altRowColours);
    mainLayout->addLayout(fontSizeBox);
    mainLayout->addLayout(rowHeightBox);

    mainLayout->addStretch();
}

void FiltersAppearancePageWidget::apply()
{
    m_settings->set<Settings::FilterHeader>(m_filterHeaders->isChecked());
    m_settings->set<Settings::FilterScrollBar>(m_filterScrollBars->isChecked());
    m_settings->set<Settings::FilterAltColours>(m_altRowColours->isChecked());
    m_settings->set<Settings::FilterFontSize>(m_fontSize->value());
    m_settings->set<Settings::FilterRowHeight>(m_rowHeight->value());
}

FiltersAppearancePage::FiltersAppearancePage(Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersAppearance);
    setName(tr("Appearance"));
    setCategory({"Plugins", "Filters"});
    setWidgetCreator([settings] {
        return new FiltersAppearancePageWidget(settings);
    });
}
} // namespace Fy::Filters::Settings
