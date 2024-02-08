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

#include "guigeneralpage.h"

#include "guipaths.h"
#include "internalguisettings.h"
#include "quicksetup/quicksetupdialog.h"

#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace Fooyin {
using namespace Settings::Gui;
using namespace Settings::Gui::Internal;

class GuiGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit GuiGeneralPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                  SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void showQuickSetup();
    void importLayout();
    void exportLayout();

    LayoutProvider* m_layoutProvider;
    EditableLayout* m_editableLayout;
    SettingsManager* m_settings;

    QRadioButton* m_detectIconTheme;
    QRadioButton* m_lightTheme;
    QRadioButton* m_darkTheme;
    QRadioButton* m_systemTheme;
    QCheckBox* m_splitterHandles;
};

GuiGeneralPageWidget::GuiGeneralPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                           SettingsManager* settings)
    : m_layoutProvider{layoutProvider}
    , m_editableLayout{editableLayout}
    , m_settings{settings}
    , m_detectIconTheme{new QRadioButton(tr("Auto-detect theme"), this)}
    , m_lightTheme{new QRadioButton(tr("Light"), this)}
    , m_darkTheme{new QRadioButton(tr("Dark"), this)}
    , m_systemTheme{new QRadioButton(tr("Use system icons"), this)}
    , m_splitterHandles{new QCheckBox(tr("Show Splitter Handles"), this)}
{
    auto* splitterBox       = new QGroupBox(tr("Splitters"));
    auto* splitterBoxLayout = new QGridLayout(splitterBox);
    splitterBoxLayout->addWidget(m_splitterHandles, 0, 0, 1, 2);
    splitterBoxLayout->setColumnStretch(2, 1);

    auto* setupBox        = new QGroupBox(tr("Setup"));
    auto* setupBoxLayout  = new QHBoxLayout(setupBox);
    auto* quickSetup      = new QPushButton(tr("Quick Setup"), this);
    auto* importLayoutBtn = new QPushButton(tr("Import Layout"), this);
    auto* exportLayoutBtn = new QPushButton(tr("Export Layout"), this);

    auto* iconThemeBox       = new QGroupBox(tr("Icon Theme"), this);
    auto* iconThemeBoxLayout = new QGridLayout(iconThemeBox);
    iconThemeBoxLayout->addWidget(m_detectIconTheme, 0, 0, 1, 2);
    iconThemeBoxLayout->addWidget(m_lightTheme, 1, 0);
    iconThemeBoxLayout->addWidget(m_darkTheme, 1, 1);
    iconThemeBoxLayout->addWidget(m_systemTheme, 2, 0, 1, 2);

    iconThemeBoxLayout->setColumnStretch(2, 1);

    setupBoxLayout->addWidget(quickSetup);
    setupBoxLayout->addWidget(importLayoutBtn);
    setupBoxLayout->addWidget(exportLayoutBtn);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(setupBox, 0, 0, 1, 2);
    mainLayout->addWidget(splitterBox, 1, 0, 1, 2);
    mainLayout->addWidget(iconThemeBox, 2, 0, 1, 2);

    mainLayout->setColumnStretch(1, 1);
    mainLayout->setRowStretch(mainLayout->rowCount(), 1);

    QObject::connect(quickSetup, &QPushButton::clicked, this, &GuiGeneralPageWidget::showQuickSetup);
    QObject::connect(importLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::importLayout);
    QObject::connect(exportLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::exportLayout);
}

void GuiGeneralPageWidget::load()
{
    m_splitterHandles->setChecked(m_settings->value<SplitterHandles>());

    const auto iconTheme = static_cast<Settings::Gui::Internal::IconThemeOption>(m_settings->value<IconTheme>());
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

void GuiGeneralPageWidget::apply()
{
    IconThemeOption iconThemeOption;

    if(m_detectIconTheme->isChecked()) {
        iconThemeOption = IconThemeOption::AutoDetect;
        QIcon::setThemeName(Utils::isDarkMode() ? Constants::DarkIconTheme : Constants::LightIconTheme);
    }
    else if(m_lightTheme->isChecked()) {
        iconThemeOption = IconThemeOption::Light;
        QIcon::setThemeName(Constants::LightIconTheme);
    }
    else if(m_darkTheme->isChecked()) {
        iconThemeOption = IconThemeOption::Dark;
        QIcon::setThemeName(Constants::DarkIconTheme);
    }
    else {
        iconThemeOption = IconThemeOption::System;
        QIcon::setThemeName(m_settings->value<SystemIconTheme>());
    }

    m_settings->set<IconTheme>(static_cast<int>(iconThemeOption));
    m_settings->set<SplitterHandles>(m_splitterHandles->isChecked());
}

void GuiGeneralPageWidget::reset()
{
    m_settings->reset<IconTheme>();
    m_settings->reset<SplitterHandles>();
}

void GuiGeneralPageWidget::showQuickSetup()
{
    auto* quickSetup = new QuickSetupDialog(m_layoutProvider, this);
    quickSetup->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(quickSetup, &QuickSetupDialog::layoutChanged, m_editableLayout, &EditableLayout::changeLayout);
    quickSetup->show();
}

void GuiGeneralPageWidget::importLayout()
{
    const QString layoutFile = QFileDialog::getOpenFileName(this, QStringLiteral("Open Layout"), QStringLiteral(""),
                                                            QStringLiteral("Fooyin Layout (*.fyl)"));

    if(layoutFile.isEmpty()) {
        return;
    }

    if(const auto layout = m_layoutProvider->importLayout(layoutFile)) {
        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText("Replace existing layout?");
        message.setInformativeText(tr("Unless exported, the current layout will be lost."));

        message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        message.setDefaultButton(QMessageBox::No);

        const int buttonClicked = message.exec();

        if(buttonClicked == QMessageBox::Yes) {
            m_editableLayout->changeLayout(layout.value());
        }
    }
}

void GuiGeneralPageWidget::exportLayout()
{
    bool success{false};
    const QString name = QInputDialog::getText(this, tr("Export layout"), tr("Layout Name") + ":", QLineEdit::Normal,
                                               QStringLiteral(""), &success);

    if(success && !name.isEmpty()) {
        const QString saveFile = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save Layout"), Gui::layoutsPath() + name, QStringLiteral("Fooyin Layout (*.fyl)"));
        if(!saveFile.isEmpty()) {
            Layout layout = m_layoutProvider->currentLayout();
            layout.name   = name;
            m_layoutProvider->exportLayout(layout, saveFile);
        }
    }
}

GuiGeneralPage::GuiGeneralPage(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                               SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::InterfaceGeneral);
    setName(tr("General"));
    setCategory({"Interface"});
    setWidgetCreator([layoutProvider, editableLayout, settings] {
        return new GuiGeneralPageWidget(layoutProvider, editableLayout, settings);
    });
}
} // namespace Fooyin

#include "guigeneralpage.moc"
#include "moc_guigeneralpage.cpp"
