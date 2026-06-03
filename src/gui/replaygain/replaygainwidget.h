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

#pragma once

#include <gui/propertiesdialog.h>

#include <QByteArray>

class QContextMenuEvent;

namespace Fooyin {
class MusicLibrary;
class ReplayGainModel;
class ReplayGainView;

class ReplayGainWidget : public PropertiesTabWidget
{
    Q_OBJECT

public:
    ReplayGainWidget(MusicLibrary* library, const TrackList& tracks, bool readOnly, SettingsManager* settings,
                     QWidget* parent = nullptr);
    ~ReplayGainWidget() override;

    void apply() override;
    void updateTracks(const TrackList& tracks) override;
    void setTrackScope(const TrackList& tracks) override;
    bool commitPendingChanges() override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void finalise();
    void saveLayoutData() const;
    void loadLayoutData();
    void updateHeaderModes() const;

    MusicLibrary* m_library;
    SettingsManager* m_settings;

    ReplayGainView* m_view;
    ReplayGainModel* m_model;
    OpusRGWriteMode m_opusWriteMode;

    TrackList m_pendingTracks;
    bool m_showHeader;
    bool m_alternatingColours;
};
} // namespace Fooyin
