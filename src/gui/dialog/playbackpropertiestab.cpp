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

#include "playbackpropertiestab.h"

#include "core/playback/trackplaybacksettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <ranges>

using namespace Qt::StringLiterals;

namespace Fooyin {
PlaybackPropertiesTab::PlaybackPropertiesTab(MusicLibrary* library, TrackList tracks, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_library{library}
    , m_session{nullptr}
    , m_tracks{std::move(tracks)}
    , m_exclusion{new QComboBox(this)}
    , m_disableCrossfading{new QCheckBox(tr("Disable crossfading"), this)}
    , m_exclusionChanged{false}
    , m_crossfadingChanged{false}
{
    m_exclusion->addItem(tr("Never"), QString{});
    m_exclusion->addItem(tr("When shuffling"), u"shuffle"_s);
    m_exclusion->addItem(tr("Always"), u"always"_s);

    m_exclusion->setPlaceholderText(tr("Multiple values"));
    m_exclusion->setToolTip(tr("Excluded tracks can still be played explicitly or added to the playback queue."));

    m_disableCrossfading->setToolTip(tr("Disable crossfading into and out of these tracks"));

    auto* behaviour = new QGroupBox(tr("Track overrides"), this);
    auto* form      = new QFormLayout(behaviour);
    form->addRow(tr("Exclude from automatic playback") + u":"_s, m_exclusion);
    form->addRow(m_disableCrossfading);

    auto* reset  = new QPushButton(tr("Reset to defaults"), this);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(behaviour);
    layout->addStretch();
    layout->addWidget(reset, 0, Qt::AlignLeft);

    QObject::connect(m_exclusion, &QComboBox::currentIndexChanged, this, [this]() {
        m_exclusionChanged = true;
        Q_EMIT pendingChangesStateChanged();
    });
    QObject::connect(m_disableCrossfading, &QCheckBox::clicked, this, [this]() {
        m_crossfadingChanged = true;
        Q_EMIT pendingChangesStateChanged();
    });
    QObject::connect(reset, &QPushButton::clicked, this, [this]() {
        m_exclusion->setCurrentIndex(0);
        m_disableCrossfading->setCheckState(Qt::Unchecked);
        m_exclusionChanged   = true;
        m_crossfadingChanged = true;
        Q_EMIT pendingChangesStateChanged();
    });

    refreshControls();
}

void PlaybackPropertiesTab::apply()
{
    commitPendingChanges();

    TrackList changedTracks;
    const TrackList& tracks = m_session ? m_session->workingTracks() : m_tracks;
    for(Track track : tracks) {
        if(const auto changes = m_pendingChanges.find(track.id()); changes != m_pendingChanges.end()) {
            applyChanges(track, changes->second);
            changedTracks.push_back(std::move(track));
        }
    }
    if(!changedTracks.empty()) {
        Q_EMIT tracksChanged(changedTracks);
        m_library->updateTrackMetadata(changedTracks);
    }
    m_pendingChanges.clear();
}

void PlaybackPropertiesTab::setSession(PropertiesDialogSession* session)
{
    m_session = session;
}

void PlaybackPropertiesTab::setTrackScope(const TrackList& tracks)
{
    m_tracks = tracks;
    for(Track& track : m_tracks) {
        if(const auto changes = m_pendingChanges.find(track.id()); changes != m_pendingChanges.end()) {
            applyChanges(track, changes->second);
        }
    }

    refreshControls();
}

void PlaybackPropertiesTab::updateTracks(const TrackList& tracks)
{
    setTrackScope(tracks);
}

bool PlaybackPropertiesTab::isAvailableForScope(const TrackList& tracks) const
{
    return !tracks.empty() && std::ranges::all_of(tracks, &Track::isInDatabase);
}

bool PlaybackPropertiesTab::hasPendingScopeChanges() const
{
    return m_exclusionChanged || m_crossfadingChanged;
}

bool PlaybackPropertiesTab::commitPendingChanges()
{
    if(!hasPendingScopeChanges()) {
        return true;
    }

    TrackList changedTracks;
    Changes changes;

    if(m_exclusionChanged && m_exclusion->currentIndex() >= 0) {
        changes.exclusion = m_exclusion->currentData().toString();
    }
    if(m_crossfadingChanged && m_disableCrossfading->checkState() != Qt::PartiallyChecked) {
        changes.disableCrossfading = m_disableCrossfading->isChecked();
    }
    for(Track track : m_tracks) {
        const Track original{track};
        applyChanges(track, changes);
        if(!track.sameDataAs(original)) {
            auto& pending = m_pendingChanges[track.id()];
            if(changes.exclusion) {
                pending.exclusion = changes.exclusion;
            }
            if(changes.disableCrossfading) {
                pending.disableCrossfading = changes.disableCrossfading;
            }
            changedTracks.push_back(std::move(track));
        }
    }

    m_exclusionChanged   = false;
    m_crossfadingChanged = false;

    if(!changedTracks.empty()) {
        Q_EMIT tracksChanged(changedTracks);
    }
    if(m_session) {
        m_tracks = m_session->activeTracks();
    }
    else {
        for(const Track& track : changedTracks) {
            for(Track& scopedTrack : m_tracks) {
                if(track.sameIdentityAs(scopedTrack)) {
                    scopedTrack = track;
                }
            }
        }
    }

    refreshControls();
    Q_EMIT pendingChangesStateChanged();
    return true;
}

void PlaybackPropertiesTab::applyChanges(Track& track, const Changes& changes)
{
    if(changes.exclusion) {
        track.setExtraProperty(QString::fromLatin1(TrackPlayback::ExclusionProperty), *changes.exclusion,
                               Track::ExtraPropertyReloadPolicy::Preserve);
    }
    if(changes.disableCrossfading) {
        track.setExtraProperty(QString::fromLatin1(TrackPlayback::DisableCrossfadingProperty),
                               *changes.disableCrossfading ? u"1"_s : QString{},
                               Track::ExtraPropertyReloadPolicy::Preserve);
    }
}

void PlaybackPropertiesTab::refreshControls()
{
    const QSignalBlocker exclusionBlocker{m_exclusion};
    const QSignalBlocker crossfadingBlocker{m_disableCrossfading};

    m_exclusionChanged   = false;
    m_crossfadingChanged = false;

    QString exclusion;
    bool crossfadingDisabled{false};
    if(!m_tracks.empty()) {
        exclusion = m_tracks.front().extraProperties().value(QString::fromLatin1(TrackPlayback::ExclusionProperty));
        crossfadingDisabled = TrackPlayback::crossfadingDisabled(m_tracks.front());
    }

    const bool mixedExclusion   = std::ranges::any_of(m_tracks, [&exclusion](const Track& track) {
        return track.extraProperties().value(QString::fromLatin1(TrackPlayback::ExclusionProperty)) != exclusion;
    });
    const bool mixedCrossfading = std::ranges::any_of(m_tracks, [crossfadingDisabled](const Track& track) {
        return TrackPlayback::crossfadingDisabled(track) != crossfadingDisabled;
    });

    m_exclusion->setCurrentIndex(mixedExclusion ? -1 : std::max(0, m_exclusion->findData(exclusion)));
    m_disableCrossfading->setTristate(mixedCrossfading);
    m_disableCrossfading->setCheckState(mixedCrossfading ? Qt::PartiallyChecked
                                                         : (crossfadingDisabled ? Qt::Checked : Qt::Unchecked));
}
} // namespace Fooyin
