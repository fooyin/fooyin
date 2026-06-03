/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "guidisplaypage.h"

#include "internalguisettings.h"

#include <core/internalcoresettings.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QRadioButton>
#include <QStyleFactory>

using namespace Qt::StringLiterals;

namespace Fooyin {
using namespace Settings::Gui;
using namespace Settings::Gui::Internal;

class GuiDisplayPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiDisplayPageWidget(SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    SettingsManager* m_settings;

    QComboBox* m_styles;

    QRadioButton* m_detectIconTheme;
    QRadioButton* m_lightTheme;
    QRadioButton* m_darkTheme;
    QRadioButton* m_systemTheme;
};

GuiDisplayPageWidget::GuiDisplayPageWidget(SettingsManager* settings)
    : m_settings{settings}
    , m_styles{new QComboBox(this)}
    , m_detectIconTheme{new QRadioButton(tr("Auto-detect theme"), this)}
    , m_lightTheme{new QRadioButton(tr("Light"), this)}
    , m_darkTheme{new QRadioButton(tr("Dark"), this)}
    , m_systemTheme{new QRadioButton(tr("Use system icons"), this)}
{
    auto* themeGroup       = new QGroupBox(tr("Theme"), this);
    auto* themeGroupLayout = new QGridLayout(themeGroup);

    auto* iconThemeBox       = new QGroupBox(tr("Icons"), themeGroup);
    auto* iconThemeBoxLayout = new QGridLayout(iconThemeBox);

    int row{0};
    iconThemeBoxLayout->addWidget(m_detectIconTheme, row++, 0, 1, 2);
    iconThemeBoxLayout->addWidget(m_lightTheme, row, 0);
    iconThemeBoxLayout->addWidget(m_darkTheme, row++, 1);
    iconThemeBoxLayout->addWidget(m_systemTheme, row++, 0, 1, 2);
    iconThemeBoxLayout->setColumnStretch(2, 1);

    row = 0;
    themeGroupLayout->addWidget(new QLabel(tr("Style") + ":"_L1, this), row, 0);
    themeGroupLayout->addWidget(m_styles, row++, 1);
    themeGroupLayout->setColumnStretch(1, 1);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(themeGroup, row++, 0, 1, 2);
    mainLayout->addWidget(iconThemeBox, row++, 0, 1, 2);
    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);
}

void GuiDisplayPageWidget::load()
{
    m_styles->clear();
    m_styles->addItem(u"System default"_s);

    const QStringList keys = QStyleFactory::keys();
    for(const QString& key : keys) {
        m_styles->addItem(key);
    }

    const auto style = m_settings->value<Style>();
    if(!style.isEmpty()) {
        m_styles->setCurrentText(style);
    }
    else {
        m_styles->setCurrentIndex(0);
    }

    const auto iconTheme = static_cast<IconThemeOption>(m_settings->value<IconTheme>());
    switch(iconTheme) {
        case(IconThemeOption::AutoDetect):
            m_detectIconTheme->setChecked(true);
            break;
        case(IconThemeOption::System):
            m_systemTheme->setChecked(true);
            break;
        case(IconThemeOption::Light):
            m_lightTheme->setChecked(true);
            break;
        case(IconThemeOption::Dark):
            m_darkTheme->setChecked(true);
            break;
    }
}

void GuiDisplayPageWidget::apply()
{
    m_settings->set<Style>(m_styles->currentText());

    IconThemeOption iconThemeOption;
    if(m_detectIconTheme->isChecked()) {
        iconThemeOption = IconThemeOption::AutoDetect;
    }
    else if(m_lightTheme->isChecked()) {
        iconThemeOption = IconThemeOption::Light;
    }
    else if(m_darkTheme->isChecked()) {
        iconThemeOption = IconThemeOption::Dark;
    }
    else {
        iconThemeOption = IconThemeOption::System;
    }
    m_settings->set<IconTheme>(static_cast<int>(iconThemeOption));
}

void GuiDisplayPageWidget::reset()
{
    m_settings->reset<Style>();
    m_settings->reset<IconTheme>();
}

GuiDisplayPage::GuiDisplayPage(SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceDisplay);
    setName(tr("Display"));
    setCategory({tr("Interface")});
    setWidgetCreator([settings] { return new GuiDisplayPageWidget(settings); });
}
} // namespace Fooyin

#include "guidisplaypage.moc"
#include "moc_guidisplaypage.cpp"
