/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "projectmplugin.h"

#include "projectmwidget.h"

#include <core/engine/enginecontroller.h>
#include <gui/guiconstants.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QAction>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace Fooyin::ProjectM {
void ProjectMPlugin::initialise(const CorePluginContext& context)
{
    m_engine   = context.engine;
    m_settings = context.settingsManager;
}

void ProjectMPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager  = context.actionManager;
    m_widgetProvider = context.widgetProvider;

    auto* viewMenu           = m_actionManager->actionContainer(Constants::Menus::View);
    auto* visualisationsMenu = m_actionManager->createMenu("Fooyin.Menu.View.Visualisations");
    visualisationsMenu->menu()->setTitle(tr("&Visualisations"));
    viewMenu->addMenu(visualisationsMenu);

    auto* showProjectM = new QAction(tr("project&M"), this);
    showProjectM->setStatusTip(tr("Open projectM in a separate window"));
    auto* showProjectMCmd = m_actionManager->registerAction(showProjectM, "ProjectM.ShowWindow");
    showProjectMCmd->setCategories({tr("View"), tr("Visualisations")});
    visualisationsMenu->addAction(showProjectMCmd);
    QObject::connect(showProjectM, &QAction::triggered, this, &ProjectMPlugin::showProjectMWindow);

    m_widgetProvider->registerWidget(
        u"ProjectM"_s, [this]() { return new ProjectMWidget(m_actionManager, m_engine, m_settings); }, tr("projectM"));
    m_widgetProvider->setSubMenus(u"ProjectM"_s, {tr("Visualisations")});
}

void ProjectMPlugin::showProjectMWindow()
{
    auto* window = new ProjectMWidget(m_actionManager, m_engine, m_settings);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setWindowTitle(tr("projectM"));
    window->resize(800, 450);
    window->show();
}
} // namespace Fooyin::ProjectM

#include "moc_projectmplugin.cpp"
