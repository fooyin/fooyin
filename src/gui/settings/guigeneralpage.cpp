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

#include "gui/editablelayout.h"
#include "gui/guiconstants.h"
#include "gui/guisettings.h"
#include "gui/quicksetup/quicksetupdialog.h"

#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class GuiGeneralPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit GuiGeneralPageWidget(LayoutProvider* layoutProvider, Widgets::EditableLayout* editableLayout,
                                  Utils::SettingsManager* settings);

    void apply() override;

private:
    void showQuickSetup();

    LayoutProvider* m_layoutProvider;
    Widgets::EditableLayout* m_editableLayout;
    Utils::SettingsManager* m_settings;

    QCheckBox* m_splitterHandles;
};

GuiGeneralPageWidget::GuiGeneralPageWidget(LayoutProvider* layoutProvider, Widgets::EditableLayout* editableLayout,
                                           Utils::SettingsManager* settings)
    : m_layoutProvider{layoutProvider}
    , m_editableLayout{editableLayout}
    , m_settings{settings}
    , m_splitterHandles{new QCheckBox("Show Splitter Handles", this)}
{
    m_splitterHandles->setChecked(m_settings->value<Settings::SplitterHandles>());

    auto* splitterBox       = new QGroupBox(tr("Splitters"));
    auto* splitterBoxLayout = new QVBoxLayout(splitterBox);
    splitterBoxLayout->addWidget(m_splitterHandles);

    auto* setupBox       = new QGroupBox(tr("Setup"));
    auto* setupBoxLayout = new QHBoxLayout(setupBox);
    auto* quickSetup     = new QPushButton("Quick Setup", this);
    auto* importLayout   = new QPushButton("Import Layout", this);
    auto* exportLayout   = new QPushButton("Export Layout", this);

    setupBoxLayout->addWidget(quickSetup);
    setupBoxLayout->addWidget(importLayout);
    setupBoxLayout->addWidget(exportLayout);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(setupBox);
    mainLayout->addWidget(splitterBox);
    mainLayout->addStretch();

    QObject::connect(quickSetup, &QPushButton::clicked, this, &GuiGeneralPageWidget::showQuickSetup);
    QObject::connect(importLayout, &QPushButton::clicked, this, [this]() {
        m_layoutProvider->importLayout();
    });
    QObject::connect(exportLayout, &QPushButton::clicked, this, [this]() {
        m_layoutProvider->exportLayout(m_editableLayout->currentLayout());
    });
}

void GuiGeneralPageWidget::apply()
{
    m_settings->set<Settings::SplitterHandles>(m_splitterHandles->isChecked());
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
    setCategory("Category.Interface");
    setCategoryName(tr("Interface"));
    setWidgetCreator([layoutProvider, editableLayout, settings] {
        return new GuiGeneralPageWidget(layoutProvider, editableLayout, settings);
    });
    setCategoryIconPath(Constants::Icons::Category::Interface);
}
} // namespace Fy::Gui::Settings
