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

#include "openaudiocddialog.h"

#include "cddadrivesettingsdialog.h"
#include "cddareader.h"
#include "cddatoc.h"

#include <core/network/networkaccessmanager.h>
#include <gui/metadatalookup/metadataapply.h>
#include <gui/metadatalookup/metadatalookupdialog.h>
#include <gui/metadatalookup/sources/musicbrainzmetadata.h>
#include <gui/widgets/elidedlabel.h>
#include <utils/async.h>

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
QString displayDuration(uint64_t milliseconds)
{
    const uint64_t seconds = milliseconds / 1000;
    return u"%1:%2"_s.arg(seconds / 60).arg(seconds % 60, 2, 10, u'0');
}
} // namespace

OpenAudioCdDialog::OpenAudioCdDialog(std::shared_ptr<CdDriveManager> driveManager,
                                     std::shared_ptr<CdDriveSettingsStore> settingsStore,
                                     std::shared_ptr<NetworkAccessManager> networkAccess,
                                     SettingsManager* settingsManager, QWidget* parent)
    : QDialog{parent}
    , m_driveManager{std::move(driveManager)}
    , m_settingsStore{std::move(settingsStore)}
    , m_networkAccess{std::move(networkAccess)}
    , m_settingsManager{settingsManager}
    , m_driveRequestId{0}
    , m_drives{new QComboBox(this)}
    , m_status{new ElidedLabel(this)}
    , m_trackTable{new QTableWidget(this)}
    , m_refreshButton{new QPushButton(tr("Refresh"), this)}
    , m_settingsButton{new QPushButton(tr("Drive settings…"), this)}
    , m_metadataButton{new QPushButton(tr("Metadata"), this)}
    , m_cdTextAction{new QAction(tr("Read CD-Text"), this)}
    , m_autoLookupAction{new QAction(tr("Auto lookup metadata"), this)}
    , m_lookupAction{new QAction(tr("Lookup metadata…"), this)}
    , m_ripButton{new QPushButton(tr("Rip…"), this)}
    , m_playButton{new QPushButton(tr("Play"), this)}
    , m_addButton{new QPushButton(tr("Add to playlist"), this)}
{
    setWindowTitle(tr("Open Audio CD"));
    resize(640, 430);

    m_trackTable->setColumnCount(3);
    m_trackTable->setHorizontalHeaderLabels({tr("Track"), tr("Title"), tr("Duration")});
    m_trackTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_trackTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_trackTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_trackTable->verticalHeader()->hide();
    m_trackTable->setAlternatingRowColors(true);
    m_trackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_trackTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_trackTable->horizontalHeaderItem(2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* metadataMenu = new QMenu(m_metadataButton);
    metadataMenu->addAction(m_cdTextAction);
    metadataMenu->addAction(m_autoLookupAction);
    metadataMenu->addAction(m_lookupAction);
    m_metadataButton->setMenu(metadataMenu);

    const int driveControlHeight = std::max(
        {m_drives->sizeHint().height(), m_refreshButton->sizeHint().height(), m_settingsButton->sizeHint().height()});
    m_drives->setFixedHeight(driveControlHeight);
    m_refreshButton->setFixedHeight(driveControlHeight);
    m_settingsButton->setFixedHeight(driveControlHeight);

    auto* driveRow = new QHBoxLayout();
    driveRow->addWidget(new QLabel(tr("CD drive") + u""_s, this));
    driveRow->addWidget(m_drives, 1);
    driveRow->addWidget(m_refreshButton);
    driveRow->addWidget(m_settingsButton);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* trackButtons = new QHBoxLayout();
    trackButtons->addWidget(m_metadataButton);
    trackButtons->addWidget(m_ripButton);
    trackButtons->addWidget(m_playButton);
    trackButtons->addWidget(m_addButton);
    trackButtons->addStretch();
    trackButtons->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(driveRow);
    layout->addWidget(m_status);
    layout->addWidget(m_trackTable, 1);
    layout->addLayout(trackButtons);

    QObject::connect(m_drives, &QComboBox::currentIndexChanged, this, &OpenAudioCdDialog::showObservation);
    QObject::connect(m_refreshButton, &QPushButton::clicked, this, &OpenAudioCdDialog::refreshCurrentDrive);
    QObject::connect(m_settingsButton, &QPushButton::clicked, this, &OpenAudioCdDialog::showDriveSettings);
    QObject::connect(m_cdTextAction, &QAction::triggered, this, [this] {
        cancelAutomaticLookup();
        loadCdText(m_drives->currentIndex());
    });
    QObject::connect(m_autoLookupAction, &QAction::triggered, this,
                     [this] { startAutomaticLookup(m_drives->currentIndex()); });
    QObject::connect(m_lookupAction, &QAction::triggered, this, &OpenAudioCdDialog::showMetadataLookup);
    QObject::connect(m_ripButton, &QPushButton::clicked, this, &OpenAudioCdDialog::rip);
    QObject::connect(m_playButton, &QPushButton::clicked, this, &OpenAudioCdDialog::play);
    QObject::connect(m_addButton, &QPushButton::clicked, this, &OpenAudioCdDialog::addToPlaylist);
    QObject::connect(m_trackTable, &QTableWidget::itemChanged, this, &OpenAudioCdDialog::updateActions);
    QObject::connect(m_trackTable, &QWidget::customContextMenuRequested, this, [this](const QPoint& position) {
        auto* menu                = new QMenu(m_trackTable);
        const QAction* checkAll   = menu->addAction(tr("Check all"));
        const QAction* uncheckAll = menu->addAction(tr("Uncheck all"));

        const auto updateCheckState = [&](Qt::CheckState state) {
            const QSignalBlocker blocker{m_trackTable};
            for(int row{0}; row < m_trackTable->rowCount(); ++row) {
                if(QTableWidgetItem* item = m_trackTable->item(row, 0)) {
                    item->setCheckState(state);
                }
            }
            updateActions();
        };

        QObject::connect(checkAll, &QAction::triggered, this, [updateCheckState]() { updateCheckState(Qt::Checked); });
        QObject::connect(uncheckAll, &QAction::triggered, this,
                         [updateCheckState]() { updateCheckState(Qt::Unchecked); });

        menu->popup(m_trackTable->viewport()->mapToGlobal(position));
    });

    enumerateDrives();
}

void OpenAudioCdDialog::enumerateDrives()
{
    cancelAutomaticLookup();

    if(m_lookupDialog) {
        m_lookupDialog->close();
    }

    m_refreshButton->setEnabled(false);
    m_drives->setEnabled(false);
    m_settingsButton->setEnabled(false);
    m_trackTable->setEnabled(false);

    setStatus(tr("Searching for CD drives…"));
    m_tracks.clear();
    updateActions();

    const uint64_t requestId = ++m_driveRequestId;
    Utils::asyncExec([manager = m_driveManager] {
        return manager->drives();
    }).then(this, [this, requestId](std::vector<CdDriveInfo> drives) {
        if(requestId != m_driveRequestId) {
            return;
        }

        m_driveStates.clear();
        m_driveStates.reserve(drives.size());
        {
            const QSignalBlocker blocker{m_drives};
            m_drives->clear();
            for(CdDriveInfo& drive : drives) {
                m_drives->addItem(drive.displayName);
                m_driveStates.push_back({.drive = std::move(drive), .observation = std::nullopt});
            }
        }

        m_refreshButton->setEnabled(true);
        m_drives->setEnabled(!m_driveStates.empty());

        showObservation(m_driveStates.empty() ? -1 : 0);
    });
}

void OpenAudioCdDialog::refreshCurrentDrive()
{
    cancelAutomaticLookup();

    if(m_lookupDialog) {
        m_lookupDialog->close();
    }

    const int index = m_drives->currentIndex();
    if(index < 0 || std::cmp_greater_equal(index, m_driveStates.size())) {
        enumerateDrives();
        return;
    }

    m_refreshButton->setEnabled(false);
    m_drives->setEnabled(false);
    m_trackTable->setEnabled(false);

    setStatus(tr("Reading audio CD…"));
    m_tracks.clear();
    updateActions();

    const CdDriveInfo drive  = m_driveStates.at(index).drive;
    const uint64_t requestId = ++m_driveRequestId;
    Utils::asyncExec([manager = m_driveManager, drive] {
        return manager->observeDrive(drive);
    }).then(this, [this, requestId, index](CdDriveObservation observation) {
        if(requestId != m_driveRequestId || std::cmp_greater_equal(index, m_driveStates.size())) {
            return;
        }

        m_driveStates[index].drive       = observation.drive;
        m_driveStates[index].observation = std::move(observation);
        m_drives->setItemText(index, m_driveStates[index].drive.displayName);
        m_refreshButton->setEnabled(true);
        m_drives->setEnabled(!m_driveStates.empty());

        showObservation(index);
    });
}

void OpenAudioCdDialog::showObservation(int index)
{
    cancelAutomaticLookup();

    if(m_lookupDialog) {
        m_lookupDialog->close();
    }

    m_trackTable->setRowCount(0);
    m_tracks.clear();

    if(index < 0 || std::cmp_greater_equal(index, m_driveStates.size())) {
        setStatus(tr("No CD drives found."));
        m_settingsButton->setEnabled(false);
        m_trackTable->setEnabled(false);
        updateActions();
        return;
    }

    m_drives->setCurrentIndex(index);
    const DriveState& driveState = m_driveStates.at(index);
    m_settingsButton->setEnabled(true);

    if(!driveState.observation) {
        refreshCurrentDrive();
        return;
    }

    const CdDriveObservation& observation = *driveState.observation;

    if(observation.error) {
        setStatus(observation.error->message);
        m_trackTable->setEnabled(false);
        updateActions();
        return;
    }
    if(!observation.toc) {
        setStatus(tr("No audio CD is available in this drive."));
        m_trackTable->setEnabled(false);
        updateActions();
        return;
    }

    const auto tracks = tracksForDisc(*observation.toc, observation.discId, observation.cdText.value_or(CdText{}));
    if(!tracks) {
        setStatus(tracks.error());
        m_trackTable->setEnabled(false);
        updateActions();
        return;
    }

    m_tracks = *tracks;

    const auto trackCount = static_cast<int>(m_tracks.size());

    setStatus(tr("Found %Ln audio track(s).", nullptr, trackCount));
    m_trackTable->setRowCount(trackCount);

    for(int row{0}; row < trackCount; ++row) {
        const Track& track = m_tracks.at(static_cast<size_t>(row));

        auto* number = new QTableWidgetItem(track.trackNumber());
        number->setCheckState(Qt::Checked);
        m_trackTable->setItem(row, 0, number);
        m_trackTable->setItem(row, 1, new QTableWidgetItem(track.effectiveTitle()));

        auto* duration = new QTableWidgetItem(displayDuration(track.duration()));
        duration->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_trackTable->setItem(row, 2, duration);
    }

    m_trackTable->setEnabled(true);
    updateActions();
}

void OpenAudioCdDialog::loadCdText(int index)
{
    if(index < 0 || std::cmp_greater_equal(index, m_driveStates.size()) || !m_driveStates.at(index).observation) {
        return;
    }

    const CdDriveObservation observation = *m_driveStates.at(index).observation;
    if(!observation.toc || observation.discId.isEmpty() || m_cdTextLoads.contains(observation.drive.id)) {
        return;
    }

    m_cdTextLoads.emplace(observation.drive.id);
    setStatus(tr("Reading CD-Text…"));
    updateActions();

    const TrackList originalTracks{m_tracks};
    Utils::asyncExec([manager = m_driveManager, observation] {
        return manager->readCdText(observation);
    }).then(this, [this, index, observation, originalTracks](std::expected<CdText, CdError> result) {
        m_cdTextLoads.erase(observation.drive.id);

        if(index < 0 || std::cmp_greater_equal(index, m_driveStates.size()) || !m_driveStates.at(index).observation) {
            updateActions();
            return;
        }

        CdDriveObservation& current = *m_driveStates[index].observation;
        if(current.drive.id != observation.drive.id || current.discId != observation.discId
           || current.generation != observation.generation || !current.toc) {
            updateActions();
            return;
        }

        if(!result) {
            if(m_drives->currentIndex() == index) {
                setStatus(tr("Failed to read CD-Text: %1").arg(result.error().message));
            }
            updateActions();
            return;
        }

        current.cdText = std::move(*result);

        if(m_drives->currentIndex() != index) {
            updateActions();
            return;
        }

        if(current.cdText->empty()) {
            setStatus(tr("No CD-Text found."));
            updateActions();
            return;
        }

        const bool tracksUnchanged
            = originalTracks.size() == m_tracks.size()
           && std::ranges::equal(originalTracks, m_tracks,
                                 [](const Track& lhs, const Track& rhs) { return lhs.sameDataAs(rhs); });
        if(!tracksUnchanged) {
            setStatus(tr("CD-Text was read; existing metadata was kept."));
            updateActions();
            return;
        }

        const auto tracks = tracksForDisc(*current.toc, current.discId, *current.cdText);
        if(tracks) {
            replaceTracks(*tracks,
                          tr("Applied CD-Text to %Ln audio track(s).", nullptr, static_cast<int>(tracks->size())));
        }
        else {
            setStatus(tracks.error());
            updateActions();
        }
    });
}

void OpenAudioCdDialog::startAutomaticLookup(int index)
{
    if(index < 0 || std::cmp_greater_equal(index, m_driveStates.size()) || !m_driveStates.at(index).observation) {
        return;
    }

    const CdDriveObservation observation = *m_driveStates.at(index).observation;
    if(!observation.toc || observation.discId.isEmpty()) {
        return;
    }

    const auto discToc = musicBrainzToc(*observation.toc);
    if(!discToc) {
        return;
    }

    if(m_autoLookup && m_autoLookupDiscId == observation.discId) {
        return;
    }

    cancelAutomaticLookup();

    auto* client       = new MusicBrainzMetadata(m_networkAccess.get(), this);
    m_autoLookup       = client;
    m_autoLookupDiscId = observation.discId;

    const auto isCurrent = [this, client, index, observation] {
        if(index < 0 || std::cmp_greater_equal(index, m_driveStates.size()) || !m_driveStates.at(index).observation) {
            return false;
        }

        const auto& ob = *m_driveStates.at(index).observation;
        return m_autoLookup == client && m_drives->currentIndex() == index && ob.drive.id == observation.drive.id
            && ob.discId == observation.discId && ob.generation == observation.generation;
    };

    QObject::connect(client, &MusicBrainzMetadata::searchFinished, this,
                     [this, client, isCurrent](const std::vector<ReleaseSummary>& releases) {
                         if(!isCurrent()) {
                             finishAutomaticLookup(client);
                             return;
                         }
                         for(const ReleaseSummary& release : releases) {
                             if(!release.id.isEmpty() && !std::ranges::contains(m_autoLookupReleaseIds, release.id)) {
                                 m_autoLookupReleaseIds.push_back(release.id);
                             }
                         }
                         fetchNextAutomaticRelease(client);
                     });
    QObject::connect(client, &MusicBrainzMetadata::releaseFetched, this,
                     [this, client, isCurrent, discId = observation.discId](const Release& release) {
                         if(!isCurrent()) {
                             finishAutomaticLookup(client);
                             return;
                         }
                         const auto tracks = applyAutomaticDiscMetadata(m_tracks, release, discId);
                         if(tracks) {
                             if(m_autoLookupTracks) {
                                 setStatus(tr("Automatic metadata lookup needs review."));
                                 finishAutomaticLookup(client);
                                 return;
                             }
                             m_autoLookupTracks = *tracks;
                         }
                         fetchNextAutomaticRelease(client);
                     });
    QObject::connect(client, &MusicBrainzMetadata::failed, this, [this, client, isCurrent](const QString& error) {
        if(isCurrent()) {
            setStatus(tr("Automatic metadata lookup failed: %1").arg(error));
        }
        finishAutomaticLookup(client);
    });

    setStatus(tr("Looking up audio CD metadata…"));
    updateActions();
    client->search({.mode       = LookupMode::DiscToc,
                    .artist     = {},
                    .album      = {},
                    .discToc    = *discToc,
                    .discId     = observation.discId,
                    .identifier = {}});
}

void OpenAudioCdDialog::fetchNextAutomaticRelease(MusicBrainzMetadata* client)
{
    if(m_autoLookup != client) {
        return;
    }

    if(!m_autoLookupReleaseIds.empty()) {
        const QString releaseId = std::move(m_autoLookupReleaseIds.front());
        m_autoLookupReleaseIds.erase(m_autoLookupReleaseIds.begin());
        client->fetchRelease(releaseId);
        return;
    }

    if(m_autoLookupTracks) {
        replaceTracks(*m_autoLookupTracks, tr("Applied automatic metadata to %Ln audio track(s).", nullptr,
                                              static_cast<int>(m_autoLookupTracks->size())));
    }
    else {
        setStatus(tr("No automatic metadata match found."));
    }

    finishAutomaticLookup(client);
}

void OpenAudioCdDialog::cancelAutomaticLookup()
{
    if(m_autoLookup) {
        m_autoLookup->cancel();
        m_autoLookup->deleteLater();
    }

    m_autoLookupDiscId.clear();
    m_autoLookupReleaseIds.clear();
    m_autoLookupTracks.reset();

    updateActions();
}

void OpenAudioCdDialog::finishAutomaticLookup(MusicBrainzMetadata* client)
{
    if(m_autoLookup != client) {
        return;
    }

    client->deleteLater();

    m_autoLookupDiscId.clear();
    m_autoLookupReleaseIds.clear();
    m_autoLookupTracks.reset();

    updateActions();
}

void OpenAudioCdDialog::applyLookupTracks(const TrackList& tracks)
{
    const bool sameTracks = tracks.size() == m_tracks.size()
                         && std::ranges::equal(tracks, m_tracks, [](const Track& lhs, const Track& rhs) {
                                return lhs.sameIdentityAs(rhs);
                            });
    if(!sameTracks) {
        return;
    }

    replaceTracks(tracks, tr("Applied metadata to %Ln audio track(s).", nullptr, static_cast<int>(tracks.size())));
}

void OpenAudioCdDialog::replaceTracks(const TrackList& tracks, const QString& status)
{
    m_tracks = tracks;

    const QSignalBlocker blocker{m_trackTable};

    for(int row{0}; row < m_trackTable->rowCount() && std::cmp_less(row, m_tracks.size()); ++row) {
        const Track& track = m_tracks.at(row);
        m_trackTable->item(row, 0)->setText(track.trackNumber());
        m_trackTable->item(row, 1)->setText(track.effectiveTitle());
        m_trackTable->item(row, 2)->setText(displayDuration(track.duration()));
    }

    setStatus(status);
    updateActions();
}

void OpenAudioCdDialog::setStatus(const QString& status)
{
    m_status->setText(status);
    m_status->setToolTip(status);
}

void OpenAudioCdDialog::updateActions()
{
    bool canAutoLookup = !m_tracks.empty();
    const int index    = m_drives->currentIndex();

    if(canAutoLookup && index >= 0 && std::cmp_less(index, m_driveStates.size())
       && m_driveStates.at(index).observation) {
        const CdDriveObservation& observation = *m_driveStates.at(index).observation;
        canAutoLookup
            = observation.toc && !observation.discId.isEmpty() && musicBrainzToc(*observation.toc).has_value();
    }
    else {
        canAutoLookup = false;
    }

    bool canReadCdText = !m_tracks.empty();
    if(canReadCdText && index >= 0 && std::cmp_less(index, m_driveStates.size())
       && m_driveStates.at(index).observation) {
        const CdDriveObservation& observation = *m_driveStates.at(index).observation;
        canReadCdText
            = observation.toc && !observation.discId.isEmpty() && !m_cdTextLoads.contains(observation.drive.id);
    }
    else {
        canReadCdText = false;
    }

    const bool hasTracks = !checkedTracks().empty();

    m_cdTextAction->setEnabled(canReadCdText);
    m_autoLookupAction->setEnabled(canAutoLookup && !m_autoLookup);
    m_lookupAction->setEnabled(canAutoLookup);
    m_metadataButton->setEnabled(canReadCdText || canAutoLookup);
    m_addButton->setEnabled(hasTracks);
    m_playButton->setEnabled(hasTracks);
    m_ripButton->setEnabled(hasTracks);
}

TrackList OpenAudioCdDialog::checkedTracks() const
{
    TrackList tracks;

    for(int row{0}; row < m_trackTable->rowCount() && std::cmp_less(row, m_tracks.size()); ++row) {
        const QTableWidgetItem* item = m_trackTable->item(row, 0);
        if(item && item->checkState() == Qt::Checked) {
            tracks.push_back(m_tracks.at(static_cast<size_t>(row)));
        }
    }

    return tracks;
}

void OpenAudioCdDialog::showMetadataLookup()
{
    if(m_lookupDialog) {
        m_lookupDialog->raise();
        m_lookupDialog->activateWindow();
        return;
    }

    const int index = m_drives->currentIndex();
    if(m_tracks.empty() || !m_networkAccess || !m_settingsManager || index < 0
       || std::cmp_greater_equal(index, m_driveStates.size()) || !m_driveStates.at(index).observation) {
        return;
    }

    const CdDriveObservation& observation = *m_driveStates.at(index).observation;
    if(!observation.toc || observation.discId.isEmpty()) {
        return;
    }

    const auto discToc = musicBrainzToc(*observation.toc);
    if(!discToc) {
        return;
    }

    cancelAutomaticLookup();

    m_lookupDialog = new MetadataLookupDialog(m_tracks, m_networkAccess, m_settingsManager,
                                              {.mode       = LookupMode::DiscToc,
                                               .artist     = {},
                                               .album      = {},
                                               .discToc    = *discToc,
                                               .discId     = observation.discId,
                                               .identifier = {}},
                                              this);
    m_lookupDialog->setModal(true);

    QObject::connect(m_lookupDialog, &MetadataLookupDialog::tracksApplied, this, &OpenAudioCdDialog::applyLookupTracks);

    m_lookupDialog->show();
    m_lookupDialog->raise();
    m_lookupDialog->activateWindow();
}

void OpenAudioCdDialog::addToPlaylist()
{
    const TrackList tracks = checkedTracks();
    if(tracks.empty()) {
        return;
    }

    const auto& observation = *m_driveStates.at(m_drives->currentIndex()).observation;
    Q_EMIT addRequested(tracks, observation);
}

void OpenAudioCdDialog::play()
{
    const TrackList tracks = checkedTracks();
    if(tracks.empty()) {
        return;
    }

    const auto& observation = *m_driveStates.at(m_drives->currentIndex()).observation;
    Q_EMIT playRequested(tracks, observation);
    accept();
}

void OpenAudioCdDialog::rip()
{
    const TrackList tracks = checkedTracks();
    if(tracks.empty()) {
        return;
    }

    const auto& observation = *m_driveStates.at(m_drives->currentIndex()).observation;
    Q_EMIT ripRequested(tracks, observation);
}

void OpenAudioCdDialog::showDriveSettings()
{
    const int index = m_drives->currentIndex();
    if(index < 0 || std::cmp_greater_equal(index, m_driveStates.size())) {
        return;
    }

    auto* dialog = new CdDriveSettingsDialog(m_driveStates.at(index).drive, m_settingsStore, m_networkAccess, this);
    dialog->show();
}
} // namespace Fooyin::Cdda
