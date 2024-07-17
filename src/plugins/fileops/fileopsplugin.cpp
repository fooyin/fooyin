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

#include "fileopsplugin.h"

#include "fileopsdefs.h"
#include "fileopsdialog.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QMainWindow>
#include <QMenu>

namespace Fooyin::FileOps {
void FileOpsPlugin::initialise(const CorePluginContext& context)
{
    m_library  = context.library;
    m_settings = context.settingsManager;
}

void FileOpsPlugin::initialise(const GuiPluginContext& context)
{
    auto* selectionMenu = context.actionManager->actionContainer(Constants::Menus::Context::TrackSelection);

    auto* fileOpsMenu = context.actionManager->createMenu("FileOperations");
    fileOpsMenu->menu()->setTitle(tr("&File Operations"));
    selectionMenu->addMenu(Constants::Menus::Context::Utilities, fileOpsMenu);

    auto* copyAction   = new QAction(tr("&Copy to…"), selectionMenu);
    auto* moveAction   = new QAction(tr("&Move to…"), selectionMenu);
    auto* renameAction = new QAction(tr("&Rename to…"), selectionMenu);

    auto openDialog = [this, context](Operation op) {
        auto* dialog = new FileOpsDialog(m_library, context.trackSelection->selectedTracks(), op, m_settings,
                                         Utils::getMainWindow());
        dialog->show();
    };

    QObject::connect(copyAction, &QAction::triggered, this, [openDialog]() { openDialog(Operation::Copy); });
    QObject::connect(moveAction, &QAction::triggered, this, [openDialog]() { openDialog(Operation::Move); });
    QObject::connect(renameAction, &QAction::triggered, this, [openDialog]() { openDialog(Operation::Rename); });

    fileOpsMenu->addAction(copyAction);
    fileOpsMenu->addAction(moveAction);
    fileOpsMenu->addAction(renameAction);
}
} // namespace Fooyin::FileOps
