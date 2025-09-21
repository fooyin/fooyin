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
#include "fileopssettings.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QMainWindow>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace Fooyin::FileOps {
FileOpsPlugin::FileOpsPlugin()
    : m_actionManager{nullptr}
    , m_library{nullptr}
    , m_trackSelectionController{nullptr}
    , m_settings{nullptr}
    , m_fileOpsMenu{nullptr}
{ }

void FileOpsPlugin::initialise(const CorePluginContext& context)
{
    m_library  = context.library;
    m_settings = context.settingsManager;
}

void FileOpsPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager            = context.actionManager;
    m_trackSelectionController = context.trackSelection;

    recreateMenu();
}

void FileOpsPlugin::recreateMenu()
{
    auto* selectionMenu = m_actionManager->actionContainer(Constants::Menus::Context::TrackSelection);

    if(!m_fileOpsMenu) {
        m_fileOpsMenu = m_actionManager->createMenu("FileOperations");
        m_fileOpsMenu->menu()->setTitle(tr("&File operations"));
        selectionMenu->addMenu(Constants::Menus::Context::Utilities, m_fileOpsMenu);
    }
    else {
        for(auto* menu : m_opActions) {
            menu->deleteLater();
        }
        m_opActions.clear();
    }

    QObject::connect(m_trackSelectionController, &TrackSelectionController::selectionChanged, m_fileOpsMenu, [this]() {
        const auto tracks = m_trackSelectionController->selectedTracks();
        const bool canOp  = std::ranges::all_of(tracks, [](const Track& track) { return !track.isInArchive(); });
        m_fileOpsMenu->menu()->setEnabled(canOp);
    });

    auto openDialog = [this](Operation op, const QString& presetName = {}) {
        auto* dialog = new FileOpsDialog(m_library, m_trackSelectionController->selectedTracks(), op, m_settings,
                                         Utils::getMainWindow());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dialog, &FileOpsDialog::presetsChanged, this, &FileOpsPlugin::recreateMenu);

        dialog->loadPreset(presetName);
        dialog->show();
    };

    const auto presets = FileOps::getMappedPresets();

    const auto addSubmenuOrAction = [this, &presets, openDialog](Operation op, const QString& title) {
        if(presets.contains(op) && !presets.at(op).empty()) {
            auto* submenu = new QMenu(title, m_fileOpsMenu->menu());
            m_opActions.emplace_back(submenu);

            for(const auto& preset : presets.at(op)) {
                auto* presetAction = new QAction(preset.name, submenu);
                QObject::connect(presetAction, &QAction::triggered, presetAction,
                                 [openDialog, op, preset]() { openDialog(op, preset.name); });
                submenu->addAction(presetAction);
            }

            submenu->addSeparator();

            auto* action = new QAction(u"…"_s, submenu);
            QObject::connect(action, &QAction::triggered, action, [openDialog, op]() { openDialog(op); });
            submenu->addAction(action);
            m_fileOpsMenu->menu()->addMenu(submenu);
        }
        else {
            auto* action = new QAction(title, m_fileOpsMenu);
            m_opActions.emplace_back(action);

            QObject::connect(action, &QAction::triggered, action, [openDialog, op]() { openDialog(op); });
            m_fileOpsMenu->addAction(action);
        }
    };

    addSubmenuOrAction(Operation::Copy, tr("&Copy to…"));
    addSubmenuOrAction(Operation::Move, tr("&Move to…"));
    addSubmenuOrAction(Operation::Rename, tr("&Rename to…"));
}
} // namespace Fooyin::FileOps
