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

#include "yearinreviewdialog.h"

#include "statisticsservice.h"
#include "statscharts.h"
#include "statswidgets.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPixmap>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
using namespace Stats;
using Category = PlayHistoryStats::Category;

constexpr int CardMaxWidth = 480;

QString tr(const char* text)
{
    return QCoreApplication::translate("Fooyin::YearInReviewDialog", text);
}

QString number(qint64 value)
{
    return QLocale().toString(value);
}

QString daysText(int days)
{
    return days == 1 ? tr("%1 day").arg(days) : tr("%1 days").arg(number(days));
}

QDate dateOf(qint64 timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp).date();
}

//! The contents of a page, which all share the same margins and are centred vertically.
QWidget* newContent(QVBoxLayout*& layout)
{
    auto* content = new QWidget();
    layout        = new QVBoxLayout(content);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(10);
    layout->addStretch(1);
    return content;
}

void finishContent(QVBoxLayout* layout)
{
    layout->addStretch(1);
}

QLabel* titleLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font  = label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.9);
    label->setFont(font);
    label->setWordWrap(true);
    return label;
}

QLabel* subtitleLabel(const QString& text, QWidget* parent)
{
    auto* label      = new QLabel(text, parent);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, QColor{255, 255, 255, 175});
    label->setPalette(palette);
    label->setWordWrap(true);
    return label;
}

QHBoxLayout* statRow(QWidget* first, QWidget* second)
{
    auto* row = new QHBoxLayout();
    row->setSpacing(16);
    row->addWidget(first, 1);
    row->addWidget(second, 1);
    return row;
}

QString trackSubtitle(const PlayHistoryStatsTrack& track)
{
    if(track.album.isEmpty()) {
        return track.artist;
    }
    return track.artist.isEmpty() ? track.album : track.artist + u" — "_s + track.album;
}

RowListWidget::Row trackRow(const PlayHistoryStatsTrack& track, const QString& value, const QString& trailing = {})
{
    return {.title    = track.title.isEmpty() ? tr("Unknown title") : track.title,
            .subtitle = trackSubtitle(track),
            .value    = value,
            .trailing = trailing};
}

RowListWidget* rowList(std::vector<RowListWidget::Row> rows, QWidget* parent, bool showRank = true)
{
    auto* list = new RowListWidget(parent);
    list->setRowHeight(56);
    list->setRows(std::move(rows), showRank);
    return list;
}
} // namespace

YearInReviewDialog::YearInReviewDialog(StatisticsService* service, QWidget* parent)
    : QDialog{parent}
    , m_service{service}
    , m_years{new ChipBar(this)}
    , m_exportButton{new QToolButton(this)}
    , m_card{new StoryCard(this)}
    , m_stack{new QStackedWidget(m_card)}
    , m_dots{new PageDots(this)}
    , m_previousButton{new QToolButton(this)}
    , m_nextButton{new QToolButton(this)}
    , m_year{0}
    , m_pendingYear{0}
    , m_requestId{0}
    , m_hue{palette().color(QPalette::Highlight).hue()}
{
    setWindowTitle(tr("Year in review"));
    resize(560, 840);

    if(m_hue < 0) {
        m_hue = 210;
    }

    auto* layout = new QVBoxLayout(this);

    auto* exportMenu = new QMenu(m_exportButton);
    exportMenu->addAction(tr("Export this page…"), this, &YearInReviewDialog::exportCurrent);
    exportMenu->addAction(tr("Export all pages…"), this, &YearInReviewDialog::exportAll);
    m_exportButton->setText(tr("Export…"));
    m_exportButton->setMenu(exportMenu);
    m_exportButton->setPopupMode(QToolButton::InstantPopup);
    m_exportButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_exportButton->setEnabled(false);

    auto* header = new QHBoxLayout();
    header->addWidget(m_years, 1);
    header->addWidget(m_exportButton);
    layout->addLayout(header);

    auto* cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->addWidget(m_stack);
    m_card->setMaximumWidth(CardMaxWidth);

    auto* cardRow = new QHBoxLayout();
    cardRow->addStretch(1);
    cardRow->addWidget(m_card, 100);
    cardRow->addStretch(1);
    layout->addLayout(cardRow, 1);

    m_previousButton->setArrowType(Qt::LeftArrow);
    m_nextButton->setArrowType(Qt::RightArrow);
    m_previousButton->setAutoRaise(true);
    m_nextButton->setAutoRaise(true);

    auto* footer = new QHBoxLayout();
    footer->addWidget(m_previousButton);
    footer->addWidget(m_dots, 1);
    footer->addWidget(m_nextButton);
    layout->addLayout(footer);

    QObject::connect(m_years, &ChipBar::indexChanged, this,
                     [this](int index) { selectYear(m_yearOrder.at(static_cast<size_t>(index))); });
    QObject::connect(m_dots, &PageDots::pageClicked, this, &YearInReviewDialog::goToPage);
    QObject::connect(m_previousButton, &QToolButton::clicked, this,
                     [this]() { goToPage(m_stack->currentIndex() - 1); });
    QObject::connect(m_nextButton, &QToolButton::clicked, this, [this]() { goToPage(m_stack->currentIndex() + 1); });

    loadSummary();
}

void YearInReviewDialog::showYear(int year)
{
    if(m_summary.isEmpty()) {
        // Still loading
        m_pendingYear = year;
        return;
    }

    selectYear(std::ranges::find(m_yearOrder, year) != m_yearOrder.end() ? year : m_yearOrder.front());
}

void YearInReviewDialog::keyPressEvent(QKeyEvent* event)
{
    switch(event->key()) {
        case Qt::Key_Left:
            goToPage(m_stack->currentIndex() - 1);
            return;
        case Qt::Key_Right:
            goToPage(m_stack->currentIndex() + 1);
            return;
        default:
            break;
    }
    QDialog::keyPressEvent(event);
}

void YearInReviewDialog::loadSummary()
{
    m_service->refreshLibraryInfo();
    showMessage(tr("Loading…"));

    m_service->summary().then(this, [this](const PlayHistorySummary& summary) {
        m_summary = summary;

        if(summary.isEmpty()) {
            showMessage(tr("There's no listening history yet.\n\nFooyin now records every track you listen to, so "
                           "your year in review will appear here as you play music."));
            return;
        }

        // Newest first
        m_yearOrder.assign(summary.years.rbegin(), summary.years.rend());
        QStringList labels;
        for(const int year : m_yearOrder) {
            labels.push_back(QString::number(year));
        }
        m_years->setItems(labels);

        showYear(m_pendingYear);
    });
}

void YearInReviewDialog::selectYear(int year)
{
    const int id = ++m_requestId;
    m_year       = year;

    const auto index = std::ranges::find(m_yearOrder, year) - m_yearOrder.begin();
    m_years->setCurrentIndex(static_cast<int>(index));

    const QDate first{year, 1, 1};
    const QDate last{year, 12, 31};

    m_service->compute(first, last, true).then(this, [this, id, year](const PlayHistoryStats& current) {
        if(id != m_requestId) {
            return;
        }

        m_service->compute(QDate{year - 1, 1, 1}, QDate{year - 1, 12, 31})
            .then(this, [this, id, year, current](const PlayHistoryStats& previous) {
                if(id == m_requestId) {
                    buildPages(year, current, previous.isEmpty() ? std::nullopt : std::optional{previous});
                }
            });
    });
}

void YearInReviewDialog::showMessage(const QString& message)
{
    clearPages();
    m_exportButton->setEnabled(false);
    m_dots->setCount(0);
    m_previousButton->setEnabled(false);
    m_nextButton->setEnabled(false);

    auto* hint = createEmptyHint(message, m_stack);
    m_stack->addWidget(hint);
    m_card->setTint(QColor::fromHsv(m_hue, 140, 125));
}

void YearInReviewDialog::clearPages()
{
    while(m_stack->count() > 0) {
        QWidget* page = m_stack->widget(0);
        m_stack->removeWidget(page);
        delete page;
    }
    m_pages.clear();
}

void YearInReviewDialog::addPage(QWidget* content)
{
    auto* scroll = new QScrollArea(m_stack);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(content);
    // The card behind is the background
    content->setAutoFillBackground(false);

    const auto index = static_cast<int>(m_pages.size());
    m_stack->addWidget(scroll);
    m_pages.push_back({scroll, QColor::fromHsv((m_hue + index * 24) % 360, 140, 125)});
}

void YearInReviewDialog::buildPages(int year, const PlayHistoryStats& stats,
                                    const std::optional<PlayHistoryStats>& previous)
{
    clearPages();

    if(stats.isEmpty()) {
        showMessage(tr("No listens in %1.").arg(year));
        return;
    }

    addPage(introPage(year, stats));
    if(!stats.topTracks.empty()) {
        addPage(topTracksPage(stats));
    }
    if(!stats.category(Category::Artist).top.empty()) {
        addPage(topArtistsPage(stats));
    }
    if(!stats.category(Category::Genre).top.empty()) {
        addPage(genresPage(stats));
    }
    addPage(clockPage(stats));
    addPage(daysPage(stats));
    if(stats.newTracks > 0 || stats.oldestFavourite) {
        addPage(discoveriesPage(stats));
    }
    if(std::ranges::any_of(stats.monthlyTop, [](const auto& track) { return track.has_value(); })) {
        addPage(songsOfTheYearPage(stats));
    }
    if(!stats.obsessions.empty() || stats.longestSession) {
        addPage(obsessionPage(stats));
    }
    addPage(habitsPage(stats));
    addPage(summaryPage(year, stats, previous));

    m_dots->setCount(static_cast<int>(m_pages.size()));
    m_exportButton->setEnabled(true);
    goToPage(0);
}

void YearInReviewDialog::goToPage(int index)
{
    if(index < 0 || index >= static_cast<int>(m_pages.size())) {
        return;
    }

    m_stack->setCurrentIndex(index);
    m_card->setTint(m_pages[static_cast<size_t>(index)].tint);
    m_dots->setCurrent(index);
    m_previousButton->setEnabled(index > 0);
    m_nextButton->setEnabled(index < static_cast<int>(m_pages.size()) - 1);
}

QPixmap YearInReviewDialog::grabPage(int index)
{
    goToPage(index);

    // Charts grow in when first shown, so settle them before taking the picture
    const auto charts = m_pages.at(static_cast<size_t>(index)).widget->findChildren<ChartWidget*>();
    for(auto* chart : charts) {
        chart->skipReveal();
    }
    return m_card->grab();
}

void YearInReviewDialog::exportCurrent()
{
    const int index = m_stack->currentIndex();
    if(index < 0 || index >= static_cast<int>(m_pages.size())) {
        return;
    }

    const QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                              + u"/fooyin-%1-%2.png"_s.arg(m_year).arg(index + 1);
    const QString path = QFileDialog::getSaveFileName(this, tr("Export image"), defaultPath, tr("PNG images (*.png)"));
    if(!path.isEmpty()) {
        grabPage(index).save(path, "PNG");
    }
}

void YearInReviewDialog::exportAll()
{
    if(m_pages.empty()) {
        return;
    }

    const QString directory = QFileDialog::getExistingDirectory(
        this, tr("Export images"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    if(directory.isEmpty()) {
        return;
    }

    const int original = m_stack->currentIndex();
    for(int i{0}; i < static_cast<int>(m_pages.size()); ++i) {
        grabPage(i).save(directory + u"/fooyin-%1-%2.png"_s.arg(m_year).arg(i + 1), "PNG");
    }
    goToPage(original);
}

QWidget* YearInReviewDialog::introPage(int year, const PlayHistoryStats& stats)
{
    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Your %1").arg(year), content));

    const QLocale locale;
    std::vector<ChartData> months;
    for(int month{0}; month < 12; ++month) {
        months.push_back({locale.monthName(month + 1, QLocale::ShortFormat), stats.months[static_cast<size_t>(month)]});
    }
    auto* chart = new VerticalBarsChart(content);
    chart->setPlotHeight(100);
    chart->setData(std::move(months));
    layout->addWidget(chart);
    layout->addSpacing(8);

    layout->addLayout(statRow(new BigStat(number(stats.totalListens), tr("Total listens"), content),
                              new BigStat(formatListenTime(stats.listenSeconds), tr("Total listen time"), content)));
    layout->addSpacing(8);
    layout->addLayout(statRow(new BigStat(daysText(stats.daysWithListens), tr("Days"), content),
                              new BigStat(number(stats.uniqueTracks), tr("Tracks"), content)));

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::topTracksPage(const PlayHistoryStats& stats)
{
    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Top tracks"), content));
    layout->addSpacing(12);

    std::vector<RowListWidget::Row> rows;
    for(size_t i{0}; i < std::min<size_t>(5, stats.topTracks.size()); ++i) {
        rows.push_back(trackRow(stats.topTracks[i], number(stats.topTracks[i].count)));
    }
    layout->addWidget(rowList(std::move(rows), content));

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::topArtistsPage(const PlayHistoryStats& stats)
{
    const auto& artists = stats.category(Category::Artist);

    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Top artists"), content));
    layout->addWidget(subtitleLabel(tr("%1 artists").arg(number(artists.unique)), content));
    layout->addSpacing(12);

    std::vector<ChartData> points;
    for(size_t i{0}; i < std::min<size_t>(8, artists.top.size()); ++i) {
        points.push_back({artists.top[i].key, artists.top[i].count});
    }
    auto* chart = new RankBarsChart(content);
    chart->setRowHeight(32);
    chart->setData(std::move(points));
    layout->addWidget(chart);

    const auto& albums = stats.category(Category::Album).top;
    const auto& genres = stats.category(Category::Genre).top;
    if(!albums.empty() || !genres.empty()) {
        layout->addSpacing(16);

        auto* row = new QHBoxLayout();
        row->setSpacing(8);
        if(!albums.empty()) {
            row->addWidget(new MiniTile(tr("Album"), albums.front().key, content), 1);
        }
        if(!genres.empty()) {
            row->addWidget(new MiniTile(tr("Genre"), genres.front().key, content), 1);
        }
        layout->addLayout(row);
    }

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::genresPage(const PlayHistoryStats& stats)
{
    const auto& genres = stats.category(Category::Genre);

    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Genres"), content));
    layout->addWidget(subtitleLabel(tr("%1 genres").arg(number(genres.unique)), content));
    layout->addSpacing(12);

    std::vector<ChartData> points;
    int other = genres.otherListens;
    for(size_t i{0}; i < genres.top.size(); ++i) {
        if(i < 6) {
            points.push_back({genres.top[i].key, genres.top[i].count});
        }
        else {
            other += genres.top[i].count;
        }
    }

    auto* donut = new DonutChart(content);
    donut->setRingSize(160);
    donut->setCentreLabel(tr("Listens"));
    donut->setData(std::move(points), other);
    layout->addWidget(donut);

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::clockPage(const PlayHistoryStats& stats)
{
    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Listening clock"), content));
    layout->addSpacing(12);

    auto* clock = new ListeningClockChart(content);
    clock->setClockSize(220);
    clock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    clock->setData(stats.hours);
    layout->addWidget(clock, 0, Qt::AlignHCenter);
    layout->addSpacing(16);

    const QLocale locale;
    std::vector<ChartData> weekdays;
    for(int i{0}; i < 7; ++i) {
        weekdays.push_back(
            {locale.dayName(i == 0 ? 7 : i, QLocale::ShortFormat), stats.weekdays[static_cast<size_t>(i)]});
    }
    auto* bars = new VerticalBarsChart(content);
    bars->setPlotHeight(90);
    bars->setData(std::move(weekdays));
    layout->addWidget(bars);

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::daysPage(const PlayHistoryStats& stats)
{
    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Days"), content));
    layout->addSpacing(12);

    auto* calendar = new CalendarHeatmapChart(content);
    calendar->setMaxCellSize(12);
    calendar->setData(stats.listensPerDay, stats.firstDay);
    layout->addWidget(calendar);
    layout->addSpacing(16);

    if(const auto& streak = stats.longestStreak) {
        layout->addWidget(
            new BigStat(daysText(streak->days), tr("Longest streak • %1").arg(formatDay(streak->startDay())), content));
        layout->addSpacing(8);
    }
    layout->addWidget(
        new BigStat(number(stats.peakDayListens), tr("Busiest day • %1").arg(formatDay(stats.peakDay)), content));

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::discoveriesPage(const PlayHistoryStats& stats)
{
    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Discoveries"), content));
    layout->addWidget(subtitleLabel(tr("First listen"), content));
    layout->addSpacing(12);

    layout->addLayout(statRow(new BigStat(number(stats.newTracks), tr("Tracks"), content),
                              new BigStat(number(stats.newArtists), tr("Artists"), content)));

    if(!stats.newTracksTop.empty()) {
        layout->addSpacing(12);

        std::vector<RowListWidget::Row> rows;
        for(size_t i{0}; i < std::min<size_t>(5, stats.newTracksTop.size()); ++i) {
            const auto& track = stats.newTracksTop[i];
            rows.push_back(trackRow(track, number(track.count), formatDay(dateOf(track.timestamp))));
        }
        layout->addWidget(rowList(std::move(rows), content, false));
    }

    if(!stats.newArtistsTop.empty()) {
        layout->addSpacing(12);

        auto* grid = new QGridLayout();
        grid->setSpacing(8);
        for(size_t i{0}; i < std::min<size_t>(6, stats.newArtistsTop.size()); ++i) {
            const auto& artist = stats.newArtistsTop[i];
            grid->addWidget(new MiniTile(listensText(artist.count), artist.key, content), static_cast<int>(i) / 2,
                            static_cast<int>(i) % 2);
        }
        layout->addLayout(grid);
    }

    if(const auto& favourite = stats.oldestFavourite) {
        layout->addSpacing(16);
        layout->addWidget(
            subtitleLabel(tr("Oldest favourite • %1").arg(formatDay(dateOf(favourite->timestamp))), content));
        layout->addWidget(rowList({trackRow(*favourite, number(favourite->count))}, content, false));
    }

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::songsOfTheYearPage(const PlayHistoryStats& stats)
{
    const int songs = static_cast<int>(
        std::ranges::count_if(stats.monthlyTop, [](const auto& track) { return track.has_value(); }));

    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Your year in %1 songs").arg(songs), content));
    layout->addSpacing(12);

    const QLocale locale;
    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    for(int month{0}; month < 12; ++month) {
        const auto& top    = stats.monthlyTop[static_cast<size_t>(month)];
        const QString name = locale.monthName(month + 1, QLocale::ShortFormat);

        auto* tile = top ? new MiniTile(name, top->title.isEmpty() ? tr("Unknown title") : top->title, content)
                         : new MiniTile(name, u"–"_s, content);
        // Months without a song keep the same height as the rest
        tile->setDetail(top ? top->artist : u" "_s);
        grid->addWidget(tile, month / 3, month % 3);
    }
    for(int column{0}; column < 3; ++column) {
        grid->setColumnStretch(column, 1);
    }
    layout->addLayout(grid);

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::obsessionPage(const PlayHistoryStats& stats)
{
    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Obsession"), content));

    if(!stats.obsessions.empty()) {
        layout->addWidget(subtitleLabel(tr("Most played • day"), content));
        layout->addSpacing(12);

        std::vector<RowListWidget::Row> rows;
        for(size_t i{0}; i < std::min<size_t>(5, stats.obsessions.size()); ++i) {
            const auto& obsession = stats.obsessions[i];
            rows.push_back(
                trackRow(obsession, number(obsession.count) + u"×"_s, formatDay(dateOf(obsession.timestamp))));
        }
        layout->addWidget(rowList(std::move(rows), content, false));
    }

    if(const auto& session = stats.longestSession) {
        const QLocale locale;
        layout->addSpacing(24);
        layout->addWidget(new BigStat(formatDuration(session->durationMs()),
                                      tr("Longest session • %1").arg(formatDay(dateOf(session->startMs))), content));
        layout->addWidget(subtitleLabel(
            tr("%1 → %2 • %3")
                .arg(locale.toString(QDateTime::fromMSecsSinceEpoch(session->startMs).time(), QLocale::ShortFormat),
                     locale.toString(QDateTime::fromMSecsSinceEpoch(session->endMs).time(), QLocale::ShortFormat),
                     listensText(session->listens)),
            content));
    }

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::habitsPage(const PlayHistoryStats& stats)
{
    const int repeatPercent = stats.totalListens <= 1
                                ? 0
                                : static_cast<int>(std::lround(stats.repeatListens * 100.0 / (stats.totalListens - 1)));

    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Habits"), content));
    layout->addSpacing(12);

    layout->addLayout(
        statRow(new BigStat(u"%1%"_s.arg(repeatPercent), tr("Repeat rate"), content),
                new BigStat(tr("%1 tracks").arg(number(stats.heavyRotationTracks)),
                            tr("Played %1+ times").arg(PlayHistoryStats::HeavyRotationListens), content)));
    layout->addSpacing(8);
    layout->addLayout(statRow(new BigStat(formatMinuteOfDay(stats.avgFirstMinute), tr("First listen"), content),
                              new BigStat(formatMinuteOfDay(stats.avgLastMinute), tr("Last listen"), content)));

    if(!stats.completedAlbums.empty()) {
        layout->addSpacing(8);
        layout->addWidget(new BigStat(tr("%1 albums").arg(number(static_cast<qint64>(stats.completedAlbums.size()))),
                                      tr("Completed albums"), content));
    }

    if(const auto& streak = stats.longestTrackStreak) {
        layout->addSpacing(16);
        layout->addWidget(subtitleLabel(tr("Streak • %1").arg(daysText(streak->days)), content));
        layout->addWidget(rowList({trackRow(*streak, number(streak->count))}, content, false));
    }

    if(!stats.rediscoveries.empty()) {
        layout->addSpacing(16);
        layout->addWidget(subtitleLabel(tr("Rediscoveries"), content));

        std::vector<RowListWidget::Row> rows;
        for(size_t i{0}; i < std::min<size_t>(3, stats.rediscoveries.size()); ++i) {
            const auto& track = stats.rediscoveries[i];
            rows.push_back(trackRow(track, number(track.count), daysText(track.days)));
        }
        layout->addWidget(rowList(std::move(rows), content, false));
    }

    finishContent(layout);
    return content;
}

QWidget* YearInReviewDialog::summaryPage(int year, const PlayHistoryStats& stats,
                                         const std::optional<PlayHistoryStats>& previous)
{
    QVBoxLayout* layout{nullptr};
    auto* content = newContent(layout);

    layout->addWidget(titleLabel(tr("Your %1").arg(year), content));
    layout->addSpacing(8);

    if(!stats.topTracks.empty()) {
        const auto& top = stats.topTracks.front();

        auto* title = new QLabel(top.title.isEmpty() ? tr("Unknown title") : top.title, content);
        QFont font  = title->font();
        font.setBold(true);
        font.setPointSizeF(font.pointSizeF() * 1.4);
        title->setFont(font);
        title->setAlignment(Qt::AlignHCenter);
        title->setWordWrap(true);
        layout->addWidget(title);

        auto* artist = subtitleLabel(trackSubtitle(top), content);
        artist->setAlignment(Qt::AlignHCenter);
        layout->addWidget(artist);
        layout->addSpacing(16);
    }

    const auto& artists = stats.category(Category::Artist).top;
    const auto& genres  = stats.category(Category::Genre).top;
    const auto& albums  = stats.category(Category::Album).top;

    layout->addLayout(statRow(new BigStat(number(stats.totalListens), tr("Total listens"), content),
                              new BigStat(formatListenTime(stats.listenSeconds), tr("Total listen time"), content)));
    layout->addSpacing(8);

    QWidget* topCategory = nullptr;
    if(!genres.empty()) {
        topCategory = new BigStat(genres.front().key, tr("Genre"), content);
    }
    else if(!albums.empty()) {
        topCategory = new BigStat(albums.front().key, tr("Album"), content);
    }
    else {
        topCategory = new BigStat(number(stats.uniqueTracks), tr("Tracks"), content);
    }
    layout->addLayout(statRow(new BigStat(artists.empty() ? QString{} : artists.front().key, tr("Top artist"), content),
                              topCategory));
    layout->addSpacing(8);
    layout->addLayout(statRow(
        new BigStat(daysText(stats.daysWithListens), tr("Days"), content),
        new BigStat(daysText(stats.longestStreak ? stats.longestStreak->days : 0), tr("Longest streak"), content)));

    if(previous) {
        layout->addSpacing(20);
        layout->addWidget(subtitleLabel(tr("Previous • %1").arg(year - 1), content));
        layout->addSpacing(4);

        auto* listens = new MiniTile(tr("Total listens"), number(previous->totalListens), content);
        listens->setDelta(stats.totalListens, previous->totalListens);

        const auto& previousArtists = previous->category(Category::Artist).top;
        layout->addLayout(
            statRow(listens, new MiniTile(tr("Top artist"),
                                          previousArtists.empty() ? u"–"_s : previousArtists.front().key, content)));
    }

    finishContent(layout);
    return content;
}
} // namespace Fooyin

#include "moc_yearinreviewdialog.cpp"
