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

#include "quicksetup/quicksetupdialog.h"

#include <gui/editablelayout.h>
#include <gui/guiconstants.h>
#include <gui/guipaths.h>
#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin {
class GuiGeneralPageWidget : public SettingsPageWidget
{
public:
    explicit GuiGeneralPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                  SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void showQuickSetup();
    void importLayout();
    void exportLayout();

    LayoutProvider* m_layoutProvider;
    EditableLayout* m_editableLayout;
    SettingsManager* m_settings;

    QRadioButton* m_lightTheme;
    QRadioButton* m_darkTheme;
    QRadioButton* m_customTheme;
    QLineEdit* m_customThemeName;
    QCheckBox* m_splitterHandles;
};

GuiGeneralPageWidget::GuiGeneralPageWidget(LayoutProvider* layoutProvider, EditableLayout* editableLayout,
                                           SettingsManager* settings)
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

    auto* setupBox        = new QGroupBox(tr("Setup"));
    auto* setupBoxLayout  = new QHBoxLayout(setupBox);
    auto* quickSetup      = new QPushButton(tr("Quick Setup"), this);
    auto* importLayoutBtn = new QPushButton(tr("Import Layout"), this);
    auto* exportLayoutBtn = new QPushButton(tr("Export Layout"), this);

    auto* iconThemeBox       = new QGroupBox(tr("Icon Theme"), this);
    auto* iconThemeBoxLayout = new QVBoxLayout(iconThemeBox);
    iconThemeBoxLayout->addWidget(m_lightTheme);
    iconThemeBoxLayout->addWidget(m_darkTheme);
    iconThemeBoxLayout->addWidget(m_customTheme);
    iconThemeBoxLayout->addWidget(m_customThemeName);

    setupBoxLayout->addWidget(quickSetup);
    setupBoxLayout->addWidget(importLayoutBtn);
    setupBoxLayout->addWidget(exportLayoutBtn);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(setupBox);
    mainLayout->addWidget(splitterBox);
    mainLayout->addWidget(iconThemeBox);
    mainLayout->addStretch();

    m_splitterHandles->setChecked(m_settings->value<Settings::Gui::SplitterHandles>());

    const QString currentTheme = m_settings->value<Settings::Gui::IconTheme>();
    if(currentTheme == "light"_L1) {
        m_lightTheme->setChecked(true);
    }
    else if(currentTheme == "dark"_L1) {
        m_darkTheme->setChecked(true);
    }
    else {
        m_customTheme->setChecked(true);
        m_customThemeName->setVisible(true);
    }

    QObject::connect(quickSetup, &QPushButton::clicked, this, &GuiGeneralPageWidget::showQuickSetup);
    QObject::connect(importLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::importLayout);
    QObject::connect(exportLayoutBtn, &QPushButton::clicked, this, &GuiGeneralPageWidget::exportLayout);

    QObject::connect(m_lightTheme, &QRadioButton::clicked, this, [this]() { m_customThemeName->setVisible(false); });
    QObject::connect(m_darkTheme, &QRadioButton::clicked, this, [this]() { m_customThemeName->setVisible(false); });
    QObject::connect(m_customTheme, &QRadioButton::clicked, this, [this]() { m_customThemeName->setVisible(true); });
}

void GuiGeneralPageWidget::apply()
{
    const QString theme = m_customTheme->isChecked() ? m_customThemeName->text()
                        : m_lightTheme->isChecked()  ? u"light"_s
                                                     : u"dark"_s;

    if(theme != m_settings->value<Settings::Gui::IconTheme>()) {
        QIcon::setThemeName(theme);
        m_settings->set<Settings::Gui::IconTheme>(theme);
    }

    m_settings->set<Settings::Gui::SplitterHandles>(m_splitterHandles->isChecked());
}

void GuiGeneralPageWidget::reset()
{
    m_settings->reset<Settings::Gui::IconTheme>();
    m_settings->reset<Settings::Gui::SplitterHandles>();

    m_splitterHandles->setChecked(m_settings->value<Settings::Gui::SplitterHandles>());
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
    const QString layoutFile = QFileDialog::getOpenFileName(this, u"Open Layout"_s, u""_s, u"Fooyin Layout (*.fyl)"_s);

    if(layoutFile.isEmpty()) {
        return;
    }

    m_layoutProvider->importLayout(layoutFile);
}

void GuiGeneralPageWidget::exportLayout()
{
    bool success{false};
    const QString name
        = QInputDialog::getText(this, tr("Export layout"), tr("Layout Name:"), QLineEdit::Normal, u""_s, &success);

    if(success && !name.isEmpty()) {
        const QString saveFile = QFileDialog::getSaveFileName(this, u"Save Layout"_s, Gui::layoutsPath() + name,
                                                              u"Fooyin Layout (*.fyl)"_s);
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
