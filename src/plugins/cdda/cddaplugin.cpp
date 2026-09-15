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
 */

#include "cddaplugin.h"

#include "accuraterip.h"
#include "cddadecoder.h"
#include "cddadrivemanager.h"
#include "cddadrivesettings.h"
#include "cddareader.h"
#include "cddatoc.h"
#include "drive/libcdiodrivebackend.h"
#include "openaudiocddialog.h"
#include "ripaudiocddialog.h"

#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugincontext.h>
#include <gui/conversion/conversionservice.h>
#include <gui/guiconstants.h>
#include <gui/metadatalookup/metadatalookupdialog.h>
#include <gui/playlist/currentplaylistcontroller.h>
#include <gui/plugins/guiplugincontext.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/utils.h>

#include <QAction>
#include <QMainWindow>
#include <QNetworkReply>
#include <QPointer>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
void CddaPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_playlistHandler  = context.playlistHandler;
    m_networkAccess    = context.networkAccess;
    m_settingsManager  = context.settingsManager;

    m_driveManager  = std::make_shared<CdDriveManager>(std::make_unique<LibcdioDriveBackend>());
    m_settingsStore = std::make_shared<CdDriveSettingsStore>();
}

void CddaPlugin::initialise(const GuiPluginContext& context)
{
    m_playlistController = context.playlistSelection;
    m_conversionService  = context.conversionService;

    auto* openAction = new QAction(tr("Open audio &CD…"), this);
    openAction->setStatusTip(tr("Open an audio CD for playback or ripping"));
    auto* command = context.actionManager->registerAction(openAction, "File.OpenAudioCd");
    command->setCategories({tr("File")});
    context.actionManager->actionContainer(Constants::Menus::File)->addAction(command, Actions::Groups::One);

    QObject::connect(openAction, &QAction::triggered, this, [this]() {
        auto* dialog = new OpenAudioCdDialog(m_driveManager, m_settingsStore, m_networkAccess, m_settingsManager,
                                             Utils::getMainWindow());
        dialog->setAttribute(Qt::WA_DeleteOnClose);

        QObject::connect(dialog, &OpenAudioCdDialog::addRequested, this, &CddaPlugin::addToPlaylist);
        QObject::connect(dialog, &OpenAudioCdDialog::playRequested, this, &CddaPlugin::play);
        QObject::connect(dialog, &OpenAudioCdDialog::ripRequested, this, &CddaPlugin::rip);

        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
}

void CddaPlugin::addToPlaylist(const TrackList& tracks, const CdDriveObservation& observation)
{
    if(tracks.empty()) {
        return;
    }

    m_driveManager->setPreferredDrive(observation.discId, observation.drive.id);

    const Playlist* playlist = m_playlistController->currentPlaylist();
    if(!playlist || playlist->isAutoPlaylist() || playlist->isLocked()) {
        playlist = m_playlistHandler->createNewPlaylist(tr("Audio CD"));
        m_playlistController->changeCurrentPlaylist(playlist->id());
    }

    m_playlistHandler->appendToPlaylist(playlist->id(), tracks);
    m_playlistController->selectTracks(tracks);
}

void CddaPlugin::play(const TrackList& tracks, const CdDriveObservation& observation)
{
    if(tracks.empty()) {
        return;
    }

    m_driveManager->setPreferredDrive(observation.discId, observation.drive.id);
    Playlist* playlist = m_playlistHandler->createNewPlaylist(tr("Audio CD"), tracks);
    m_playlistController->changeCurrentPlaylist(playlist->id());
    m_playerController->startPlayback(playlist);
}

void CddaPlugin::rip(const TrackList& tracks, const CdDriveObservation& observation)
{
    if(tracks.empty() || !observation.toc) {
        return;
    }

    m_driveManager->setPreferredDrive(observation.discId, observation.drive.id);

    QString discToc;
    if(const auto toc = musicBrainzToc(*observation.toc)) {
        discToc = *toc;
    }

    auto* dialog = new RipAudioCdDialog(
        tracks, m_conversionService->presets(), !discToc.isEmpty() && !observation.discId.isEmpty(),
        m_settingsStore->hasSettingsForDrive(observation.drive), Utils::getMainWindow());

    QObject::connect(dialog, &RipAudioCdDialog::metadataLookupRequested, this,
                     [this, dialog, discToc, discId = observation.discId](const TrackList& lookupTracks) {
                         auto* lookup = new MetadataLookupDialog(lookupTracks, m_networkAccess, m_settingsManager,
                                                                 {.mode       = LookupMode::DiscToc,
                                                                  .artist     = {},
                                                                  .album      = {},
                                                                  .discToc    = discToc,
                                                                  .discId     = discId,
                                                                  .identifier = {}},
                                                                 dialog);
                         lookup->setModal(true);

                         QObject::connect(lookup, &MetadataLookupDialog::tracksApplied, dialog,
                                          &RipAudioCdDialog::applyLookupTracks);
                         QObject::connect(lookup, &QObject::destroyed, dialog,
                                          &RipAudioCdDialog::metadataLookupFinished);

                         lookup->show();
                         lookup->raise();
                         lookup->activateWindow();
                     });
    QObject::connect(dialog, &RipAudioCdDialog::ripRequested, this,
                     [this, dialog, toc = *observation.toc](const TrackList& ripTracks, const QString& presetId,
                                                            bool showSetup, bool verifyAccurateRip) {
                         startRip(dialog, ripTracks, toc, presetId, showSetup, verifyAccurateRip);
                     });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void CddaPlugin::startRip(RipAudioCdDialog* dialog, TrackList tracks, CdToc toc, QString presetId, bool showSetup,
                          bool verifyAccurateRip)
{
    if(!verifyAccurateRip) {
        startConversion(dialog, tracks, presetId, showSetup, {});
        return;
    }

    const auto discId = accurateRipDiscId(toc);
    if(!discId) {
        startConversion(dialog, tracks, presetId, showSetup, {}, discId.error());
        return;
    }
    const auto layout = accurateRipLayout(toc);
    if(!layout) {
        startConversion(dialog, tracks, presetId, showSetup, {}, layout.error());
        return;
    }

    QNetworkReply* reply = m_networkAccess->get(makeNetworkRequest(accurateRipDiscUrl(*discId)));
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    QObject::connect(reply, &QNetworkReply::finished, dialog,
                     [this, dialog, reply, tracks = std::move(tracks), presetId = std::move(presetId), discId = *discId,
                      layout = *layout, showSetup]() {
                         if(reply->error() != QNetworkReply::NoError) {
                             startConversion(dialog, tracks, presetId, showSetup, {},
                                             reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404
                                                 ? tr("This disc is not present in AccurateRip.")
                                                 : tr("AccurateRip lookup failed: %1").arg(reply->errorString()));
                             return;
                         }

                         const QByteArray payload = reply->read((8 * 1024 * 1024) + 1);
                         if(payload.size() > 8L * 1024 * 1024) {
                             startConversion(dialog, tracks, presetId, showSetup, {},
                                             tr("AccurateRip returned an unexpectedly large disc record."));
                             return;
                         }

                         const auto records = parseAccurateRipResponse(payload, discId);
                         if(!records) {
                             startConversion(dialog, tracks, presetId, showSetup, {}, records.error());
                             return;
                         }

                         startConversion(dialog, tracks, presetId, showSetup,
                                         std::make_shared<AccurateRip::Verifier>(layout, *records, tracks));
                     });
}

void CddaPlugin::startConversion(RipAudioCdDialog* dialog, const TrackList& tracks, const QString& presetId,
                                 bool showSetup, std::shared_ptr<AccurateRip::Verifier> verifier, QString lookupMessage)
{
    const QPointer reportParent{dialog->parentWidget()};
    const auto completion = [verifier, lookupMessage = std::move(lookupMessage), reportParent](const auto&) {
        const auto results = verifier ? verifier->results() : std::vector<AccurateRip::TrackResult>{};
        if(!results.empty() || !lookupMessage.isEmpty()) {
            showAccurateRipResults(reportParent, results, lookupMessage);
        }
    };

    if(showSetup) {
        m_conversionService->showSetup(tracks, u"%tracknumber% - %title%"_s, verifier, completion);
        dialog->accept();
        return;
    }

    if(m_conversionService->startPreset(presetId, tracks, u"%tracknumber% - %title%"_s, verifier, completion)) {
        dialog->accept();
    }
    else {
        dialog->ripStartFailed();
    }
}

QString CddaPlugin::inputName() const
{
    return tr("Audio CD");
}

InputCreator CddaPlugin::inputCreator() const
{
    InputCreator creator;
    creator.decoder = [driveManager = m_driveManager, settingsStore = m_settingsStore]() {
        return std::make_unique<CddaDecoder>(driveManager, settingsStore);
    };
    creator.reader = [driveManager = m_driveManager]() {
        return std::make_unique<CddaReader>(driveManager);
    };
    return creator;
}
} // namespace Fooyin::Cdda

#include "moc_cddaplugin.cpp"
