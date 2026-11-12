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

#include <core/library/librarystatistics.h>
#include <core/player/playbackhistorydefs.h>

#include <QBasicTimer>
#include <QDate>
#include <QDialog>

#include <cstdint>
#include <unordered_map>
#include <vector>

class QComboBox;
class QAction;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableView;
class QTimer;
class QTreeView;
class QModelIndex;

namespace Fooyin {
class MusicLibrary;
class LibraryBreakdownModel;
class ListeningHistoryModel;
class PlaybackHistoryService;
class ScriptLineEdit;
class SettingsManager;
struct PlaybackHistoryOverview;
struct PlaybackHistoryPage;
struct PlaybackSourceRanking;
struct StreamMetadataSpan;

class StatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatisticsDialog(PlaybackHistoryService* historyService, MusicLibrary* library, SettingsManager* settings,
                              QWidget* parent = nullptr);
    ~StatisticsDialog() override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void refresh(bool library = false);
    void updateLiveProgress();
    void updateHistoryEvent(qint64 eventId);
    void updateHistoryStatus();
    void updateLibraryStatistics(const LibraryStatistics& statistics);
    void updateLibraryBreakdown();
    void updateCustomBreakdown();
    void saveBreakdownAs();
    void storeSavedBreakdowns();
    void updateBreakdownActions();
    void displayLibraryBreakdown(const std::vector<LibraryGroupStatistics>& groups);
    void updateListeningOverview(const PlaybackHistoryOverview& overview);
    void updateFooterStatus();
    void updateRankings(const std::vector<PlaybackSourceRanking>& rankings);
    void refreshHistory(bool reset);
    void appendHistoryPage(const PlaybackHistoryPage& page);
    void loadMetadataSpans(const QModelIndex& eventIndex);

    PlaybackHistoryService* m_historyService;
    QComboBox* m_scope;
    QLabel* m_trackCount;
    QLabel* m_totalDuration;
    QLabel* m_totalSize;
    QLabel* m_totalPlayCount;
    QLabel* m_averageRating;
    QLabel* m_unratedTrackCount;
    QLabel* m_trackingSince;
    QString m_trackingSinceText;
    QString m_breakdownError;
    QComboBox* m_breakdown;
    ScriptLineEdit* m_breakdownScript;
    QAction* m_saveBreakdown;
    QAction* m_breakdownActions;
    QAction* m_updateBreakdown;
    QAction* m_deleteBreakdown;
    bool m_breakdownLoading;
    QBasicTimer m_breakdownTimer;
    LibraryBreakdownModel* m_breakdownModel;
    QTableView* m_libraryBreakdown;
    LibraryStatistics m_libraryStatistics;
    QTableWidget* m_periods;
    QTableWidget* m_rankings;
    QComboBox* m_historyScope;
    QLineEdit* m_historyFilter;
    ListeningHistoryModel* m_historyModel;
    QTreeView* m_historyTree;
    QLabel* m_historyStatus;
    QPushButton* m_loadMore;
    QTimer* m_historyFilterTimer;
    uint64_t m_requestToken;
    uint64_t m_breakdownRequestToken;
    uint64_t m_historyRequestToken;
    uint64_t m_historyTotal;
    uint64_t m_historyGeneration;
    bool m_totalsOnly;
    bool m_refreshPending;
    bool m_libraryDirty;
    bool m_libraryPending;
    QBasicTimer m_liveTimer;
    QDate m_overviewDay;
    PlaybackHistoryOverview m_overviewBaseline;
    std::optional<PlaybackEvent> m_liveSnapshot;
    std::optional<PlaybackEvent> m_rankingSnapshot;
    std::vector<PlaybackSourceRanking> m_rankingBaseline;
    std::optional<std::pair<qint64, qint64>> m_historyCursor;
    std::unordered_map<qint64, uint64_t> m_eventRequests;
};
} // namespace Fooyin
