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

#include "fileopsplugin.h"

#include "fileopsdefs.h"
#include "fileopsdeletedialog.h"
#include "fileopsdialog.h"
#include "fileopssettings.h"

#include <core/engine/audioloader.h>
#include <gui/guiconstants.h>
#include <gui/plugins/guiplugincontext.h>
#include <gui/statusevent.h>
#include <gui/trackselectioncontroller.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/advancedsettingsregistry.h>
#include <utils/utils.h>

#include "fileopsworker.h"

#include <QMainWindow>
#include <QMenu>
#include <QThread>

using namespace Qt::StringLiterals;

namespace Fooyin::FileOps {
namespace {
bool canOperateOnTracks(const TrackList& tracks)
{
    return !tracks.empty() && std::ranges::all_of(tracks, [](const Track& track) { return !track.isInArchive(); });
}

bool canExtractTracks(const TrackList& tracks)
{
    return !tracks.empty() && std::ranges::all_of(tracks, [](const Track& track) { return track.isInArchive(); });
}

Id presetActionId(const QString& presetName)
{
    return Id{u"FileOps.Preset.%1"_s.arg(presetName)};
}

bool canUsePreset(Operation operation, const TrackList& tracks)
{
    return operation == Operation::Extract ? canExtractTracks(tracks) : canOperateOnTracks(tracks);
}
} // namespace

FileOpsPlugin::FileOpsPlugin()
    : m_actionManager{nullptr}
    , m_audioLoader{nullptr}
    , m_library{nullptr}
    , m_libraryManager{nullptr}
    , m_trackSelectionController{nullptr}
    , m_settings{nullptr}
{ }

void FileOpsPlugin::initialise(const CorePluginContext& context)
{
    m_audioLoader    = context.audioLoader;
    m_library        = context.library;
    m_libraryManager = context.libraryManager;
    m_settings       = context.settingsManager;
}

void FileOpsPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager            = context.actionManager;
    m_trackSelectionController = context.trackSelection;

    context.advancedSettingsRegistry->add(
        {.id           = QString::fromLatin1(Settings::ConfirmDelete),
         .category     = {tr("File Operations")},
         .label        = tr("Confirm before deleting tracks"),
         .description  = {},
         .defaultValue = true,
         .editor       = AdvancedSettingCheckBox{},
         .read         = [this] { return m_settings->fileValue(Settings::ConfirmDelete, true).toBool(); },
         .write
         = [this](const QVariant& value) { return m_settings->fileSet(Settings::ConfirmDelete, value.toBool()); },
         .normalise = {},
         .validate  = {}});

    context.advancedSettingsRegistry->add(
        {.id           = QString::fromLatin1(Settings::ImmediateDelete),
         .category     = {tr("File Operations")},
         .label        = tr("Immediately delete tracks (bypass trash)"),
         .description  = {},
         .defaultValue = false,
         .editor       = AdvancedSettingCheckBox{},
         .read         = [this] { return m_settings->fileValue(Settings::ImmediateDelete, false).toBool(); },
         .write
         = [this](const QVariant& value) { return m_settings->fileSet(Settings::ImmediateDelete, value.toBool()); },
         .normalise = {},
         .validate  = {}});

    context.advancedSettingsRegistry->add(
        {.id           = QString::fromLatin1(Settings::RemoveEmptyParentFolders),
         .category     = {tr("File Operations")},
         .label        = tr("Remove empty parent folders"),
         .description  = tr("Remove empty parent folders after moving or deleting files, stopping at the library root"),
         .defaultValue = false,
         .editor       = AdvancedSettingCheckBox{},
         .read         = [this] { return m_settings->fileValue(Settings::RemoveEmptyParentFolders, false).toBool(); },
         .write =
             [this](const QVariant& value) {
                 return m_settings->fileSet(Settings::RemoveEmptyParentFolders, value.toBool());
             },
         .normalise = {},
         .validate  = {}});

    setupMenu();
}

void FileOpsPlugin::setupMenu()
{
    m_trackSelectionController->registerTrackContextSubmenu(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::TrackSelection, "FileOperations",
        tr("File operations"), Constants::Menus::Context::Utilities);

    const auto registerOpEntry = [this](Operation op, const Id& id, const QString& title, const auto& canUseTracks) {
        m_trackSelectionController->registerTrackContextAction(
            this, TrackContextMenuArea::Track, "FileOperations", id, title,
            [this, op, title, canUseTracks](QMenu* menu, const TrackSelection& selection) {
                if(!canUseTracks(selection.tracks)) {
                    return;
                }

                refreshPresetActions();

                const auto presets = getMappedPresets();
                if(!presets.contains(op) || presets.at(op).empty()) {
                    auto* action = new QAction(title, menu);
                    QObject::connect(action, &QAction::triggered, action,
                                     [this, op, selection]() { openDialog(selection, op); });
                    menu->addAction(action);
                    return;
                }

                auto* submenu = new QMenu(title, menu);
                for(const auto& preset : presets.at(op)) {
                    const auto presetAction
                        = std::ranges::find(m_presetActions, preset.name, &PresetAction::presetName);
                    if(presetAction != m_presetActions.cend()) {
                        submenu->addAction(presetAction->command->action());
                    }
                }

                submenu->addSeparator();

                auto* action = new QAction(u"…"_s, submenu);
                QObject::connect(action, &QAction::triggered, action,
                                 [this, op, selection]() { openDialog(selection, op); });
                submenu->addAction(action);
                menu->addMenu(submenu);
            });
    };

    registerOpEntry(Operation::Copy, "FileOps.Copy", tr("&Copy to…"), canOperateOnTracks);
    registerOpEntry(Operation::Move, "FileOps.Move", tr("&Move to…"), canOperateOnTracks);
    registerOpEntry(Operation::Rename, "FileOps.Rename", tr("&Rename to…"), canOperateOnTracks);
    registerOpEntry(Operation::Extract, "FileOps.Extract", tr("&Extract to…"), canExtractTracks);

    refreshPresetActions();

    auto* deleteAction = new QAction(tr("&Delete"), this);
    auto* deleteCmd
        = m_actionManager->registerAction(deleteAction, "FileOps.Delete", Context{Constants::Context::TrackSelection});
    deleteCmd->setCategories({tr("Tracks"), tr("File operations")});

    QObject::connect(deleteAction, &QAction::triggered, deleteAction, [this]() {
        const auto* selection = m_trackSelectionController->selectedSelection();
        if(!selection || !canOperateOnTracks(selection->tracks)) {
            return;
        }

        const auto runDelete = [this, tracks = selection->tracks]() {
            auto* worker = new FileOpsWorker(m_library, m_audioLoader, tracks, m_settings);
            auto* thread = new QThread(this);
            worker->moveToThread(thread);

            QObject::connect(worker, &FileOpsWorker::deleteFinished, this, [](const TrackList& deletedTracks) {
                const QString status = deletedTracks.empty()
                                         ? tr("No tracks deleted")
                                         : tr("Deleted %Ln track(s)", nullptr, static_cast<int>(deletedTracks.size()));
                StatusEvent::post(status);
            });
            QObject::connect(worker, &Worker::finished, thread, &QThread::quit);
            QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

            thread->start();
            QMetaObject::invokeMethod(worker, &FileOpsWorker::deleteFiles);
        };

        const bool confirm = m_settings->fileValue(Settings::ConfirmDelete, true).toBool();
        if(confirm) {
            auto* dialog = new FileOpsDeleteDialog(selection->tracks, m_settings, Utils::getMainWindow());
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            QObject::connect(dialog, &QDialog::accepted, dialog, runDelete);
            dialog->open();
        }
        else {
            runDelete();
        }
    });

    m_trackSelectionController->registerTrackContextAction(
        this, TrackContextMenuArea::Track, "FileOperations", "FileOps.Delete", deleteAction->text(),
        [deleteAction](QMenu* menu, const TrackSelection& selection) {
            if(!canOperateOnTracks(selection.tracks)) {
                return;
            }

            deleteAction->setEnabled(true);
            menu->addAction(deleteAction);
        });
}

void FileOpsPlugin::openDialog(const TrackSelection& selection, Operation operation, const QString& presetName)
{
    auto* dialog = new FileOpsDialog(m_library, m_audioLoader, selection.tracks, operation, m_settings,
                                     m_libraryManager, Utils::getMainWindow());
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->loadPreset(presetName);
    QObject::connect(dialog, &FileOpsDialog::presetsChanged, this, &FileOpsPlugin::refreshPresetActions);
    dialog->open();
}

void FileOpsPlugin::refreshPresetActions()
{
    const auto presets                = getPresets();
    const auto categoriesForOperation = [](Operation operation) {
        QString operationCategory;
        switch(operation) {
            case Operation::Copy:
                operationCategory = tr("Copy");
                break;
            case Operation::Move:
                operationCategory = tr("Move");
                break;
            case Operation::Rename:
                operationCategory = tr("Rename");
                break;
            case Operation::Extract:
                operationCategory = tr("Extract");
                break;
            default:
                operationCategory = tr("Other");
                break;
        }
        return QStringList{tr("Tracks"), tr("File operations"), operationCategory};
    };

    std::erase_if(m_presetActions, [this, &presets](const PresetAction& presetAction) {
        const bool removed = std::ranges::none_of(
            presets, [&presetAction](const FileOpPreset& preset) { return preset.name == presetAction.presetName; });
        if(removed && presetAction.action) {
            m_actionManager->unregisterAction(presetAction.action, presetActionId(presetAction.presetName));
            presetAction.action->deleteLater();
        }
        return removed;
    });

    for(const FileOpPreset& preset : presets) {
        if(preset.name.isEmpty()) {
            continue;
        }

        const auto existing = std::ranges::find(m_presetActions, preset.name, &PresetAction::presetName);
        if(existing != m_presetActions.end()) {
            existing->action->setText(preset.name);
            existing->command->setCategories(categoriesForOperation(preset.op));
            continue;
        }

        auto* action  = new QAction(preset.name, this);
        auto* command = m_actionManager->registerAction(action, presetActionId(preset.name));
        command->setCategories(categoriesForOperation(preset.op));
        command->setAttribute(ProxyAction::UpdateText);
        command->action()->setShortcutVisibleInContextMenu(true);

        QObject::connect(action, &QAction::triggered, this, [this, presetName = preset.name]() {
            const auto currentPresets = getPresets();
            const auto current        = std::ranges::find(currentPresets, presetName, &FileOpPreset::name);
            const auto* selection     = m_trackSelectionController->selectedSelection();
            if(!selection) {
                selection = m_trackSelectionController->displaySelection();
            }
            if(current != currentPresets.cend() && selection && canUsePreset(current->op, selection->tracks)) {
                openDialog(*selection, current->op, current->name);
            }
        });

        m_presetActions.push_back({.presetName = preset.name, .action = action, .command = command});
    }
}
} // namespace Fooyin::FileOps
