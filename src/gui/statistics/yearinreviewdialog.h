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

#include <QColor>
#include <QDialog>

#include <optional>
#include <vector>

class QStackedWidget;
class QToolButton;
class QVBoxLayout;

namespace Fooyin {
class StatisticsService;

namespace Stats {
class ChipBar;
class PageDots;
class StoryCard;
} // namespace Stats

/*!
 * A year of listening told as a series of cards, each highlighting something about the year such as the most
 * played tracks, when music was listened to and what was discovered.
 */
class YearInReviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit YearInReviewDialog(StatisticsService* service, QWidget* parent = nullptr);

    //! Shows @p year, or the latest year with listens when it isn't a year in the history.
    void showYear(int year);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct Page
    {
        QWidget* widget{nullptr};
        QColor tint;
    };

    void loadSummary();
    void selectYear(int year);
    void showMessage(const QString& message);
    void clearPages();
    void addPage(QWidget* content);
    void buildPages(int year, const PlayHistoryStats& stats, const std::optional<PlayHistoryStats>& previous);
    void goToPage(int index);
    void exportCurrent();
    void exportAll();
    [[nodiscard]] QPixmap grabPage(int index);

    QWidget* introPage(int year, const PlayHistoryStats& stats);
    QWidget* topTracksPage(const PlayHistoryStats& stats);
    QWidget* topArtistsPage(const PlayHistoryStats& stats);
    QWidget* genresPage(const PlayHistoryStats& stats);
    QWidget* clockPage(const PlayHistoryStats& stats);
    QWidget* daysPage(const PlayHistoryStats& stats);
    QWidget* discoveriesPage(const PlayHistoryStats& stats);
    QWidget* songsOfTheYearPage(const PlayHistoryStats& stats);
    QWidget* obsessionPage(const PlayHistoryStats& stats);
    QWidget* habitsPage(const PlayHistoryStats& stats);
    QWidget* summaryPage(int year, const PlayHistoryStats& stats, const std::optional<PlayHistoryStats>& previous);

    StatisticsService* m_service;

    Stats::ChipBar* m_years;
    QToolButton* m_exportButton;
    Stats::StoryCard* m_card;
    QStackedWidget* m_stack;
    Stats::PageDots* m_dots;
    QToolButton* m_previousButton;
    QToolButton* m_nextButton;

    PlayHistorySummary m_summary;
    std::vector<int> m_yearOrder;
    std::vector<Page> m_pages;
    int m_year;
    int m_pendingYear;
    int m_requestId;
    int m_hue;
};
} // namespace Fooyin
