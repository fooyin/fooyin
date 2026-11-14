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

#include "librarybreakdownmodel.h"
#include "listeninghistorymodel.h"
#include "trackstatisticsdialog.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/player/playbackhistoryservice.h>
#include <core/scripting/scriptparser.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/widgets/scriptlineedit.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringcollator.h>
#include <utils/stringutils.h>
#include <utils/utils.h>

#include <QAction>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTimer>
#include <QTimerEvent>
#include <QTreeView>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

constexpr uint64_t HistoryPageSize = 100;
constexpr int SavedScriptRole      = Qt::UserRole + 1;

namespace Fooyin {
namespace {
QString countText(uint64_t count)
{
    return QLocale{}.toString(static_cast<qulonglong>(count));
}

QString dateText(const std::optional<qint64>& date)
{
    return date ? QLocale{}.toString(QDateTime::fromMSecsSinceEpoch(*date), QLocale::ShortFormat) : u"—"_s;
}

void prepareTable(QTableWidget* table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
}

void setTableText(QTableWidget* table, int row, int column, const QString& text,
                  Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(alignment);
    table->setItem(row, column, item);
}

QString sourceName(const PlaybackSource& source)
{
    return source.displayName.isEmpty() ? source.originalUri : source.displayName;
}
} // namespace

StatisticsDialog::StatisticsDialog(PlaybackHistoryService* historyService, MusicLibrary* library,
                                   SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_historyService{historyService}
    , m_scope{new QComboBox(this)}
    , m_trackCount{new QLabel(u"—"_s, this)}
    , m_totalDuration{new QLabel(u"—"_s, this)}
    , m_totalSize{new QLabel(u"—"_s, this)}
    , m_totalPlayCount{new QLabel(u"—"_s, this)}
    , m_averageRating{new QLabel(u"—"_s, this)}
    , m_unratedTrackCount{new QLabel(u"—"_s, this)}
    , m_trackingSince{new QLabel(u"—"_s, this)}
    , m_trackingSinceText{u"—"_s}
    , m_breakdown{new QComboBox(this)}
    , m_breakdownScript{new ScriptLineEdit(u"%albumartist%"_s, this)}
    , m_saveBreakdown{new QAction(tr("Save as…"), this)}
    , m_breakdownActions{new QAction(tr("Saved breakdown actions"), this)}
    , m_updateBreakdown{new QAction(tr("Update"), this)}
    , m_deleteBreakdown{new QAction(tr("Delete"), this)}
    , m_breakdownLoading{false}
    , m_breakdownModel{new LibraryBreakdownModel(this)}
    , m_libraryBreakdown{new QTableView(this)}
    , m_periods{new QTableWidget(this)}
    , m_rankings{new QTableWidget(this)}
    , m_historyScope{new QComboBox(this)}
    , m_historyFilter{new QLineEdit(this)}
    , m_historyModel{new ListeningHistoryModel(this)}
    , m_historyTree{new QTreeView(this)}
    , m_historyStatus{new QLabel(this)}
    , m_loadMore{new QPushButton(tr("Load more"), this)}
    , m_historyFilterTimer{new QTimer(this)}
    , m_requestToken{0}
    , m_breakdownRequestToken{0}
    , m_historyRequestToken{0}
    , m_historyTotal{0}
    , m_historyGeneration{0}
    , m_totalsOnly{false}
    , m_refreshPending{false}
    , m_libraryDirty{false}
    , m_libraryPending{false}
{
    setWindowTitle(tr("Statistics"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(760, 560);

    auto* tabs = new QTabWidget(this);

    auto* libraryPage   = new QWidget(tabs);
    auto* libraryLayout = new QVBoxLayout(libraryPage);

    auto* summaryGroup  = new QGroupBox(tr("Library Overview"), libraryPage);
    auto* summaryLayout = new QFormLayout(summaryGroup);
    summaryLayout->addRow(tr("Tracks") + u":"_s, m_trackCount);
    summaryLayout->addRow(tr("Duration") + u":"_s, m_totalDuration);
    summaryLayout->addRow(tr("Storage") + u":"_s, m_totalSize);
    summaryLayout->addRow(tr("Play count") + u":"_s, m_totalPlayCount);
    summaryLayout->addRow(tr("Average rating") + u":"_s, m_averageRating);
    summaryLayout->addRow(tr("Unrated tracks") + u":"_s, m_unratedTrackCount);
    m_averageRating->setToolTip(tr("Average rating of rated library tracks"));
    libraryLayout->addWidget(summaryGroup);

    m_breakdown->addItem(tr("Codec"), static_cast<int>(LibraryBreakdown::Codec));
    m_breakdown->addItem(tr("Bit depth"), static_cast<int>(LibraryBreakdown::BitDepth));
    m_breakdown->addItem(tr("Sample rate"), static_cast<int>(LibraryBreakdown::SampleRate));
    m_breakdown->addItem(tr("Library folder"), static_cast<int>(LibraryBreakdown::Folder));
    m_breakdown->addItem(tr("Rating"), static_cast<int>(LibraryBreakdown::Rating));

    FySettings savedSettings;
    const int savedCount = savedSettings.beginReadArray(u"Statistics/Breakdowns"_s);
    for(int index{0}; index < savedCount; ++index) {
        savedSettings.setArrayIndex(index);
        const QString name   = savedSettings.value(u"Name"_s).toString();
        const QString script = savedSettings.value(u"Script"_s).toString();
        if(!name.trimmed().isEmpty() && !script.trimmed().isEmpty()) {
            m_breakdown->addItem(name, static_cast<int>(LibraryBreakdown::Custom));
            m_breakdown->setItemData(m_breakdown->count() - 1, script, SavedScriptRole);
        }
    }
    savedSettings.endArray();
    m_breakdown->addItem(tr("Custom"), static_cast<int>(LibraryBreakdown::Custom));

    auto* breakdownMenu = new QMenu(m_breakdownScript);
    breakdownMenu->addAction(m_saveBreakdown);
    breakdownMenu->addAction(m_updateBreakdown);
    breakdownMenu->addAction(m_deleteBreakdown);
    Gui::setThemeIcon(m_breakdownActions, Constants::Icons::MainMenu);
    auto* editorAction = m_breakdownScript->actions().front();
    m_breakdownScript->removeAction(editorAction);
    m_breakdownScript->addAction(m_breakdownActions, QLineEdit::TrailingPosition);
    m_breakdownScript->addAction(editorAction, QLineEdit::TrailingPosition);
    QObject::connect(m_breakdownActions, &QAction::triggered, this, [this, breakdownMenu]() {
        const int x
            = layoutDirection() == Qt::LeftToRight ? m_breakdownScript->width() - breakdownMenu->sizeHint().width() : 0;
        breakdownMenu->popup(m_breakdownScript->mapToGlobal(QPoint{x, m_breakdownScript->height()}));
    });

    auto* breakdownControls = new QHBoxLayout();
    breakdownControls->addWidget(m_breakdown);
    breakdownControls->addWidget(m_breakdownScript, 1);
    auto* breakdownLayout = new QFormLayout();
    breakdownLayout->addRow(tr("Breakdown") + u":"_s, breakdownControls);
    libraryLayout->addLayout(breakdownLayout);

    m_breakdownScript->setPlaceholderText(tr("Grouping expression"));
    m_breakdownScript->hide();
    updateBreakdownActions();

    auto* breakdownProxy = new StringSortProxyModel(this);
    breakdownProxy->setSourceModel(m_breakdownModel);
    breakdownProxy->setSortRole(LibraryBreakdownModel::SortRole);

    m_libraryBreakdown->setModel(breakdownProxy);
    m_libraryBreakdown->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryBreakdown->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryBreakdown->setAlternatingRowColors(true);
    m_libraryBreakdown->verticalHeader()->hide();
    m_libraryBreakdown->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_libraryBreakdown->horizontalHeader()->setSortIndicator(1, Qt::DescendingOrder);
    m_libraryBreakdown->setSortingEnabled(true);
    m_libraryBreakdown->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    libraryLayout->addWidget(m_libraryBreakdown, 1);
    tabs->addTab(libraryPage, tr("Library"));

    auto* listeningPage   = new QWidget(tabs);
    auto* listeningLayout = new QVBoxLayout(listeningPage);

    m_scope->addItem(tr("All playback"), static_cast<int>(PlaybackHistoryScope::All));
    m_scope->addItem(tr("Local tracks"), static_cast<int>(PlaybackHistoryScope::LocalTracks));
    m_scope->addItem(tr("Remote streams"), static_cast<int>(PlaybackHistoryScope::RemoteStreams));

    auto* scopeLayout = new QFormLayout();
    scopeLayout->addRow(tr("Scope") + u":"_s, m_scope);
    listeningLayout->addLayout(scopeLayout);

    m_periods->setColumnCount(4);
    m_periods->setRowCount(4);
    m_periods->setHorizontalHeaderLabels({tr("Period"), tr("Listening time"), tr("Listens"), tr("Unique sources")});
    prepareTable(m_periods);
    m_periods->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    const QStringList periods{tr("Lifetime"), tr("Today"), tr("Last 7 days"), tr("Last 30 days")};
    for(int row = 0; row < periods.size(); ++row) {
        setTableText(m_periods, row, 0, periods.at(row));
    }
    m_periods->item(2, 0)->setToolTip(tr("This period includes today and the previous six calendar days."));
    m_periods->item(3, 0)->setToolTip(tr("This period includes today and the previous 29 calendar days."));
    m_periods->horizontalHeaderItem(2)->setToolTip(
        tr("Local tracks are grouped by when they reach the played threshold. Remote sessions use their start time."));

    listeningLayout->addWidget(m_periods);

    auto* rankingsTitle = new QLabel(tr("Top sources"), listeningPage);
    listeningLayout->addWidget(rankingsTitle);
    m_rankings->setColumnCount(4);
    m_rankings->setHorizontalHeaderLabels({tr("Source"), tr("Listening time"), tr("Listens"), tr("Last listened")});
    prepareTable(m_rankings);
    m_rankings->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    listeningLayout->addWidget(m_rankings, 1);
    tabs->addTab(listeningPage, tr("Listening"));

    auto* historyPage   = new QWidget(tabs);
    auto* historyLayout = new QVBoxLayout(historyPage);

    m_historyScope->addItem(tr("All playback"), static_cast<int>(PlaybackHistoryScope::All));
    m_historyScope->addItem(tr("Local tracks"), static_cast<int>(PlaybackHistoryScope::LocalTracks));
    m_historyScope->addItem(tr("Remote streams"), static_cast<int>(PlaybackHistoryScope::RemoteStreams));
    m_historyFilter->setPlaceholderText(tr("Filter by source, location, or track identity"));

    auto* historyControls = new QFormLayout();
    historyControls->addRow(tr("Scope") + u":"_s, m_historyScope);
    historyControls->addRow(tr("Filter") + u":"_s, m_historyFilter);
    historyLayout->addLayout(historyControls);

    m_historyTree->setModel(m_historyModel);
    m_historyTree->setAlternatingRowColors(true);
    m_historyTree->setUniformRowHeights(true);
    m_historyTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTree->header()->setStretchLastSection(false);
    m_historyTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_historyTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_historyTree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_historyTree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_historyTree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    historyLayout->addWidget(m_historyTree, 1);

    auto* historyFooter = new QHBoxLayout();
    historyFooter->addWidget(m_historyStatus);
    historyFooter->addStretch();
    historyFooter->addWidget(m_loadMore);
    historyLayout->addLayout(historyFooter);
    tabs->addTab(historyPage, tr("History"));

    const auto updateTrackingMode = [this, tabs, historyPage, rankingsTitle](int mode) {
        m_totalsOnly = (mode == static_cast<int>(ListeningHistoryMode::TotalsOnly));
        if(m_totalsOnly && tabs->currentWidget() == historyPage) {
            tabs->setCurrentIndex(tabs->indexOf(historyPage) - 1);
        }
        tabs->setTabVisible(tabs->indexOf(historyPage), !m_totalsOnly);
        rankingsTitle->setVisible(!m_totalsOnly);
        m_rankings->setVisible(!m_totalsOnly);
        m_periods->setColumnHidden(2, m_totalsOnly);
        m_periods->setColumnHidden(3, m_totalsOnly);
    };
    updateTrackingMode(settings->value<Settings::Core::ListeningHistoryMode>());
    settings->subscribe<Settings::Core::ListeningHistoryMode>(this, [this, updateTrackingMode](int mode) {
        updateTrackingMode(mode);
        refresh();
        refreshHistory(true);
    });

    auto* buttons      = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* manageButton = buttons->addButton(tr("Manage"), QDialogButtonBox::ActionRole);
    auto* manageMenu   = new QMenu(manageButton);
    manageButton->setMenu(manageMenu);

    manageMenu->addAction(tr("Configure statistics grouping…"), this,
                          [library, settings, this]() { TrackStatisticsDialog::openRebuild(library, settings, this); });
    auto* resetStatistics = manageMenu->addAction(tr("Reset track statistics…"), this, [library, settings, this]() {
        TrackStatisticsDialog::openReset(library, settings, library->libraryTracks(), this);
    });
    resetStatistics->setToolTip(tr("Reset track statistics for all library tracks and their shared statistics groups"));
    QObject::connect(manageMenu, &QMenu::aboutToShow, this,
                     [library, resetStatistics]() { resetStatistics->setEnabled(!library->libraryTracks().empty()); });
    manageMenu->addSeparator();
    manageMenu->addAction(tr("Clear listening history…"), this, [this]() {
        const auto answer = QMessageBox::question(
            this, tr("Clear listening history"),
            tr("Permanently delete all recorded listening history and listening-time statistics? "
               "Library play counts and ratings will be kept."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if(answer != QMessageBox::Yes) {
            return;
        }
        if(!m_historyService->clearHistory()) {
            QMessageBox::warning(this, tr("Clear listening history"), tr("Listening history could not be cleared."));
            return;
        }
    });

    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_scope, &QComboBox::currentIndexChanged, this, [this]() { refresh(); });
    QObject::connect(m_breakdown, &QComboBox::currentIndexChanged, this, [this]() {
        const auto script = m_breakdown->currentData(SavedScriptRole);
        if(script.isValid()) {
            const QSignalBlocker blocker{m_breakdownScript};
            m_breakdownScript->setScript(script.toString());
        }
        updateBreakdownActions();
        updateLibraryBreakdown();
    });
    QObject::connect(m_saveBreakdown, &QAction::triggered, this, &StatisticsDialog::saveBreakdownAs);
    QObject::connect(m_updateBreakdown, &QAction::triggered, this, [this]() {
        m_breakdown->setItemData(m_breakdown->currentIndex(), m_breakdownScript->text(), SavedScriptRole);
        storeSavedBreakdowns();
        updateBreakdownActions();
    });
    QObject::connect(m_deleteBreakdown, &QAction::triggered, this, [this]() {
        const QSignalBlocker blocker{m_breakdown};
        m_breakdown->removeItem(m_breakdown->currentIndex());
        m_breakdown->setCurrentIndex(m_breakdown->count() - 1);
        storeSavedBreakdowns();
        updateBreakdownActions();
        updateLibraryBreakdown();
    });
    QObject::connect(m_breakdownScript, &QLineEdit::textChanged, this, [this]() {
        updateBreakdownActions();
        if(static_cast<LibraryBreakdown>(m_breakdown->currentData().toInt()) == LibraryBreakdown::Custom) {
            ++m_breakdownRequestToken;
            m_breakdownTimer.start(250, this);
        }
    });
    QObject::connect(m_historyScope, &QComboBox::currentIndexChanged, this, [this]() { refreshHistory(true); });
    QObject::connect(m_historyFilter, &QLineEdit::textChanged, m_historyFilterTimer, qOverload<>(&QTimer::start));
    QObject::connect(m_historyFilterTimer, &QTimer::timeout, this, [this]() { refreshHistory(true); });
    QObject::connect(m_loadMore, &QPushButton::clicked, this, [this]() { refreshHistory(false); });
    QObject::connect(m_historyTree, &QTreeView::expanded, this,
                     [this](const QModelIndex& index) { loadMetadataSpans(index); });

    m_historyFilterTimer->setSingleShot(true);
    m_historyFilterTimer->setInterval(250);

    auto* footer = new QHBoxLayout();
    m_trackingSince->setTextFormat(Qt::PlainText);
    m_trackingSince->setWordWrap(true);
    footer->addWidget(m_trackingSince, 1);
    footer->addWidget(buttons);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addLayout(footer);

    QObject::connect(m_historyService, &PlaybackHistoryService::historyChanged, this, [this](qint64 id) {
        refresh();
        updateHistoryEvent(id);
    });
    QObject::connect(m_historyService, &PlaybackHistoryService::historyCleared, this, [this]() {
        refresh();
        refreshHistory(true);
    });

    const auto changed = [this]() {
        m_libraryDirty = true;
    };
    QObject::connect(library, &MusicLibrary::tracksLoaded, this, changed);
    QObject::connect(library, &MusicLibrary::tracksAdded, this, changed);
    QObject::connect(library, &MusicLibrary::tracksMetadataChanged, this, changed);
    QObject::connect(library, &MusicLibrary::tracksUpdated, this, changed);
    QObject::connect(library, &MusicLibrary::tracksStatsChanged, this, changed);
    QObject::connect(library, &MusicLibrary::tracksDeleted, this, changed);

    m_liveTimer.start(1000, this);
    const FyStateSettings stateSettings;
    Utils::restoreState(this, stateSettings, u"Statistics"_s);

    refresh(true);
    refreshHistory(true);
}

StatisticsDialog::~StatisticsDialog()
{
    FyStateSettings settings;
    Utils::saveState(this, settings, u"Statistics"_s);
}

void StatisticsDialog::refresh(bool library)
{
    m_libraryDirty |= library;
    if(m_libraryDirty && !m_libraryPending) {
        m_libraryDirty   = false;
        m_libraryPending = true;
        m_historyService->queryLibraryStatistics().then(this, [this](const LibraryStatistics& statistics) {
            m_libraryPending = false;
            updateLibraryStatistics(statistics);
        });
    }

    const uint64_t requestToken = ++m_requestToken;
    if(m_refreshPending) {
        return;
    }
    m_refreshPending = true;

    const auto scope = static_cast<PlaybackHistoryScope>(m_scope->currentData().toInt());
    const auto live  = m_historyService->currentEvent();
    const auto day   = QDate::currentDate();

    auto overviewFuture = m_historyService->queryOverview(scope, live);

    PlaybackRankingQuery rankingQuery;
    rankingQuery.scope = scope;
    rankingQuery.limit = 10;

    auto finish = [this, requestToken, live, day, overviewFuture](std::vector<PlaybackSourceRanking> rankings) mutable {
        overviewFuture.then(this, [this, requestToken, live, day,
                                   rankings = std::move(rankings)](const PlaybackHistoryOverview& overview) mutable {
            if(requestToken == m_requestToken) {
                m_overviewBaseline = overview;
                m_liveSnapshot     = live;
                m_overviewDay      = day;
                m_rankingBaseline  = std::move(rankings);
                m_rankingSnapshot  = live;
                updateLiveProgress();
            }
            m_refreshPending = false;
            if(requestToken != m_requestToken) {
                refresh();
            }
        });
    };

    if(m_totalsOnly) {
        finish({});
        return;
    }

    m_historyService->querySourceRankings(rankingQuery, live)
        .then(this, [this, rankingQuery, live, finish](std::vector<PlaybackSourceRanking> rankings) mutable {
            if(!live || std::ranges::any_of(rankings, [&live](const auto& entry) {
                   return entry.source.id == live->sourceId;
               })) {
                finish(std::move(rankings));
                return;
            }

            auto query{rankingQuery};
            query.sourceId = live->sourceId;
            query.limit    = 1;

            m_historyService->querySourceRankings(query, live)
                .then(this, [finish, rankings = std::move(rankings)](
                                const std::vector<PlaybackSourceRanking>& candidate) mutable {
                    rankings.insert(rankings.end(), candidate.begin(), candidate.end());
                    finish(std::move(rankings));
                });
        });
}

void StatisticsDialog::updateLiveProgress()
{
    const auto live = m_historyService->currentEvent();

    if(m_libraryDirty && !m_libraryPending && !m_refreshPending) {
        refresh(true);
    }

    if(m_overviewDay != QDate::currentDate()
       || m_overviewBaseline.scope != static_cast<PlaybackHistoryScope>(m_scope->currentData().toInt())) {
        if(!m_refreshPending) {
            refresh();
        }
        return;
    }

    auto overview{m_overviewBaseline};

    if(live) {
        m_historyModel->updateListeningTime(live->id, live->listeningTimeMs);

        if(m_liveSnapshot && live->id == m_liveSnapshot->id) {
            if(m_liveSnapshot->listeningTimeMs == 0 && live->listeningTimeMs > 0) {
                if(!m_refreshPending) {
                    refresh();
                }
                return;
            }

            const bool remote = m_historyService->currentSourceKind() == PlaybackSourceKind::RemoteStream;
            if(overview.scope == PlaybackHistoryScope::All
               || (overview.scope == PlaybackHistoryScope::RemoteStreams && remote)
               || (overview.scope == PlaybackHistoryScope::LocalTracks && !remote)) {
                const auto delta
                    = live->listeningTimeMs - std::min(live->listeningTimeMs, m_liveSnapshot->listeningTimeMs);
                overview.lifetime.listeningTimeMs += delta;
                overview.today.listeningTimeMs += delta;
                overview.last7Days.listeningTimeMs += delta;
                overview.last30Days.listeningTimeMs += delta;
            }
        }
    }

    updateListeningOverview(overview);

    if(m_totalsOnly) {
        return;
    }

    auto rankings{m_rankingBaseline};
    if(live && m_rankingSnapshot && live->id == m_rankingSnapshot->id) {
        for(auto& ranking : rankings) {
            if(ranking.source.id == live->sourceId) {
                ranking.listeningTimeMs
                    += live->listeningTimeMs - std::min(live->listeningTimeMs, m_rankingSnapshot->listeningTimeMs);
                ranking.lastListenedAt = live->lastCheckpointAt;
            }
        }
    }

    std::ranges::sort(rankings, [](const auto& lhs, const auto& rhs) {
        if(lhs.listeningTimeMs != rhs.listeningTimeMs) {
            return lhs.listeningTimeMs > rhs.listeningTimeMs;
        }
        if(lhs.listenCount != rhs.listenCount) {
            return lhs.listenCount > rhs.listenCount;
        }
        if(lhs.lastListenedAt != rhs.lastListenedAt) {
            return lhs.lastListenedAt > rhs.lastListenedAt;
        }
        return lhs.source.id < rhs.source.id;
    });

    if(rankings.size() > 10) {
        rankings.resize(10);
    }
    updateRankings(rankings);
}

void StatisticsDialog::updateLibraryStatistics(const LibraryStatistics& statistics)
{
    m_libraryStatistics = statistics;
    m_trackCount->setText(countText(statistics.trackCount));
    m_totalDuration->setText(Utils::msToString(statistics.totalDurationMs));
    m_totalSize->setText(Utils::formatFileSize(statistics.totalFileSize));
    m_totalPlayCount->setText(countText(statistics.totalPlayCount));
    m_averageRating->setText(statistics.averageRating
                                 ? tr("%1 out of 5").arg(QLocale{}.toString(*statistics.averageRating * 5.0, 'f', 2))
                                 : u"—"_s);
    m_unratedTrackCount->setText(countText(statistics.unratedTrackCount));
    updateLibraryBreakdown();
}

void StatisticsDialog::updateLibraryBreakdown()
{
    m_breakdownTimer.stop();
    ++m_breakdownRequestToken;

    const auto breakdown = static_cast<LibraryBreakdown>(m_breakdown->currentData().toInt());

    m_breakdownScript->setVisible(breakdown == LibraryBreakdown::Custom);
    m_breakdownLoading = false;
    m_breakdownError.clear();
    updateFooterStatus();

    const auto* groups = &m_libraryStatistics.formats;
    switch(breakdown) {
        case LibraryBreakdown::Codec:
            break;
        case LibraryBreakdown::BitDepth:
            groups = &m_libraryStatistics.bitDepths;
            break;
        case LibraryBreakdown::SampleRate:
            groups = &m_libraryStatistics.sampleRates;
            break;
        case LibraryBreakdown::Folder:
            groups = &m_libraryStatistics.folders;
            break;
        case LibraryBreakdown::Rating:
            groups = &m_libraryStatistics.ratings;
            break;
        case LibraryBreakdown::Custom: {
            updateCustomBreakdown();
            return;
        }
    }

    displayLibraryBreakdown(*groups);
}

void StatisticsDialog::updateCustomBreakdown()
{
    const uint64_t requestToken = ++m_breakdownRequestToken;
    m_breakdownLoading          = false;
    m_breakdownError.clear();
    updateFooterStatus();

    m_breakdownModel->setGroups({}, LibraryBreakdown::Custom);

    ScriptParser parser;
    const auto parsed = parser.parse(m_breakdownScript->text());

    if(m_breakdownScript->text().trimmed().isEmpty() || !parsed.isValid()) {
        m_breakdownError = parsed.errors.empty() ? tr("Enter a grouping expression.")
                                                 : tr("Invalid expression: %1").arg(parsed.errors.front().message);
        updateFooterStatus();
        return;
    }

    m_breakdownLoading = true;
    updateFooterStatus();

    m_historyService->queryLibraryBreakdown(m_breakdownScript->text())
        .then(this, [this, requestToken](const std::vector<LibraryGroupStatistics>& customGroups) {
            if(requestToken == m_breakdownRequestToken) {
                m_breakdownLoading = false;
                updateFooterStatus();
                displayLibraryBreakdown(customGroups);
            }
        });
}

void StatisticsDialog::updateBreakdownActions()
{
    const bool custom = static_cast<LibraryBreakdown>(m_breakdown->currentData().toInt()) == LibraryBreakdown::Custom;
    const auto savedScript = m_breakdown->currentData(SavedScriptRole);

    ScriptParser parser;
    const bool valid
        = custom && !m_breakdownScript->text().trimmed().isEmpty() && parser.parse(m_breakdownScript->text()).isValid();
    m_saveBreakdown->setVisible(custom);
    m_saveBreakdown->setEnabled(valid);
    m_breakdownActions->setVisible(custom);
    m_updateBreakdown->setEnabled(valid && savedScript.isValid()
                                  && savedScript.toString() != m_breakdownScript->text());
    m_deleteBreakdown->setEnabled(savedScript.isValid());
}

void StatisticsDialog::saveBreakdownAs()
{
    bool accepted{false};
    const QString name
        = QInputDialog::getText(this, tr("Save Breakdown"), tr("Name") + u":"_s, QLineEdit::Normal, {}, &accepted)
              .trimmed();
    if(!accepted) {
        return;
    }

    if(name.isEmpty()) {
        m_breakdownError = tr("Enter a breakdown name.");
        updateFooterStatus();
        return;
    }

    for(int index{0}; index < m_breakdown->count(); ++index) {
        if(m_breakdown->itemText(index).compare(name, Qt::CaseInsensitive) == 0) {
            m_breakdownError = tr("A breakdown named ‘%1’ already exists.").arg(name);
            updateFooterStatus();
            return;
        }
    }

    const int index = m_breakdown->count() - 1;
    {
        const QSignalBlocker blocker{m_breakdown};
        m_breakdown->insertItem(index, name, static_cast<int>(LibraryBreakdown::Custom));
        m_breakdown->setItemData(index, m_breakdownScript->text(), SavedScriptRole);
    }
    storeSavedBreakdowns();
    m_breakdown->setCurrentIndex(index);
}

void StatisticsDialog::storeSavedBreakdowns()
{
    FySettings settings;
    settings.remove(u"Statistics/Breakdowns"_s);
    settings.beginWriteArray(u"Statistics/Breakdowns"_s);
    int savedIndex{0};
    for(int index{0}; index < m_breakdown->count(); ++index) {
        const auto script = m_breakdown->itemData(index, SavedScriptRole);
        if(script.isValid()) {
            settings.setArrayIndex(savedIndex++);
            settings.setValue(u"Name"_s, m_breakdown->itemText(index));
            settings.setValue(u"Script"_s, script.toString());
        }
    }
    settings.endArray();
}

void StatisticsDialog::displayLibraryBreakdown(const std::vector<LibraryGroupStatistics>& groups)
{
    m_breakdownModel->setGroups(groups, static_cast<LibraryBreakdown>(m_breakdown->currentData().toInt()));
}

void StatisticsDialog::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_liveTimer.timerId()) {
        updateLiveProgress();
        return;
    }
    if(event->timerId() == m_breakdownTimer.timerId()) {
        updateLibraryBreakdown();
        return;
    }
    QDialog::timerEvent(event);
}

void StatisticsDialog::updateListeningOverview(const PlaybackHistoryOverview& overview)
{
    const std::array totals{overview.lifetime, overview.today, overview.last7Days, overview.last30Days};
    for(int row{0}; std::cmp_less(row, totals.size()); ++row) {
        const auto& total = totals.at(static_cast<size_t>(row));
        setTableText(m_periods, row, 1, Utils::msToString(total.listeningTimeMs), Qt::AlignRight | Qt::AlignVCenter);
        setTableText(m_periods, row, 2, countText(total.listenCount), Qt::AlignRight | Qt::AlignVCenter);
        setTableText(m_periods, row, 3, countText(total.uniqueSourceCount), Qt::AlignRight | Qt::AlignVCenter);
    }

    m_trackingSinceText
        = overview.lifetime.firstActivityAt
            ? tr("Listening statistics tracked since %1").arg(dateText(overview.lifetime.firstActivityAt))
            : tr("No listening statistics have been recorded yet.");
    updateFooterStatus();
}

void StatisticsDialog::updateFooterStatus()
{
    if(!m_breakdownError.isEmpty()) {
        m_trackingSince->setText(m_breakdownError);
    }
    else {
        m_trackingSince->setText(m_breakdownLoading ? tr("Loading…") : m_trackingSinceText);
    }
}

void StatisticsDialog::updateRankings(const std::vector<PlaybackSourceRanking>& rankings)
{
    m_rankings->setRowCount(static_cast<int>(rankings.size()));
    for(int row{0}; row < m_rankings->rowCount(); ++row) {
        const auto& ranking = rankings.at(static_cast<size_t>(row));
        setTableText(m_rankings, row, 0, sourceName(ranking.source));
        setTableText(m_rankings, row, 1, Utils::msToString(ranking.listeningTimeMs), Qt::AlignRight | Qt::AlignVCenter);
        setTableText(m_rankings, row, 2, countText(ranking.listenCount), Qt::AlignRight | Qt::AlignVCenter);
        setTableText(m_rankings, row, 3, dateText(ranking.lastListenedAt), Qt::AlignRight | Qt::AlignVCenter);
    }
}

void StatisticsDialog::refreshHistory(bool reset)
{
    if(m_totalsOnly) {
        return;
    }

    if(reset) {
        ++m_historyGeneration;
        m_eventRequests.clear();
        m_historyModel->clear();
        m_historyCursor.reset();
        m_historyTotal = 0;
    }

    const uint64_t requestToken = ++m_historyRequestToken;
    m_loadMore->setEnabled(false);
    m_historyStatus->setText(tr("Loading…"));

    PlaybackHistoryQuery query;
    query.scope  = static_cast<PlaybackHistoryScope>(m_historyScope->currentData().toInt());
    query.filter = m_historyFilter->text();
    query.limit  = HistoryPageSize;
    query.before = m_historyCursor;

    m_historyService->queryHistory(query).then(this, [this, requestToken](const PlaybackHistoryPage& page) {
        if(requestToken == m_historyRequestToken) {
            if(!page.entries.empty()) {
                const auto& last = page.entries.back().event;
                m_historyCursor  = std::pair{last.startedAt, last.id};
            }
            appendHistoryPage(page);
            m_loadMore->setEnabled(page.hasMore);
        }
    });
}

void StatisticsDialog::appendHistoryPage(const PlaybackHistoryPage& page)
{
    std::vector<QPersistentModelIndex> dates;
    for(int row{0}; row < m_historyModel->rowCount({}); ++row) {
        dates.emplace_back(m_historyModel->index(row, 0, {}));
    }

    m_historyModel->appendEntries(page.entries);

    for(int row{0}; row < m_historyModel->rowCount({}); ++row) {
        const auto index = m_historyModel->index(row, 0, {});
        m_historyTree->setFirstColumnSpanned(row, {}, true);
        if(std::ranges::find(dates, QPersistentModelIndex{index}) == dates.cend()) {
            m_historyTree->expand(index);
        }
    }

    m_historyTotal = std::max(m_historyTotal, page.totalCount);
    updateHistoryStatus();
}

void StatisticsDialog::updateHistoryStatus()
{
    m_historyStatus->setText(
        m_historyTotal == 0
            ? tr("No listened tracks")
            : tr("Showing %1 of %2 listens").arg(countText(m_historyModel->eventCount()), countText(m_historyTotal)));
}

void StatisticsDialog::updateHistoryEvent(qint64 eventId)
{
    if(m_totalsOnly || eventId < 0) {
        return;
    }

    const auto generation = m_historyGeneration;
    const auto token      = ++m_eventRequests[eventId];

    PlaybackHistoryQuery query;
    query.scope   = static_cast<PlaybackHistoryScope>(m_historyScope->currentData().toInt());
    query.filter  = m_historyFilter->text();
    query.eventId = eventId;
    query.limit   = 1;

    m_historyService->queryHistory(query).then(
        this, [this, eventId, generation, token](const PlaybackHistoryPage& page) {
            if(generation != m_historyGeneration || token != m_eventRequests[eventId]) {
                return;
            }

            appendHistoryPage(page);
            if(const auto live = m_historyService->currentEvent(); live && live->id == eventId) {
                m_historyModel->updateListeningTime(eventId, live->listeningTimeMs);
            }

            const auto index = m_historyModel->eventIndex(eventId);
            if(index.isValid() && m_historyTree->isExpanded(index)) {
                const QPersistentModelIndex persistent{index};
                m_historyService->queryMetadataSpans(eventId).then(
                    this, [this, eventId, persistent](const std::vector<StreamMetadataSpan>& spans) {
                        if(persistent.isValid()) {
                            m_historyModel->setMetadataSpans(eventId, spans);
                        }
                    });
            }
        });
}

void StatisticsDialog::loadMetadataSpans(const QModelIndex& eventIndex)
{
    const auto eventId = m_historyModel->beginMetadataLoad(eventIndex);
    if(!eventId) {
        return;
    }

    const QPersistentModelIndex metadataIndex{eventIndex};
    m_historyService->queryMetadataSpans(*eventId).then(
        this, [this, eventId, metadataIndex](const std::vector<StreamMetadataSpan>& spans) {
            if(metadataIndex.isValid()) {
                m_historyModel->setMetadataSpans(*eventId, spans);
            }
        });
}
} // namespace Fooyin

#include "moc_statisticsdialog.cpp"
