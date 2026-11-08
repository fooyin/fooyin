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

#include <core/playhistory/playhistory.h>
#include <core/playhistory/playhistorystats.h>

#include <QDate>
#include <QDialog>

#include <memory>
#include <optional>

class QPushButton;
class QScrollArea;
class QVBoxLayout;

namespace Fooyin {
class StatisticsService;

namespace Stats {
class ChipBar;
}

/*!
 * Charts of listening habits over a chosen period, such as the most played tracks, when music is listened to
 * and how the period compares to the one before it.
 */
class StatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatisticsDialog(StatisticsService* service, QWidget* parent = nullptr);

Q_SIGNALS:
    void yearInReviewRequested(int year);

private:
    enum class Period : uint8_t
    {
        Week = 0,
        Month,
        ThreeMonths,
        SixMonths,
        Year,
        AllTime,
        Custom,
    };

    [[nodiscard]] std::pair<QDate, QDate> resolveRange() const;
    void selectPeriod(Period period);
    void chooseCustomRange();
    void setCustomRange(const QDate& first, const QDate& last);

    void loadSummary();
    void refresh();
    void showStats(const PlayHistoryStats& current, const std::optional<PlayHistoryStats>& previous);
    void showMessage(const QString& message);
    void setContent(QWidget* content);
    void exportImage();

    void addOverview(QVBoxLayout* layout, const PlayHistoryStats& stats,
                     const std::optional<PlayHistoryStats>& previous);
    void addMonths(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addListens(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addDays(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addListeningClock(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addHabits(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addObsession(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addDiscoveries(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addTopTracks(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addMostPlayed(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addTopArtists(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addRediscoveries(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addCompletedAlbums(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addCategories(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addAudioQuality(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addListenTime(QVBoxLayout* layout, const PlayHistoryStats& stats);
    void addDecades(QVBoxLayout* layout, const PlayHistoryStats& stats);

    StatisticsService* m_service;

    Stats::ChipBar* m_periods;
    QPushButton* m_customButton;
    QPushButton* m_yearButton;
    QPushButton* m_exportButton;
    QScrollArea* m_scrollArea;

    PlayHistorySummary m_summary;
    std::shared_ptr<const PlayHistoryStats> m_stats;
    Period m_period;
    QDate m_customFirst;
    QDate m_customLast;
    int m_requestId;

    bool m_cumulative;
    PlayHistoryStats::Category m_category;
};
} // namespace Fooyin
