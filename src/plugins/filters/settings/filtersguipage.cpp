/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "filtersguipage.h"

#include "filterconstants.h"
#include "filtersettings.h"

#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QApplication>
#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
class FiltersGuiPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit FiltersGuiPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QCheckBox* m_filterHeaders;
    QCheckBox* m_filterScrollBars;
    QCheckBox* m_altRowColours;

    QCheckBox* m_overrideRowHeight;
    QSpinBox* m_rowHeight;
    QSpinBox* m_iconWidth;
    QSpinBox* m_iconHeight;
};

FiltersGuiPageWidget::FiltersGuiPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_filterHeaders{new QCheckBox(tr("Show headers"), this)}
    , m_filterScrollBars{new QCheckBox(tr("Show scrollbars"), this)}
    , m_altRowColours{new QCheckBox(tr("Alternating row colours"), this)}
    , m_overrideRowHeight{new QCheckBox(tr("Override row height") + u":"_s, this)}
    , m_rowHeight{new QSpinBox(this)}
    , m_iconWidth{new QSpinBox(this)}
    , m_iconHeight{new QSpinBox(this)}
{
    auto* appearance       = new QGroupBox(tr("Appearance"), this);
    auto* appearanceLayout = new QGridLayout(appearance);

    m_rowHeight->setMinimum(1);

    auto* artworkMode   = new QGroupBox(tr("Artwork Mode"), this);
    auto* artworkLayout = new QGridLayout(artworkMode);

    m_iconWidth->setSuffix(u"px"_s);
    m_iconHeight->setSuffix(u"px"_s);

    m_iconWidth->setMaximum(2048);
    m_iconHeight->setMaximum(2048);

    m_iconWidth->setSingleStep(20);
    m_iconHeight->setSingleStep(20);

    auto* iconSizeHint = new QLabel(
        u"🛈 "_s + tr("Size can also be changed using %1 in the widget.").arg(u"<b>Ctrl+Scroll</b>"_s), this);

    int row{0};
    artworkLayout->addWidget(new QLabel(tr("Width") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_iconWidth, row++, 1);
    artworkLayout->addWidget(new QLabel(tr("Height") + u":"_s, this), row, 0);
    artworkLayout->addWidget(m_iconHeight, row++, 1);
    artworkLayout->addWidget(iconSizeHint, row, 0, 1, 3);
    artworkLayout->setColumnStretch(3, 1);

    row = 0;
    appearanceLayout->addWidget(m_filterHeaders, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_filterScrollBars, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_altRowColours, row++, 0, 1, 2);
    appearanceLayout->addWidget(m_overrideRowHeight, row, 0, 1, 2);
    appearanceLayout->addWidget(m_rowHeight, row++, 2);
    appearanceLayout->setColumnStretch(3, 1);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(appearance, 0, 0);
    mainLayout->addWidget(artworkMode, 1, 0);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    QObject::connect(m_overrideRowHeight, &QCheckBox::toggled, m_rowHeight, &QWidget::setEnabled);
}

void FiltersGuiPageWidget::load()
{
    m_filterHeaders->setChecked(m_settings->value<Settings::Filters::FilterHeader>());
    m_filterScrollBars->setChecked(m_settings->value<Settings::Filters::FilterScrollBar>());
    m_altRowColours->setChecked(m_settings->value<Settings::Filters::FilterAltColours>());
    m_overrideRowHeight->setChecked(m_settings->value<Settings::Filters::FilterRowHeight>() > 0);
    m_rowHeight->setValue(m_settings->value<Settings::Filters::FilterRowHeight>());
    m_rowHeight->setEnabled(m_overrideRowHeight->isChecked());

    const auto iconSize = m_settings->value<Settings::Filters::FilterIconSize>().toSize();
    m_iconWidth->setValue(iconSize.width());
    m_iconHeight->setValue(iconSize.height());
}

void FiltersGuiPageWidget::apply()
{
    m_settings->set<Settings::Filters::FilterHeader>(m_filterHeaders->isChecked());
    m_settings->set<Settings::Filters::FilterScrollBar>(m_filterScrollBars->isChecked());
    m_settings->set<Settings::Filters::FilterAltColours>(m_altRowColours->isChecked());

    if(m_overrideRowHeight->isChecked()) {
        m_settings->set<Settings::Filters::FilterRowHeight>(m_rowHeight->value());
    }
    else {
        m_settings->reset<Settings::Filters::FilterRowHeight>();
    }

    const QSize iconSize{m_iconWidth->value(), m_iconHeight->value()};
    m_settings->set<Settings::Filters::FilterIconSize>(iconSize);
}

void FiltersGuiPageWidget::reset()
{
    m_settings->reset<Settings::Filters::FilterHeader>();
    m_settings->reset<Settings::Filters::FilterScrollBar>();
    m_settings->reset<Settings::Filters::FilterAltColours>();
    m_settings->reset<Settings::Filters::FilterRowHeight>();
    m_settings->reset<Settings::Filters::FilterIconSize>();
}

FiltersGuiPage::FiltersGuiPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::FiltersAppearance);
    setName(tr("Appearance"));
    setCategory({tr("Widgets"), tr("Filters")});
    setWidgetCreator([settings] { return new FiltersGuiPageWidget(settings); });
}
} // namespace Fooyin::Filters

#include "filtersguipage.moc"
#include "moc_filtersguipage.cpp"
