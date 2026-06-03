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

#include "rgscannerplugin.h"

#include "opusheadergaindialog.h"
#include "opusreplaygainutils.h"
#include "rgscanner.h"
#include "rgscannerpage.h"
#include "rgscanresults.h"

#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/actions/actionmanager.h>
#include <utils/utils.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::RGScanner {
void RGScannerPlugin::initialise(const CorePluginContext& context)
{
    m_audioLoader = context.audioLoader;
    m_library     = context.library;
    m_settings    = context.settingsManager;
}

void RGScannerPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager       = context.actionManager;
    m_selectionController = context.trackSelection;

    new RGScannerPage(m_settings, this);
    setupReplayGainMenu();
}

void RGScannerPlugin::calculateReplayGain(RGScanType type)
{
    const auto tracksToScan = m_selectionController->selectedTracks();
    if(tracksToScan.empty()) {
        return;
    }

    const auto total = static_cast<int>(tracksToScan.size());
    auto* progress
        = new ElapsedProgressDialog(tr("Scanning tracks…"), tr("Abort"), 0, total + 1, Utils::getMainWindow());
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setValue(0);
    progress->setWindowTitle(tr("ReplayGain Scan Progress"));

    auto* scanner = new RGScanner(m_audioLoader, this);
    QObject::connect(scanner, &RGScanner::calculationFinished, this,
                     [this, scanner, progress, tracksToScan, type](const TrackList& tracks) {
                         const auto finishTime = progress->elapsedTime();
                         scanner->close();
                         progress->deleteLater();

                         auto* rgResults = new RGScanResults(m_library, m_settings, tracksToScan, tracks, type,
                                                             finishTime, Utils::getMainWindow());
                         rgResults->setAttribute(Qt::WA_DeleteOnClose);
                         rgResults->show();
                     });

    QObject::connect(scanner, &RGScanner::startingCalculation, progress, [scanner, progress](const QString& filepath) {
        if(progress->wasCancelled()) {
            scanner->close();
            progress->deleteLater();
            return;
        }
        progress->setValue(progress->value() + 1);
        progress->setText(tr("Current file") + ":\n"_L1 + filepath);
    });

    switch(type) {
        case RGScanType::Track:
            scanner->calculatePerTrack(tracksToScan);
            break;
        case RGScanType::SingleAlbum:
            scanner->calculateAsAlbum(tracksToScan);
            break;
        case RGScanType::Album:
            scanner->calculateByAlbumTags(tracksToScan);
            break;
    }

    progress->startTimer();
}

void RGScannerPlugin::setupReplayGainMenu()
{
    m_selectionController->registerTrackContextSubmenu(
        this, TrackContextMenuArea::Track, Constants::Menus::Context::TrackSelection,
        Constants::Menus::Context::ReplayGain, tr("ReplayGain"), Constants::Menus::Context::TrackFinalSeparator);

    auto* window = Utils::getMainWindow();

    auto* rgTrackAction        = new QAction(tr("Calculate ReplayGain values per-file"), window);
    auto* rgSingleAlbumAction  = new QAction(tr("Calculate ReplayGain values as a single album"), window);
    auto* rgAlbumAction        = new QAction(tr("Calculate ReplayGain values as albums (by tags)"), window);
    auto* opusHeaderGainAction = new QAction(tr("Edit Opus header gain"), window);
    auto* rgRemoveAction       = new QAction(tr("Remove ReplayGain information from files"), window);
    rgTrackAction->setStatusTip(
        tr("Calculate ReplayGain values for selected files, considering each file individually"));
    rgSingleAlbumAction->setStatusTip(
        tr("Calculate ReplayGain values for selected files, considering all files as part of one album"));
    rgAlbumAction->setStatusTip(tr("Calculate ReplayGain values for selected files, dividing into albums by tags"));
    opusHeaderGainAction->setStatusTip(tr("Manipulate the Opus header gain field"));
    rgRemoveAction->setStatusTip(tr("Remove ReplayGain values from the selected files"));

    const auto removeInfo = [this]() {
        TrackList tracks = m_selectionController->selectedTracks();
        for(Track& track : tracks) {
            removeReplayGainInfoFromFile(track);
        }

        auto* removeDialog = createRemoveDialog();

        QObject::connect(
            m_library, &MusicLibrary::tracksMetadataChanged, this, [removeDialog]() { removeDialog->accept(); },
            Qt::SingleShotConnection);

        const auto request = m_library->writeTrackMetadata(tracks);

        QObject::connect(
            removeDialog, &QDialog::rejected, this,
            [request, removeDialog]() {
                request.cancel();
                removeDialog->reject();
            },
            Qt::SingleShotConnection);

        removeDialog->show();
    };

    const auto canWriteInfo = [](const TrackSelection& selection) -> bool {
        return std::ranges::any_of(selection.tracks, [](const Track& track) { return !track.isInArchive(); });
    };
    const auto hasWritableOpus = [](const TrackSelection& selection) -> bool {
        return std::ranges::any_of(selection.tracks, isWritableOpusTrack);
    };

    QObject::connect(rgTrackAction, &QAction::triggered, this, [this] { calculateReplayGain(RGScanType::Track); });
    QObject::connect(rgSingleAlbumAction, &QAction::triggered, this,
                     [this] { calculateReplayGain(RGScanType::SingleAlbum); });
    QObject::connect(rgAlbumAction, &QAction::triggered, this, [this] { calculateReplayGain(RGScanType::Album); });
    QObject::connect(opusHeaderGainAction, &QAction::triggered, this, [this]() {
        TrackList tracks;
        for(const Track& track : m_selectionController->selectedTracks()) {
            if(isWritableOpusTrack(track)) {
                tracks.emplace_back(track);
            }
        }

        if(tracks.empty()) {
            return;
        }

        auto* dialog = createOpusHeaderGainDialog(m_audioLoader, m_library, m_settings, tracks, Utils::getMainWindow());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    QObject::connect(rgRemoveAction, &QAction::triggered, this, removeInfo);

    const auto registerReplayGainAction
        = [this](QAction* action, const Id& id, const auto& updateState, bool hide = false) {
              m_selectionController->registerTrackContextAction(
                  this, TrackContextMenuArea::Track, Constants::Menus::Context::ReplayGain, id, action->text(),
                  [action, updateState, hide](QMenu* menu, const TrackSelection& selection) {
                      if(hide) {
                          action->setVisible(updateState(selection));
                      }
                      else {
                          action->setEnabled(updateState(selection));
                      }
                      menu->addAction(action);
                  });
          };

    registerReplayGainAction(rgTrackAction, "ReplayGain.Scan.Track",
                             [canWriteInfo](const TrackSelection& selection) { return canWriteInfo(selection); });
    registerReplayGainAction(rgSingleAlbumAction, "ReplayGain.Scan.SingleAlbum",
                             [canWriteInfo](const TrackSelection& selection) { return canWriteInfo(selection); });
    registerReplayGainAction(rgAlbumAction, "ReplayGain.Scan.Album", [canWriteInfo](const TrackSelection& selection) {
        return canWriteInfo(selection) && selection.tracks.size() > 1;
    });
    registerReplayGainAction(
        opusHeaderGainAction, "ReplayGain.OpusHeaderGain",
        [hasWritableOpus](const TrackSelection& selection) { return hasWritableOpus(selection); }, true);
    registerReplayGainAction(rgRemoveAction, "ReplayGain.RemoveInfo", [canWriteInfo](const TrackSelection& selection) {
        return canWriteInfo(selection) && std::ranges::any_of(selection.tracks, [](const Track& track) {
                   return track.hasRGInfo() || track.hasOpusHeaderGain();
               });
    });
}

QDialog* RGScannerPlugin::createRemoveDialog()
{
    auto* removeDialog = new QDialog(Utils::getMainWindow());
    removeDialog->setAttribute(Qt::WA_DeleteOnClose);
    removeDialog->setWindowTitle(tr("Remove ReplayGain Info"));
    removeDialog->setModal(true);

    auto* label = new QLabel(tr("Writing to file tags…"), removeDialog);
    label->setAlignment(Qt::AlignCenter);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, removeDialog);
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Abort"));
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, removeDialog, &QDialog::reject);

    auto* layout = new QVBoxLayout(removeDialog);
    layout->addWidget(label);
    layout->addWidget(buttonBox);

    return removeDialog;
}
} // namespace Fooyin::RGScanner

#include "moc_rgscannerplugin.cpp"
