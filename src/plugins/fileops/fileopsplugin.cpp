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

#include "fileopsconfigdialog.h"
#include "fileopsdefs.h"
#include "fileopsdeletedialog.h"
#include "fileopsdialog.h"
#include "fileopssettings.h"

#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/utils.h>

#include "fileopsworker.h"

#include <utils/settings/settingsmanager.h>

#include <QMainWindow>
#include <QMenu>
#include <QThread>

using namespace Qt::StringLiterals;

namespace {
bool canOperateOnTracks(const Fooyin::TrackList& tracks)
{
    return !tracks.empty()
        && std::ranges::all_of(tracks, [](const Fooyin::Track& track) { return !track.isInArchive(); });
}

class FileOpsPluginSettingsProvider : public Fooyin::PluginSettingsProvider
{
public:
    explicit FileOpsPluginSettingsProvider(Fooyin::SettingsManager* settings)
        : m_settings{settings}
    { }

    void showSettings(QWidget* parent) override
    {
        auto* dialog = new Fooyin::FileOps::FileOpsConfigDialog(m_settings, parent);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }

private:
    Fooyin::SettingsManager* m_settings;
};
} // namespace

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

std::unique_ptr<PluginSettingsProvider> FileOpsPlugin::settingsProvider() const
{
    return std::make_unique<FileOpsPluginSettingsProvider>(m_settings);
}

void FileOpsPlugin::recreateMenu()
{
    auto* selectionMenu = m_actionManager->actionContainer(Constants::Menus::Context::TrackSelection);

    if(!m_fileOpsMenu) {
        m_fileOpsMenu = m_actionManager->createMenu("FileOperations");
        m_fileOpsMenu->menu()->setTitle(tr("&File operations"));
        selectionMenu->addMenu(Constants::Menus::Context::Utilities, m_fileOpsMenu);

        QObject::connect(
            m_trackSelectionController, &TrackSelectionController::selectionChanged, m_fileOpsMenu, [this]() {
                m_fileOpsMenu->menu()->setEnabled(canOperateOnTracks(m_trackSelectionController->selectedTracks()));
            });
    }
    else {
        for(auto* menu : m_opActions) {
            menu->deleteLater();
        }
        m_opActions.clear();
    }
    m_fileOpsMenu->menu()->setEnabled(canOperateOnTracks(m_trackSelectionController->selectedTracks()));

    auto openDialog = [this](Operation op, const QString& presetName = {}) {
        auto* dialog = new FileOpsDialog(m_library, m_trackSelectionController->selectedTracks(), op, m_settings,
                                         Utils::getMainWindow());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(dialog, &FileOpsDialog::presetsChanged, this, &FileOpsPlugin::recreateMenu);

        dialog->loadPreset(presetName);
        dialog->open();
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

    auto* deleteAction = new QAction(tr("&Delete"), m_fileOpsMenu);
    m_opActions.emplace_back(deleteAction);

    auto* deleteCmd
        = m_actionManager->registerAction(deleteAction, "FileOps.Delete", Context{Constants::Context::TrackSelection});
    deleteCmd->setCategories({tr("Edit")});

    const auto updateDeleteAction = [this, deleteAction]() {
        deleteAction->setEnabled(canOperateOnTracks(m_trackSelectionController->selectedTracks()));
    };

    QObject::connect(m_trackSelectionController, &TrackSelectionController::selectionChanged, deleteAction,
                     updateDeleteAction);
    updateDeleteAction();

    QObject::connect(deleteAction, &QAction::triggered, deleteAction, [this]() {
        const auto selection = m_trackSelectionController->selectedSelection();
        if(!canOperateOnTracks(selection.tracks)) {
            return;
        }

        const auto runDelete = [this, selection]() {
            auto* worker = new FileOpsWorker(m_library, selection.tracks, m_settings);
            auto* thread = new QThread(this);
            worker->moveToThread(thread);

            QObject::connect(worker, &Worker::finished, thread, &QThread::quit);
            QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

            thread->start();
            QMetaObject::invokeMethod(worker, &FileOpsWorker::deleteFiles);
        };

        const bool confirm = m_settings->fileValue(Settings::ConfirmDelete, true).toBool();
        if(confirm) {
            auto* dialog = new FileOpsDeleteDialog(selection.tracks, m_settings, Utils::getMainWindow());
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dialog, &QDialog::accepted, dialog, [runDelete]() { runDelete(); });
            dialog->open();
        }
        else {
            runDelete();
        }
    });
    m_fileOpsMenu->addAction(deleteAction);
}
} // namespace Fooyin::FileOps
