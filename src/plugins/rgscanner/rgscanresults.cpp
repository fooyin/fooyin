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

#include "rgscanresults.h"

#include "opusreplaygainutils.h"
#include "rgscanresultsmodel.h"

#include <core/internalcoresettings.h>
#include <core/library/musiclibrary.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin::RGScanner {
namespace {
const Track* findOriginalTrack(const TrackList& tracks, const Track& scannedTrack)
{
    const auto it = std::ranges::find_if(
        tracks, [&scannedTrack](const Track& originalTrack) { return originalTrack.sameIdentityAs(scannedTrack); });
    return it != tracks.cend() ? &*it : nullptr;
}

Track mergePerTrackReplayGainWriteData(const Track& scannedTrack, const TrackList& originalTracks)
{
    Track writeTrack{scannedTrack};
    const Track* originalTrack = findOriginalTrack(originalTracks, scannedTrack);
    if(!originalTrack) {
        return writeTrack;
    }

    if(originalTrack->hasAlbumGain()) {
        writeTrack.setRGAlbumGain(originalTrack->rgAlbumGain());
    }

    if(originalTrack->hasAlbumPeak()) {
        writeTrack.setRGAlbumPeak(originalTrack->rgAlbumPeak());
    }

    return writeTrack;
}
} // namespace

RGScanResults::RGScanResults(MusicLibrary* library, SettingsManager* settings, TrackList originalTracks,
                             TrackList tracks, const RGScanType scanType, std::chrono::milliseconds timeTaken,
                             QWidget* parent)
    : QDialog{parent}
    , m_library{library}
    , m_settings{settings}
    , m_originalTracks{std::move(originalTracks)}
    , m_tracks{std::move(tracks)}
    , m_scanType{scanType}
    , m_opusWriteMode{static_cast<OpusRGWriteMode>(settings->value<Settings::Core::Internal::OpusHeaderWriteMode>())}
    , m_resultsView{new QTableView(this)}
    , m_resultsModel{new RGScanResultsModel(m_tracks, this)}
    , m_status{new QLabel(tr("Time taken") + ": "_L1 + Utils::msToString(timeTaken, false), this)}
    , m_buttonBox{new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this)}
{
    setWindowTitle(tr("ReplayGain Scan Results"));
    setModal(true);

    m_resultsView->setModel(m_resultsModel);
    m_resultsView->verticalHeader()->hide();
    m_resultsView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultsView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultsView->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    auto* applyButton = m_buttonBox->button(QDialogButtonBox::Ok);
    applyButton->setText(tr("&Update File Tags"));
    QObject::connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_resultsView, 0, 0, 1, 2);
    layout->addWidget(m_status, 1, 0);
    layout->addWidget(m_buttonBox, 1, 1);
    layout->setColumnStretch(1, 1);
}

void RGScanResults::accept()
{
    QObject::connect(
        m_library, &MusicLibrary::tracksMetadataChanged, this, [this]() { QDialog::accept(); },
        Qt::SingleShotConnection);

    m_status->setText(tr("Writing to file tags…"));
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    TrackList tracksToWrite;
    tracksToWrite.reserve(m_tracks.size());

    for(const Track& track : std::as_const(m_tracks)) {
        Track writeTrack = m_scanType == RGScanType::Track ? mergePerTrackReplayGainWriteData(track, m_originalTracks)
                                                           : Track{track};
        if(writeTrack.isOpus()) {
            writeTrack = prepareOpusRGWriteTrack(writeTrack, m_opusWriteMode);
            if(m_opusWriteMode == OpusRGWriteMode::LeaveNull && track.hasOpusHeaderGain()) {
                writeTrack.setOpusHeaderGainQ78(0);
            }
        }
        else {
            writeTrack = prepareOpusRGWriteTrack(writeTrack, m_opusWriteMode);
        }
        tracksToWrite.emplace_back(std::move(writeTrack));
    }

    const auto request = m_library->writeTrackMetadata(tracksToWrite);
    QObject::connect(
        m_buttonBox, &QDialogButtonBox::rejected, this, [request]() { request.cancel(); }, Qt::SingleShotConnection);
}

QSize RGScanResults::sizeHint() const
{
    QSize size = m_resultsView->sizeHint();
    size.rheight() += 200;
    size.rwidth() += 400;
    return size;
}

QSize RGScanResults::minimumSizeHint() const
{
    return QDialog::minimumSizeHint();
}
} // namespace Fooyin::RGScanner
