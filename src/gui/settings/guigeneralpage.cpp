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

#include "guigeneralpage.h"

#include "editablelayout.h"
#include "quicksetup/quicksetupdialog.h"

#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class GuiGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit GuiGeneralPageWidget(LayoutProvider* layoutProvider, Widgets::EditableLayout* editableLayout,
                                  Utils::SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void showQuickSetup();

    LayoutProvider* m_layoutProvider;
    Widgets::EditableLayout* m_editableLayout;
    Utils::SettingsManager* m_settings;

    QRadioButton* m_lightTheme;
    QRadioButton* m_darkTheme;
    QRadioButton* m_customTheme;
    QLineEdit* m_customThemeName;
    QCheckBox* m_splitterHandles;
};

GuiGeneralPageWidget::GuiGeneralPageWidget(LayoutProvider* layoutProvider, Widgets::EditableLayout* editableLayout,
                                           Utils::SettingsManager* settings)
    : m_layoutProvider{layoutProvider}
    , m_editableLayout{editableLayout}
    , m_settings{settings}
    , m_lightTheme{new QRadioButton(tr("Light"), this)}
    , m_darkTheme{new QRadioButton(tr("Dark"), this)}
    , m_customTheme{new QRadioButton(tr("Custom"), this)}
    , m_customThemeName{new QLineEdit(this)}
    , m_splitterHandles{new QCheckBox(tr("Show Splitter Handles"), this)}
{
    m_customThemeName->setVisible(false);

    auto* splitterBox       = new QGroupBox(tr("Splitters"));
    auto* splitterBoxLayout = new QGridLayout(splitterBox);
    splitterBoxLayout->addWidget(m_splitterHandles, 0, 0, 1, 2);
    splitterBoxLayout->setColumnStretch(2, 1);

    auto* setupBox       = new QGroupBox(tr("Setup"));
    auto* setupBoxLayout = new QHBoxLayout(setupBox);
    auto* quickSetup     = new QPushButton(tr("Quick Setup"), this);
    auto* importLayout   = new QPushButton(tr("Import Layout"), this);
    auto* exportLayout   = new QPushButton(tr("Export Layout"), this);

    auto* iconThemeBox       = new QGroupBox(tr("Icon Theme"), this);
    auto* iconThemeBoxLayout = new QVBoxLayout(iconThemeBox);
    iconThemeBoxLayout->addWidget(m_lightTheme);
    iconThemeBoxLayout->addWidget(m_darkTheme);
    iconThemeBoxLayout->addWidget(m_customTheme);
    iconThemeBoxLayout->addWidget(m_customThemeName);

    setupBoxLayout->addWidget(quickSetup);
    setupBoxLayout->addWidget(importLayout);
    setupBoxLayout->addWidget(exportLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(setupBox);
    mainLayout->addWidget(splitterBox);
    mainLayout->addWidget(iconThemeBox);
    mainLayout->addStretch();

    m_splitterHandles->setChecked(m_settings->value<Settings::SplitterHandles>());

    const QString currentTheme = m_settings->value<Settings::IconTheme>();
    if(currentTheme == "light") {
        m_lightTheme->setChecked(true);
    }
    else if(currentTheme == "dark") {
        m_darkTheme->setChecked(true);
    }
    else {
        m_customTheme->setChecked(true);
        m_customThemeName->setVisible(true);
    }

    QObject::connect(quickSetup, &QPushButton::clicked, this, &GuiGeneralPageWidget::showQuickSetup);
    QObject::connect(importLayout, &QPushButton::clicked, this, [this]() {
        m_layoutProvider->importLayout();
    });
    QObject::connect(exportLayout, &QPushButton::clicked, this, [this]() {
        m_layoutProvider->exportLayout(m_editableLayout->currentLayout());
    });

    QObject::connect(m_lightTheme, &QRadioButton::clicked, this, [this]() {
        m_customThemeName->setVisible(false);
    });
    QObject::connect(m_darkTheme, &QRadioButton::clicked, this, [this]() {
        m_customThemeName->setVisible(false);
    });
    QObject::connect(m_customTheme, &QRadioButton::clicked, this, [this]() {
        m_customThemeName->setVisible(true);
    });
}

void GuiGeneralPageWidget::apply()
{
    const QString theme = m_customTheme->isChecked() ? m_customThemeName->text()
                        : m_lightTheme->isChecked()  ? "light"
                                                     : "dark";

    if(theme != m_settings->value<Settings::IconTheme>()) {
        QIcon::setThemeName(theme);
        m_settings->set<Settings::IconTheme>(theme);
    }

    m_settings->set<Settings::SplitterHandles>(m_splitterHandles->isChecked());
}

void GuiGeneralPageWidget::reset()
{
    m_settings->reset<Settings::IconTheme>();
    m_settings->reset<Settings::SplitterHandles>();

    m_splitterHandles->setChecked(m_settings->value<Settings::SplitterHandles>());
}

void GuiGeneralPageWidget::showQuickSetup()
{
    auto* quickSetup = new QuickSetupDialog(m_layoutProvider, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
    connect(quickSetup, &QuickSetupDialog::layoutChanged, m_editableLayout, &Widgets::EditableLayout::changeLayout);
    quickSetup->show();
}

GuiGeneralPage::GuiGeneralPage(LayoutProvider* layoutProvider, Widgets::EditableLayout* editableLayout,
                               Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::InterfaceGeneral);
    setName(tr("General"));
    setCategory({"Interface"});
    setWidgetCreator([layoutProvider, editableLayout, settings] {
        return new GuiGeneralPageWidget(layoutProvider, editableLayout, settings);
    });
}
} // namespace Fy::Gui::Settings
