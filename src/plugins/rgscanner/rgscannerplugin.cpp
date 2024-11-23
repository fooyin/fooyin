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

#include "rgscannerplugin.h"

#include "rgscanner.h"
#include "rgscannerpage.h"
#include "rgscanresults.h"

#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/actions/actioncontainer.h>
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
                     [this, scanner, progress](const TrackList& tracks) {
                         const auto finishTime = progress->elapsedTime();
                         scanner->close();
                         progress->deleteLater();

                         auto* rgResults = new RGScanResults(m_library, tracks, finishTime, Utils::getMainWindow());
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
        case(RGScanType::Track):
            scanner->calculatePerTrack(tracksToScan);
            break;
        case(RGScanType::SingleAlbum):
            scanner->calculateAsAlbum(tracksToScan);
            break;
        case(RGScanType::Album):
            scanner->calculateByAlbumTags(tracksToScan);
            break;
    }

    progress->startTimer();
}

void RGScannerPlugin::setupReplayGainMenu()
{
    auto* selectionMenu  = m_actionManager->actionContainer(Constants::Menus::Context::TrackSelection);
    auto* replayGainMenu = m_actionManager->createMenu(Constants::Menus::Context::ReplayGain);
    replayGainMenu->menu()->setTitle(tr("ReplayGain"));
    selectionMenu->addMenu(replayGainMenu);

    auto* window = Utils::getMainWindow();

    auto* rgTrackAction       = new QAction(tr("Calculate ReplayGain values per-file"), window);
    auto* rgSingleAlbumAction = new QAction(tr("Calculate ReplayGain values as a single album"), window);
    auto* rgAlbumAction       = new QAction(tr("Calculate ReplayGain values as albums (by tags)"), window);
    auto* rgRemoveAction      = new QAction(tr("Remove ReplayGain information from files"), window);
    rgTrackAction->setStatusTip(
        tr("Calculate ReplayGain values for selected files, considering each file individually"));
    rgSingleAlbumAction->setStatusTip(
        tr("Calculate ReplayGain values for selected files, considering all files as part of one album"));
    rgAlbumAction->setStatusTip(tr("Calculate ReplayGain values for selected files, dividing into albums by tags"));
    rgRemoveAction->setStatusTip(tr("Remove ReplayGain values from the selected files"));

    const auto removeInfo = [this]() {
        TrackList tracks = m_selectionController->selectedTracks();
        for(Track& track : tracks) {
            track.clearRGInfo();
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

    const auto canWriteInfo = [this]() -> bool {
        return std::ranges::any_of(m_selectionController->selectedTracks(),
                                   [](const Track& track) { return !track.isInArchive(); });
    };

    QObject::connect(rgTrackAction, &QAction::triggered, this, [this] { calculateReplayGain(RGScanType::Track); });
    QObject::connect(rgSingleAlbumAction, &QAction::triggered, this,
                     [this] { calculateReplayGain(RGScanType::SingleAlbum); });
    QObject::connect(rgAlbumAction, &QAction::triggered, this, [this] { calculateReplayGain(RGScanType::Album); });
    QObject::connect(rgRemoveAction, &QAction::triggered, this, removeInfo);

    QObject::connect(
        m_selectionController, &TrackSelectionController::selectionChanged, this,
        [this, replayGainMenu, rgSingleAlbumAction, rgAlbumAction, rgRemoveAction, canWriteInfo] {
            const bool tracksWritable = canWriteInfo();
            replayGainMenu->menu()->setEnabled(tracksWritable);
            rgAlbumAction->setEnabled(tracksWritable && m_selectionController->selectedTrackCount() > 1);
            rgSingleAlbumAction->setEnabled(tracksWritable && m_selectionController->selectedTrackCount() > 1);
            rgRemoveAction->setEnabled(tracksWritable
                                       && std::ranges::any_of(m_selectionController->selectedTracks(),
                                                              [](const Track& track) { return track.hasRGInfo(); }));
        });

    replayGainMenu->menu()->addAction(rgTrackAction);
    replayGainMenu->menu()->addAction(rgSingleAlbumAction);
    replayGainMenu->menu()->addAction(rgAlbumAction);
    replayGainMenu->menu()->addAction(rgRemoveAction);
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
