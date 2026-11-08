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

#include "statisticsdialog.h"

#include "statisticsservice.h"
#include "statscharts.h"
#include "statswidgets.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
using namespace Stats;
using Category = PlayHistoryStats::Category;

//! Number of categories drawn individually in a donut chart, the rest being combined
constexpr size_t DonutSlices = 8;

QString tr(const char* text)
{
    return QCoreApplication::translate("Fooyin::StatisticsDialog", text);
}

QString number(qint64 value)
{
    return QLocale().toString(value);
}

QString daysText(int days)
{
    return days == 1 ? tr("%1 day").arg(days) : tr("%1 days").arg(number(days));
}

QString categoryName(Category category)
{
    switch(category) {
        case Category::Genre:
            return tr("Genre");
        case Category::Artist:
            return tr("Artist");
        case Category::AlbumArtist:
            return tr("Album artist");
        case Category::Composer:
            return tr("Composer");
        case Category::Album:
            return tr("Album");
    }
    return {};
}

QString categoryCountText(Category category, int count)
{
    const QString value = number(count);
    switch(category) {
        case Category::Genre:
            return tr("%1 genres").arg(value);
        case Category::Artist:
            return tr("%1 artists").arg(value);
        case Category::AlbumArtist:
            return tr("%1 album artists").arg(value);
        case Category::Composer:
            return tr("%1 composers").arg(value);
        case Category::Album:
            return tr("%1 albums").arg(value);
    }
    return {};
}

RowListWidget::Row trackRow(const PlayHistoryStatsTrack& track, const QString& value, const QString& trailing = {})
{
    QString subtitle = track.artist;
    if(!track.album.isEmpty()) {
        subtitle = subtitle.isEmpty() ? track.album : subtitle + u" — "_s + track.album;
    }

    return {.title    = track.title.isEmpty() ? tr("Unknown title") : track.title,
            .subtitle = subtitle,
            .value    = value,
            .trailing = trailing};
}

QDate dateOf(qint64 timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp).date();
}

QLabel* captionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font  = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setContentsMargins(6, 4, 0, 0);
    return label;
}

QGridLayout* tileGrid(const std::vector<MiniTile*>& tiles, int columns = 3)
{
    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    for(size_t i{0}; i < tiles.size(); ++i) {
        grid->addWidget(tiles[i], static_cast<int>(i) / columns, static_cast<int>(i) % columns);
    }
    for(int column{0}; column < columns; ++column) {
        grid->setColumnStretch(column, 1);
    }
    return grid;
}

//! A list that starts out short and can be expanded to show the rest.
QWidget* expandableRows(std::vector<RowListWidget::Row> rows, int initial, int step, QWidget* parent,
                        bool showRank = true)
{
    auto* container = new QWidget(parent);
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* list = new RowListWidget(container);
    auto* more = new ShowMoreButton(static_cast<int>(rows.size()), initial, step, container);
    auto all   = std::make_shared<std::vector<RowListWidget::Row>>(std::move(rows));

    const auto apply = [list, all, showRank](int shown) {
        const auto count = static_cast<std::ptrdiff_t>(std::min<size_t>(static_cast<size_t>(shown), all->size()));
        list->setRows({all->begin(), all->begin() + count}, showRank);
    };
    apply(more->shown());
    QObject::connect(more, &ShowMoreButton::shownChanged, list, apply);

    layout->addWidget(list);
    layout->addWidget(more);
    return container;
}

QString minuteRange(const PlayHistoryStatsSession& session)
{
    const QLocale locale;
    return u"%1 → %2"_s.arg(
        locale.toString(QDateTime::fromMSecsSinceEpoch(session.startMs).time(), QLocale::ShortFormat),
        locale.toString(QDateTime::fromMSecsSinceEpoch(session.endMs).time(), QLocale::ShortFormat));
}
} // namespace

StatisticsDialog::StatisticsDialog(StatisticsService* service, QWidget* parent)
    : QDialog{parent}
    , m_service{service}
    , m_periods{new ChipBar(this)}
    , m_customButton{new QPushButton(tr("Custom range…"), this)}
    , m_yearButton{new QPushButton(this)}
    , m_exportButton{new QPushButton(tr("Export image…"), this)}
    , m_scrollArea{new QScrollArea(this)}
    , m_period{Period::AllTime}
    , m_requestId{0}
    , m_cumulative{true}
    , m_category{Category::Genre}
{
    setWindowTitle(tr("Statistics"));
    resize(820, 880);

    auto* layout = new QVBoxLayout(this);

    auto* title     = new QLabel(tr("Listening statistics"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.4);
    title->setFont(titleFont);

    m_yearButton->setVisible(false);
    m_exportButton->setEnabled(false);

    auto* header = new QHBoxLayout();
    header->addWidget(title, 1);
    header->addWidget(m_yearButton);
    header->addWidget(m_exportButton);
    layout->addLayout(header);

    m_customButton->setCheckable(true);
    m_periods->setItems({tr("7 days"), tr("30 days"), tr("3 months"), tr("6 months"), tr("1 year"), tr("All time")});
    m_periods->setCurrentIndex(static_cast<int>(Period::AllTime));

    auto* periods = new QHBoxLayout();
    periods->addWidget(m_customButton);
    periods->addWidget(m_periods, 1);
    layout->addLayout(periods);

    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_scrollArea, 1);

    QObject::connect(m_periods, &ChipBar::indexChanged, this,
                     [this](int index) { selectPeriod(static_cast<Period>(index)); });
    QObject::connect(m_customButton, &QPushButton::clicked, this, &StatisticsDialog::chooseCustomRange);
    QObject::connect(m_yearButton, &QPushButton::clicked, this, [this]() {
        if(!m_summary.years.empty()) {
            Q_EMIT yearInReviewRequested(m_summary.years.back());
        }
    });
    QObject::connect(m_exportButton, &QPushButton::clicked, this, &StatisticsDialog::exportImage);

    loadSummary();
}

std::pair<QDate, QDate> StatisticsDialog::resolveRange() const
{
    const QDate today = QDate::currentDate();

    switch(m_period) {
        case Period::Week:
            return {today.addDays(-6), today};
        case Period::Month:
            return {today.addDays(-29), today};
        case Period::ThreeMonths:
            return {today.addDays(-89), today};
        case Period::SixMonths:
            return {today.addDays(-181), today};
        case Period::Year:
            return {today.addDays(-364), today};
        case Period::AllTime: {
            const QDate oldest = m_summary.isEmpty() ? today : dateOf(m_summary.oldestTimestamp);
            return {oldest, today};
        }
        case Period::Custom:
            return {m_customFirst, m_customLast};
    }
    return {today, today};
}

void StatisticsDialog::selectPeriod(Period period)
{
    m_period = period;
    m_periods->setCurrentIndex(period == Period::Custom ? -1 : static_cast<int>(period));
    m_customButton->setChecked(period == Period::Custom);
    if(period != Period::Custom) {
        m_customButton->setText(tr("Custom range…"));
    }
    refresh();
}

void StatisticsDialog::chooseCustomRange()
{
    const QDate today  = QDate::currentDate();
    const QDate oldest = m_summary.isEmpty() ? today : dateOf(m_summary.oldestTimestamp);

    QDialog dialog{this};
    dialog.setWindowTitle(tr("Choose a date range"));

    auto* from = new QDateEdit(m_customFirst.isValid() ? m_customFirst : today.addDays(-29), &dialog);
    auto* to   = new QDateEdit(m_customLast.isValid() ? m_customLast : today, &dialog);
    for(auto* edit : {from, to}) {
        edit->setCalendarPopup(true);
        edit->setDateRange(oldest, today);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto* form = new QFormLayout(&dialog);
    form->addRow(tr("From"), from);
    form->addRow(tr("To"), to);
    form->addRow(buttons);

    if(dialog.exec() == QDialog::Accepted) {
        setCustomRange(std::min(from->date(), to->date()), std::max(from->date(), to->date()));
    }
    else {
        m_customButton->setChecked(m_period == Period::Custom);
    }
}

void StatisticsDialog::setCustomRange(const QDate& first, const QDate& last)
{
    m_customFirst = first;
    m_customLast  = last;
    selectPeriod(Period::Custom);
    m_customButton->setText(formatRange(first, last));
}

void StatisticsDialog::loadSummary()
{
    m_service->refreshLibraryInfo();
    showMessage(tr("Loading…"));

    m_service->summary().then(this, [this](const PlayHistorySummary& summary) {
        m_summary = summary;
        m_yearButton->setVisible(!summary.years.empty());
        if(!summary.years.empty()) {
            m_yearButton->setText(tr("Your %1").arg(summary.years.back()));
        }
        refresh();
    });
}

void StatisticsDialog::refresh()
{
    const int id = ++m_requestId;

    if(m_summary.isEmpty()) {
        m_stats.reset();
        m_exportButton->setEnabled(false);
        showMessage(tr("There's no listening history yet.\n\nFooyin now records every track you listen to, so "
                       "statistics will appear here as you play music."));
        return;
    }

    const auto [first, last] = resolveRange();

    m_service->compute(first, last).then(this, [this, id, first, last](const PlayHistoryStats& current) {
        if(id != m_requestId) {
            return;
        }

        if(m_period == Period::AllTime) {
            showStats(current, std::nullopt);
            return;
        }

        // Compare against the period of the same length directly before it
        const qint64 length = first.daysTo(last) + 1;
        m_service->compute(first.addDays(-length), first.addDays(-1))
            .then(this, [this, id, current](const PlayHistoryStats& previous) {
                if(id == m_requestId) {
                    showStats(current, previous);
                }
            });
    });
}

void StatisticsDialog::showMessage(const QString& message)
{
    setContent(createEmptyHint(message));
}

void StatisticsDialog::setContent(QWidget* content)
{
    // The scroll area deletes the previous content
    m_scrollArea->setWidget(content);
}

void StatisticsDialog::showStats(const PlayHistoryStats& current, const std::optional<PlayHistoryStats>& previous)
{
    m_stats                       = std::make_shared<const PlayHistoryStats>(current);
    const PlayHistoryStats& stats = *m_stats;

    if(stats.isEmpty()) {
        m_exportButton->setEnabled(false);
        showMessage(tr("No listens in this period."));
        return;
    }

    auto* content = new QWidget();
    auto* layout  = new QVBoxLayout(content);
    layout->setContentsMargins(0, 4, 8, 12);
    layout->setSpacing(12);

    addOverview(layout, stats, previous);
    if(stats.totalDays() > 31) {
        addMonths(layout, stats);
    }
    addListens(layout, stats);
    addDays(layout, stats);
    addListeningClock(layout, stats);
    addHabits(layout, stats);
    if(!stats.obsessions.empty() || stats.longestSession) {
        addObsession(layout, stats);
    }
    addDiscoveries(layout, stats);
    addTopTracks(layout, stats);
    addMostPlayed(layout, stats);
    if(!stats.category(Category::Artist).top.empty()) {
        addTopArtists(layout, stats);
    }
    if(!stats.rediscoveries.empty()) {
        addRediscoveries(layout, stats);
    }
    if(!stats.completedAlbums.empty()) {
        addCompletedAlbums(layout, stats);
    }
    addCategories(layout, stats);
    if(stats.losslessListens + stats.lossyListens > 0
       || std::ranges::any_of(stats.bitrateBuckets, [](int count) { return count > 0; })) {
        addAudioQuality(layout, stats);
    }
    addListenTime(layout, stats);
    if(!stats.decades.empty()) {
        addDecades(layout, stats);
    }
    layout->addStretch();

    setContent(content);
    m_exportButton->setEnabled(true);
}

void StatisticsDialog::exportImage()
{
    QWidget* content = m_scrollArea->widget();
    if(!content) {
        return;
    }

    const QString defaultPath
        = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + u"/fooyin-statistics.png"_s;
    const QString path = QFileDialog::getSaveFileName(this, tr("Export image"), defaultPath, tr("PNG images (*.png)"));
    if(path.isEmpty()) {
        return;
    }

    // Charts grow in when first shown, so settle them before taking the picture
    const auto charts = content->findChildren<ChartWidget*>();
    for(auto* chart : charts) {
        chart->skipReveal();
    }

    content->grab().save(path, "PNG");
}

void StatisticsDialog::addOverview(QVBoxLayout* layout, const PlayHistoryStats& stats,
                                   const std::optional<PlayHistoryStats>& previous)
{
    auto* card = new StatsCard({});

    const bool hasPrevious = previous && !previous->isEmpty();

    auto* listens = new MiniTile(tr("Total listens"), number(stats.totalListens));
    auto* time    = new MiniTile(tr("Total listen time"), formatListenTime(stats.listenSeconds));
    auto* tracks  = new MiniTile(tr("Tracks"), number(stats.uniqueTracks));
    auto* artists = new MiniTile(tr("Artists"), number(stats.uniqueArtists));
    auto* days   = new MiniTile(tr("Days"), u"%1 / %2"_s.arg(number(stats.daysWithListens), number(stats.totalDays())));
    auto* streak = new MiniTile(tr("Streak"), daysText(stats.currentStreak));

    if(hasPrevious) {
        listens->setDelta(stats.totalListens, previous->totalListens);
        time->setDelta(stats.listenSeconds, previous->listenSeconds);
        tracks->setDelta(stats.uniqueTracks, previous->uniqueTracks);
        artists->setDelta(stats.uniqueArtists, previous->uniqueArtists);
        days->setDelta(stats.daysWithListens, previous->daysWithListens);
    }

    card->addLayout(tileGrid({listens, time, tracks, artists, days, streak}));

    if(hasPrevious) {
        auto* caption
            = new QLabel(tr("Compared with %1").arg(formatRange(previous->firstDay, previous->lastDay)), card);
        QPalette palette = caption->palette();
        palette.setColor(QPalette::WindowText, Colours::fromPalette(palette).textMuted);
        caption->setPalette(palette);
        card->addWidget(caption);
    }

    layout->addWidget(card);
}

void StatisticsDialog::addMonths(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    const QLocale locale;
    std::vector<ChartData> points;
    for(int month{0}; month < 12; ++month) {
        points.push_back({locale.monthName(month + 1, QLocale::ShortFormat), stats.months[static_cast<size_t>(month)]});
    }

    auto* card
        = new StatsCard(tr("Months"), stats.totalDays() > 366 ? tr("All time") : QString::number(stats.lastDay.year()));
    auto* chart = new VerticalBarsChart(card);
    chart->setPlotHeight(110);
    chart->setClickable(true);
    chart->setData(std::move(points));
    card->addWidget(chart);

    // Selecting a month narrows the statistics down to it
    QObject::connect(chart, &VerticalBarsChart::barClicked, this,
                     [this, lastDay = stats.lastDay, firstDay = stats.firstDay](int month) {
                         int year = lastDay.year();
                         if(month + 1 > lastDay.month()) {
                             --year;
                         }
                         const QDate start{year, month + 1, 1};
                         const QDate end = start.addMonths(1).addDays(-1);
                         if(end < firstDay) {
                             return;
                         }
                         setCustomRange(start, std::min(end, QDate::currentDate()));
                     });

    layout->addWidget(card);
}

void StatisticsDialog::addListens(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    auto* card = new StatsCard(tr("Listens"), formatRange(stats.firstDay, stats.lastDay));

    auto* chart = new ListensLineChart(card);
    chart->setData(stats.listensPerDay, stats.firstDay, m_cumulative);

    auto* toggle = new ChipBar(card);
    toggle->setItems({tr("Total"), tr("Daily")});
    toggle->setCurrentIndex(m_cumulative ? 0 : 1);
    QObject::connect(toggle, &ChipBar::indexChanged, chart, [this, chart](int index) {
        m_cumulative = index == 0;
        chart->setData(m_stats->listensPerDay, m_stats->firstDay, m_cumulative);
    });

    card->addHeaderWidget(toggle);
    card->addWidget(chart);
    layout->addWidget(card);
}

void StatisticsDialog::addDays(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    QString subtitle;
    if(stats.longestStreak) {
        subtitle
            = tr("Longest streak: %1 • Busiest day: %2 (%3)")
                  .arg(daysText(stats.longestStreak->days), formatDay(stats.peakDay), number(stats.peakDayListens));
    }

    auto* card  = new StatsCard(tr("Days"), subtitle);
    auto* chart = new CalendarHeatmapChart(card);
    chart->setData(stats.listensPerDay, stats.firstDay);
    card->addWidget(chart);
    layout->addWidget(card);
}

void StatisticsDialog::addListeningClock(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    const QLocale locale;
    // Sunday first, matching the data
    QStringList weekdayNames;
    std::vector<ChartData> weekdays;
    for(int i{0}; i < 7; ++i) {
        const QString name = locale.dayName(i == 0 ? 7 : i, QLocale::ShortFormat);
        weekdayNames.push_back(name);
        weekdays.push_back({name, stats.weekdays[static_cast<size_t>(i)]});
    }

    auto* card = new StatsCard(tr("Listening clock"));

    auto* clock = new ListeningClockChart(card);
    clock->setClockSize(200);
    clock->setData(stats.hours);
    clock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* bars = new VerticalBarsChart(card);
    bars->setPlotHeight(150);
    bars->setData(weekdays);

    auto* top = new QHBoxLayout();
    top->setSpacing(16);
    top->addWidget(clock);
    top->addWidget(bars, 1, Qt::AlignVCenter);
    card->addLayout(top);

    QStringList hourNames;
    for(int hour{0}; hour < 24; ++hour) {
        hourNames.push_back(u"%1"_s.arg(hour, 2, 10, QLatin1Char{'0'}));
    }

    auto* grid = new GridHeatmapChart(card);
    grid->setData({stats.weekHours.begin(), stats.weekHours.end()}, 7, 24);
    grid->setLabels(weekdayNames, hourNames, 3);
    grid->setTipFormatter([weekdayNames](int row, int column, int value) {
        return u"%1 %2:00\n%3"_s.arg(weekdayNames.at(row)).arg(column, 2, 10, QLatin1Char{'0'}).arg(listensText(value));
    });
    card->addWidget(grid);

    layout->addWidget(card);
}

void StatisticsDialog::addHabits(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    const int repeatPercent = stats.totalListens <= 1
                                ? 0
                                : static_cast<int>(std::lround(stats.repeatListens * 100.0 / (stats.totalListens - 1)));
    const auto& library     = m_service->libraryInfo();

    auto* card = new StatsCard(tr("Habits"));

    std::vector<MiniTile*> tiles;
    tiles.push_back(new MiniTile(tr("Repeat rate"), u"%1%"_s.arg(repeatPercent)));
    tiles.push_back(new MiniTile(tr("Played once"), tr("%1 tracks").arg(number(stats.oneTimeTracks))));
    tiles.push_back(new MiniTile(tr("Played %1+ times").arg(PlayHistoryStats::HeavyRotationListens),
                                 tr("%1 tracks").arg(number(stats.heavyRotationTracks))));
    if(library.trackCount > 0) {
        const int reach = static_cast<int>(std::lround(stats.uniqueTracks * 100.0 / library.trackCount));
        tiles.push_back(new MiniTile(tr("Library reach"), u"%1%"_s.arg(reach)));
        tiles.push_back(new MiniTile(tr("Never played"), tr("%1 tracks").arg(number(library.neverPlayed))));
    }
    if(stats.avgFirstMinute >= 0) {
        tiles.push_back(new MiniTile(tr("First listen"), formatMinuteOfDay(stats.avgFirstMinute)));
    }
    if(stats.avgLastMinute >= 0) {
        tiles.push_back(new MiniTile(tr("Last listen"), formatMinuteOfDay(stats.avgLastMinute)));
    }
    card->addLayout(tileGrid(tiles));

    if(const auto& streak = stats.longestTrackStreak) {
        const QDate end = dateOf(streak->timestamp);
        card->addWidget(
            captionLabel(tr("Streak • %1 • %2 → %3")
                             .arg(daysText(streak->days), formatDay(end.addDays(1 - streak->days)), formatDay(end)),
                         card));

        auto* list = new RowListWidget(card);
        list->setRows({trackRow(*streak, number(streak->count))}, false);
        card->addWidget(list);
    }

    layout->addWidget(card);
}

void StatisticsDialog::addObsession(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    auto* card = new StatsCard(tr("Obsession"), stats.obsessions.empty() ? QString{} : tr("Most played • day"));

    if(!stats.obsessions.empty()) {
        std::vector<RowListWidget::Row> rows;
        for(const auto& obsession : stats.obsessions) {
            rows.push_back(
                trackRow(obsession, number(obsession.count) + u"×"_s, formatDay(dateOf(obsession.timestamp))));
        }
        card->addWidget(expandableRows(std::move(rows), 5, 5, card, false));
    }

    if(const auto& session = stats.longestSession) {
        card->addWidget(captionLabel(tr("Longest session • %1").arg(formatDay(dateOf(session->startMs))), card));
        card->addLayout(tileGrid({new MiniTile(minuteRange(*session), formatDuration(session->durationMs())),
                                  new MiniTile(tr("Listens"), number(session->listens))},
                                 3));
    }

    layout->addWidget(card);
}

void StatisticsDialog::addDiscoveries(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    auto* card
        = new StatsCard(tr("Discoveries"), tr("First listen • %1").arg(formatRange(stats.firstDay, stats.lastDay)));

    card->addLayout(tileGrid(
        {new MiniTile(tr("Tracks"), number(stats.newTracks)), new MiniTile(tr("Artists"), number(stats.newArtists))}));

    if(!stats.newTracksTop.empty()) {
        std::vector<RowListWidget::Row> rows;
        for(const auto& track : stats.newTracksTop) {
            rows.push_back(trackRow(track, number(track.count), formatDay(dateOf(track.timestamp))));
        }
        card->addWidget(expandableRows(std::move(rows), 5, 5, card, false));
    }

    if(!stats.newArtistsTop.empty()) {
        card->addWidget(captionLabel(tr("Top artists • First listen"), card));

        std::vector<MiniTile*> tiles;
        for(const auto& artist : stats.newArtistsTop) {
            tiles.push_back(new MiniTile(listensText(artist.count), artist.key));
        }
        card->addLayout(tileGrid(tiles));
    }

    if(const auto& favourite = stats.oldestFavourite) {
        card->addWidget(captionLabel(tr("Oldest favourite • %1").arg(formatDay(dateOf(favourite->timestamp))), card));

        auto* list = new RowListWidget(card);
        list->setRows({trackRow(*favourite, number(favourite->count))}, false);
        card->addWidget(list);
    }

    layout->addWidget(card);
}

void StatisticsDialog::addTopTracks(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    auto* card = new StatsCard(tr("Top tracks"));

    std::vector<RowListWidget::Row> rows;
    for(const auto& track : stats.topTracks) {
        rows.push_back(trackRow(track, number(track.count)));
    }
    card->addWidget(expandableRows(std::move(rows), 10, 10, card));

    layout->addWidget(card);
}

void StatisticsDialog::addMostPlayed(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    std::vector<RowListWidget::Row> rows;
    for(const Category category : {Category::Artist, Category::Album, Category::Genre}) {
        const auto& top = stats.category(category).top;
        if(!top.empty()) {
            rows.push_back({.title    = top.front().key,
                            .subtitle = categoryName(category),
                            .value    = number(top.front().count),
                            .trailing = {}});
        }
    }
    if(rows.empty()) {
        return;
    }

    auto* card = new StatsCard(tr("Most played"));
    auto* list = new RowListWidget(card);
    list->setRows(std::move(rows), false);
    card->addWidget(list);
    layout->addWidget(card);
}

void StatisticsDialog::addTopArtists(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    const auto& artists = stats.category(Category::Artist).top;

    auto* card = new StatsCard(tr("Top artists"));

    auto* container = new QWidget(card);
    auto* box       = new QVBoxLayout(container);
    box->setContentsMargins(0, 0, 0, 0);

    auto* chart = new RankBarsChart(container);
    auto* more  = new ShowMoreButton(static_cast<int>(artists.size()), 10, 10, container);

    const auto apply = [chart, &artists](int shown) {
        std::vector<ChartData> points;
        for(size_t i{0}; i < std::min<size_t>(static_cast<size_t>(shown), artists.size()); ++i) {
            points.push_back({artists[i].key, artists[i].count});
        }
        chart->setData(std::move(points));
    };
    apply(more->shown());
    // The statistics outlive the card as they're only replaced along with it
    QObject::connect(more, &ShowMoreButton::shownChanged, chart, apply);

    box->addWidget(chart);
    box->addWidget(more);
    card->addWidget(container);
    layout->addWidget(card);
}

void StatisticsDialog::addRediscoveries(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    auto* card = new StatsCard(tr("Rediscoveries"),
                               tr("Last listen over %1 ago").arg(daysText(PlayHistoryStats::RediscoveryGapDays)));

    std::vector<RowListWidget::Row> rows;
    for(const auto& track : stats.rediscoveries) {
        rows.push_back(trackRow(track, number(track.count), daysText(track.days)));
    }

    auto* list = new RowListWidget(card);
    list->setRows(std::move(rows), false);
    card->addWidget(list);
    layout->addWidget(card);
}

void StatisticsDialog::addCompletedAlbums(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    auto* card = new StatsCard(tr("Completed albums"),
                               categoryCountText(Category::Album, static_cast<int>(stats.completedAlbums.size())));

    std::vector<RowListWidget::Row> rows;
    for(const auto& album : stats.completedAlbums) {
        rows.push_back({.title    = album.name,
                        .subtitle = tr("%1 tracks").arg(album.tracks),
                        .value    = number(album.listens),
                        .trailing = {}});
    }
    card->addWidget(expandableRows(std::move(rows), 5, 5, card, false));
    layout->addWidget(card);
}

void StatisticsDialog::addCategories(QVBoxLayout* layout, const PlayHistoryStats& /*stats*/)
{
    auto* card = new StatsCard(tr("Listens by"));

    auto* selector = new QComboBox(card);
    for(const Category category :
        {Category::Genre, Category::Artist, Category::AlbumArtist, Category::Composer, Category::Album}) {
        selector->addItem(categoryName(category), static_cast<int>(category));
        if(category == m_category) {
            selector->setCurrentIndex(selector->count() - 1);
        }
    }

    auto* donut = new DonutChart(card);
    donut->setCentreLabel(tr("Listens"));

    const auto update = [this, card, donut](Category category) {
        const auto& rank = m_stats->category(category);

        std::vector<ChartData> points;
        int other = rank.otherListens;
        for(size_t i{0}; i < rank.top.size(); ++i) {
            if(i < DonutSlices) {
                points.push_back({rank.top[i].key, rank.top[i].count});
            }
            else {
                other += rank.top[i].count;
            }
        }

        card->setSubtitle(categoryCountText(category, rank.unique));
        donut->setVisible(!points.empty());
        donut->setData(std::move(points), other);
    };
    update(m_category);

    QObject::connect(selector, &QComboBox::currentIndexChanged, donut, [this, selector, update]() {
        m_category = static_cast<Category>(selector->currentData().toInt());
        update(m_category);
    });

    card->addHeaderWidget(selector);
    card->addWidget(donut);
    layout->addWidget(card);
}

void StatisticsDialog::addAudioQuality(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    const int total = stats.losslessListens + stats.lossyListens;

    QString subtitle;
    if(total > 0) {
        subtitle = tr("Lossless: %1%").arg(static_cast<int>(std::lround(stats.losslessListens * 100.0 / total)));
    }
    auto* card = new StatsCard(tr("Audio"), subtitle);

    const QStringList names{tr("< 128 kbps"), tr("128 – 255 kbps"), tr("256 – 319 kbps"), tr("320+ kbps")};
    std::vector<ChartData> points;
    for(size_t i{0}; i < stats.bitrateBuckets.size(); ++i) {
        if(stats.bitrateBuckets[i] > 0) {
            points.push_back({names.at(static_cast<qsizetype>(i)), stats.bitrateBuckets[i]});
        }
    }

    if(!points.empty()) {
        auto* donut = new DonutChart(card);
        donut->setCentreLabel(tr("Bitrate"));
        donut->setData(std::move(points));
        card->addWidget(donut);
    }
    if(total > 0) {
        card->addLayout(tileGrid({new MiniTile(tr("Lossless"), number(stats.losslessListens)),
                                  new MiniTile(tr("Lossy"), number(stats.lossyListens))}));
    }

    layout->addWidget(card);
}

void StatisticsDialog::addListenTime(QVBoxLayout* layout, const PlayHistoryStats& /*stats*/)
{
    const qint64 listenMs  = m_summary.totalListenMs;
    const qint64 libraryMs = m_service->libraryInfo().durationMs;
    const double fraction  = libraryMs > 0 ? static_cast<double>(listenMs) / static_cast<double>(libraryMs) : 0.0;

    auto* card = new StatsCard(tr("Total listen time"),
                               tr("Total track duration: %1").arg(formatListenTime(libraryMs / 1000)));

    auto* gauge = new GaugeChart(card);
    gauge->setValue(fraction, formatListenTime(listenMs / 1000),
                    QLocale().toString(fraction * 100.0, 'f', fraction >= 10 ? 0 : 1) + u"%"_s);

    auto* centred = new QHBoxLayout();
    centred->addStretch();
    centred->addWidget(gauge);
    centred->addStretch();
    card->addLayout(centred);

    layout->addWidget(card);
}

void StatisticsDialog::addDecades(QVBoxLayout* layout, const PlayHistoryStats& stats)
{
    std::vector<ChartData> points;
    for(const auto& decade : stats.decades) {
        points.push_back({decade.key + u"s"_s, decade.count});
    }

    auto* card  = new StatsCard(tr("Release decades"));
    auto* chart = new VerticalBarsChart(card);
    chart->setPlotHeight(110);
    chart->setData(std::move(points));
    card->addWidget(chart);
    layout->addWidget(card);
}
} // namespace Fooyin

#include "moc_statisticsdialog.cpp"
