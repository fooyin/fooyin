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

#include <QPointer>

class QDialog;

namespace Fooyin {
class ActionManager;
class SettingsManager;

namespace TagEditor {
class TagEditorEditor;
class TagEditorFieldRegistry;
struct FillValuesResult;

class TagEditorPropertiesTab : public PropertiesTabWidget
{
    Q_OBJECT

public:
    explicit TagEditorPropertiesTab(ActionManager* actionManager, TagEditorFieldRegistry* registry,
                                    SettingsManager* settings, QWidget* parent = nullptr);
    ~TagEditorPropertiesTab() override;

    void setTracks(const TrackList& tracks);
    void setReadOnly(bool readOnly);

    void setSession(PropertiesDialogSession* session) override;
    void setTrackScope(const TrackList& tracks) override;
    [[nodiscard]] bool hasPendingScopeChanges() const override;
    bool commitPendingChanges() override;
    void apply() override;
    void updateTracks(const TrackList& tracks) override;

Q_SIGNALS:
    void trackMetadataChanged(const Fooyin::TrackList& tracks);
    void trackStatsChanged(const Fooyin::TrackList& tracks);

private:
    void autoFillValues();
    void handleFillDialogAccepted(const FillValuesResult& result);
    TrackList commitCurrentScopeEdits();
    void stageTrackChanges(const TrackList& tracks, bool metadataChanges);
    void refreshTrackScope();
    [[nodiscard]] TrackList activeTracks() const;
    static void mergeTracks(TrackList& destination, const TrackList& source);

    void saveState() const;
    void restoreState() const;

    SettingsManager* m_settings;
    PropertiesDialogSession* m_session;
    TrackList m_tracks;
    TrackList m_pendingTracks;
    int m_activeTrackIndex;
    bool m_hasPendingMetadataChanges;
    bool m_hasPendingStatChanges;
    QPointer<QDialog> m_fillDialog;
    TagEditorEditor* m_editor;
};
} // namespace TagEditor
} // namespace Fooyin
