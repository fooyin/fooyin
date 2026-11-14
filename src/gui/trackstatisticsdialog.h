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

#include <core/library/musiclibrary.h>

#include <QBasicTimer>
#include <QDialog>
#include <QElapsedTimer>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QProgressBar;
class QPushButton;

namespace Fooyin {
class ScriptLineEdit;
class SettingsManager;

class TrackStatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    static void openRebuild(MusicLibrary* library, SettingsManager* settings, QWidget* parent);
    static void openReset(MusicLibrary* library, SettingsManager* settings, const TrackList& tracks, QWidget* parent);

    void reject() override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    TrackStatisticsDialog(MusicLibrary* library, SettingsManager* settings, const TrackList& tracks, bool resetting,
                          QWidget* parent);

    [[nodiscard]] PlaybackStatisticsRequest request(bool preview) const;
    void preview();
    void apply();
    void invalidatePreview();
    void setBusy(bool busy);
    void trackProgress(const QFuture<PlaybackStatisticsResult>& future);
    void updateControls();
    void updateHeight();
    void showResult(const PlaybackStatisticsResult& result);

    MusicLibrary* m_library;
    TrackIds m_trackIds;
    bool m_resetting;
    bool m_previewValid;
    bool m_hasWork;
    bool m_applying;
    bool m_busy;
    bool m_refreshPending;
    int m_affectedTrackCount;
    PlaybackStatisticsConfig m_savedConfig;
    QFuture<PlaybackStatisticsResult> m_progressFuture;
    QBasicTimer m_progressTimer;
    QElapsedTimer m_progressElapsed;

    ScriptLineEdit* m_script;
    QPushButton* m_restoreOriginal;
    QComboBox* m_playcount;
    QComboBox* m_rating;
    QComboBox* m_loved;
    QComboBox* m_split;
    QCheckBox* m_clearFirst;
    QCheckBox* m_clearLast;
    QCheckBox* m_resetHistory;
    QCheckBox* m_resetRating;
    QCheckBox* m_resetLoved;
    QCheckBox* m_writePlaycount;
    QCheckBox* m_writeRating;
    QLabel* m_affectedTracks;
    QLabel* m_statisticsGroups;
    QLabel* m_mergedGroups;
    QLabel* m_splitGroups;
    QLabel* m_fallbackTracks;
    QLabel* m_orphanedGroups;
    QLabel* m_preview;
    QProgressBar* m_progress;
    QPushButton* m_previewButton;
    QPushButton* m_applyButton;
    QDialogButtonBox* m_buttons;
};
} // namespace Fooyin
