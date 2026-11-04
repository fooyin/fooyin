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

#pragma once

#include <gui/propertiesdialog.h>

#include <map>
#include <optional>

class QCheckBox;
class QComboBox;

namespace Fooyin {
class PlaybackPropertiesTab : public PropertiesTabWidget
{
    Q_OBJECT

public:
    PlaybackPropertiesTab(MusicLibrary* library, TrackList tracks, QWidget* parent = nullptr);

    void apply() override;

    void setSession(PropertiesDialogSession* session) override;
    void setTrackScope(const TrackList& tracks) override;
    void updateTracks(const TrackList& tracks) override;

    [[nodiscard]] bool isAvailableForScope(const TrackList& tracks) const override;
    [[nodiscard]] bool hasPendingScopeChanges() const override;
    bool commitPendingChanges() override;

private:
    struct Changes
    {
        std::optional<QString> exclusion;
        std::optional<bool> disableCrossfading;
    };

    static void applyChanges(Track& track, const Changes& changes);
    void refreshControls();

    MusicLibrary* m_library;
    PropertiesDialogSession* m_session;
    TrackList m_tracks;
    std::map<int, Changes> m_pendingChanges;
    QComboBox* m_exclusion;
    QCheckBox* m_disableCrossfading;
    bool m_exclusionChanged;
    bool m_crossfadingChanged;
};
} // namespace Fooyin
