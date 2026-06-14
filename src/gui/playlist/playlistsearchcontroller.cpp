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

#include "playlistsearchcontroller.h"

#include "playlistcontroller.h"
#include "playlistmodel.h"
#include "playlistview.h"
#include "scripting/scriptvariableproviders.h"

#include <core/playlist/playlist.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <core/scripting/scriptparser.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/iconloader.h>
#include <gui/widgets/toolbutton.h>
#include <utils/async.h>
#include <utils/settings/settingsmanager.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QWidget>

namespace Fooyin {
namespace {
QStringList searchWords(const QString& text)
{
    QStringList words;
    QString word;

    for(const QChar& ch : text) {
        if(ch.isLetterOrNumber()) {
            word.append(ch);
        }
        else if(!word.isEmpty()) {
            words.push_back(word);
            word.clear();
        }
    }

    if(!word.isEmpty()) {
        words.push_back(word);
    }

    return words;
}

bool matchesWordBeginnings(const QString& text, const QString& query)
{
    const QStringList queryWords = searchWords(query);
    if(queryWords.empty()) {
        return false;
    }

    const QStringList titleWords = searchWords(text);
    for(const QString& queryWord : queryWords) {
        const auto match = std::ranges::find_if(titleWords, [&queryWord](const QString& titleWord) {
            return titleWord.startsWith(queryWord, Qt::CaseInsensitive);
        });
        if(match == titleWords.cend()) {
            return false;
        }
    }

    return true;
}

bool matchesSearch(const QString& text, const QString& query, PlaylistSearchController::Mode mode)
{
    switch(mode) {
        case PlaylistSearchController::Mode::MatchWordBeginnings:
            return matchesWordBeginnings(text, query);
        case PlaylistSearchController::Mode::MatchAnywhere:
            return text.contains(query, Qt::CaseInsensitive);
    }

    return false;
}

struct MatchEvalRequest
{
    UId playlistId;
    PlaylistTrackList tracks;
    QString search;
    QString script;
    PlaylistSearchController::Mode mode;
};

std::vector<int> evaluateMatches(const MatchEvalRequest& request)
{
    ScriptParser parser;
    parser.addProvider(playlistVariableProvider());
    parser.addProvider(artworkMarkerVariableProvider());

    const auto parsedScript = parser.parse(request.script);
    if(!parsedScript.isValid()) {
        return {};
    }

    const TrackList trackList = PlaylistTrack::toTracks(request.tracks);

    PlaylistScriptEnvironment environment;
    environment.setPlaylistData(nullptr, nullptr, &trackList);

    ScriptContext context;
    context.environment = &environment;

    std::vector<int> matches;
    matches.reserve(request.tracks.size());

    for(const PlaylistTrack& track : request.tracks) {
        environment.setTrackState(track.indexInPlaylist, -1, -1, 0);

        const QString matchText = parser.evaluate(parsedScript, track.track, context);
        if(matchesSearch(matchText, request.search, request.mode)) {
            matches.emplace_back(track.indexInPlaylist);
        }
    }

    return matches;
}
} // namespace

PlaylistSearchController::PlaylistSearchController(PlaylistController* playlistController, PlaylistModel* model,
                                                   PlaylistView* view, QHeaderView* header, SettingsManager* settings,
                                                   QWidget* parent)
    : QObject{parent}
    , m_playlistController{playlistController}
    , m_model{model}
    , m_view{view}
    , m_header{header}
    , m_settings{settings}
    , m_widget{new QWidget(parent)}
    , m_search{new QLineEdit(m_widget)}
    , m_previous{new ToolButton(m_settings, m_widget)}
    , m_next{new ToolButton(m_settings, m_widget)}
    , m_matchesLabel{new QLabel(m_widget)}
    , m_close{new ToolButton(m_settings, m_widget)}
    , m_matchIndex{-1}
    , m_resultsOutOfDate{false}
    , m_searchRequestToken{0}
{
    auto* layout = new QHBoxLayout(m_widget);
    layout->setContentsMargins({});

    m_search->setPlaceholderText(tr("Search playlist"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);

    m_previous->setToolTip(tr("Previous match"));
    m_previous->setIcon(Gui::iconFromTheme(Constants::Icons::Up));
    QObject::connect(m_previous, &QToolButton::clicked, this, [this]() { navigate(-1); });

    m_next->setToolTip(tr("Next match"));
    m_next->setIcon(Gui::iconFromTheme(Constants::Icons::Down));
    QObject::connect(m_next, &QToolButton::clicked, this, [this]() { navigate(1); });

    m_close->setToolTip(tr("Close"));
    m_close->setIcon(Gui::iconFromTheme(Constants::Icons::Close));
    QObject::connect(m_close, &QToolButton::clicked, this, &PlaylistSearchController::close);

    layout->addWidget(m_search, 1);
    layout->addWidget(m_previous);
    layout->addWidget(m_next);
    layout->addWidget(m_matchesLabel);
    layout->addWidget(m_close);
    m_widget->hide();

    QObject::connect(m_search, &QLineEdit::textChanged, this, &PlaylistSearchController::refreshMatches);
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistChanged, this,
                     &PlaylistSearchController::markResultsOutOfDate);
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistUpdated, this,
                     &PlaylistSearchController::markResultsOutOfDate);
    QObject::connect(m_playlistController, &PlaylistController::currentPlaylistTracksChanged, this,
                     &PlaylistSearchController::markResultsOutOfDate);
    QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        if(!isOpen()) {
            return;
        }
        if(m_resultsOutOfDate) {
            if(m_view->selectionModel()) {
                m_view->selectionModel()->clearSelection();
            }
            return;
        }
        if(!m_matches.empty()) {
            selectMatch(m_matchIndex);
        }
    });
    QObject::connect(
        m_model, &PlaylistModel::playlistLoaded, this,
        [this]() {
            if(isOpen() && m_resultsOutOfDate && m_view->selectionModel()) {
                m_view->selectionModel()->clearSelection();
            }
        },
        Qt::QueuedConnection);

    m_settings->subscribe<Settings::Gui::PlaylistIntegratedSearch>(this, &PlaylistSearchController::updateEnabledState);
    m_settings->subscribe<Settings::Gui::PlaylistSearchScript>(this, &PlaylistSearchController::refreshMatchesIfOpen);
    m_settings->subscribe<Settings::Gui::PlaylistSearchMode>(this, &PlaylistSearchController::refreshMatchesIfOpen);
}

QWidget* PlaylistSearchController::widget() const
{
    return m_widget;
}

bool PlaylistSearchController::isOpen() const
{
    return m_widget->isVisible();
}

bool PlaylistSearchController::open()
{
    if(!m_settings->value<Settings::Gui::PlaylistIntegratedSearch>()) {
        return false;
    }

    m_widget->show();
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
    refreshMatches();
    return true;
}

void PlaylistSearchController::close()
{
    ++m_searchRequestToken;

    const QSignalBlocker blocker{m_search};
    m_search->clear();
    m_matches.clear();
    m_matchIndex       = -1;
    m_resultsOutOfDate = false;
    updateMatchLabel();

    if(m_view->selectionModel()) {
        m_view->selectionModel()->clearSelection();
    }

    m_widget->hide();
    m_view->setFocus(Qt::ShortcutFocusReason);
}

void PlaylistSearchController::navigate(int delta)
{
    if(m_resultsOutOfDate) {
        refreshMatches();
        return;
    }

    if(m_matches.empty()) {
        refreshMatches();
        return;
    }

    selectMatch(m_matchIndex + delta);
}

void PlaylistSearchController::refreshMatches()
{
    const uint64_t requestToken = ++m_searchRequestToken;

    m_matches.clear();
    m_matchIndex       = -1;
    m_resultsOutOfDate = false;

    const QString search = m_search->text().trimmed();
    const auto* playlist = m_playlistController->currentPlaylist();

    if(search.isEmpty() || !playlist) {
        if(m_view->selectionModel()) {
            m_view->selectionModel()->clearSelection();
        }
        updateMatchLabel();
        return;
    }

    MatchEvalRequest request{.playlistId = playlist->id(),
                             .tracks     = playlist->playlistTracks(),
                             .search     = search,
                             .script     = m_settings->value<Settings::Gui::PlaylistSearchScript>(),
                             .mode       = static_cast<Mode>(m_settings->value<Settings::Gui::PlaylistSearchMode>())};

    const UId playlistId = request.playlistId;

    Utils::asyncExec([request = std::move(request)] {
        return evaluateMatches(request);
    }).then(this, [this, requestToken, search, playlistId](const std::vector<int>& matches) {
        if(requestToken != m_searchRequestToken || m_search->text().trimmed() != search) {
            return;
        }

        const auto* currentPlaylist = m_playlistController->currentPlaylist();
        if(!currentPlaylist || currentPlaylist->id() != playlistId) {
            return;
        }

        m_matches = matches;

        if(!m_matches.empty()) {
            selectMatch(0);
            return;
        }

        if(m_view->selectionModel()) {
            m_view->selectionModel()->clearSelection();
        }

        updateMatchLabel();
    });
}

void PlaylistSearchController::refreshMatchesIfOpen()
{
    if(isOpen()) {
        refreshMatches();
    }
}

void PlaylistSearchController::updateEnabledState()
{
    if(!m_settings->value<Settings::Gui::PlaylistIntegratedSearch>() && isOpen()) {
        close();
    }
}

void PlaylistSearchController::markResultsOutOfDate()
{
    if(!isOpen() || m_search->text().trimmed().isEmpty()) {
        return;
    }

    ++m_searchRequestToken;

    m_matches.clear();
    m_matchIndex       = -1;
    m_resultsOutOfDate = true;

    if(m_view->selectionModel()) {
        m_view->selectionModel()->clearSelection();
    }

    updateMatchLabel();
}

bool PlaylistSearchController::eventFilter(QObject* watched, QEvent* event)
{
    if(watched != m_search) {
        return QObject::eventFilter(watched, event);
    }

    if(event->type() == QEvent::ShortcutOverride) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_F3) {
            event->accept();
            return true;
        }
    }

    if(event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const int key  = keyEvent->key();

        if(key == Qt::Key_Escape) {
            close();
            return true;
        }
        if(key == Qt::Key_Down || (key == Qt::Key_F3 && !(keyEvent->modifiers() & Qt::ShiftModifier))) {
            navigate(1);
            return true;
        }
        if(key == Qt::Key_Up || (key == Qt::Key_F3 && (keyEvent->modifiers() & Qt::ShiftModifier))) {
            navigate(-1);
            return true;
        }
        if(key == Qt::Key_Enter || key == Qt::Key_Return) {
            if(hasCurrentMatch()) {
                if(keyEvent->modifiers() & Qt::ControlModifier) {
                    Q_EMIT queueCurrentRequested();
                }
                else {
                    Q_EMIT playCurrentRequested();
                }
            }
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

bool PlaylistSearchController::hasCurrentMatch() const
{
    return !m_resultsOutOfDate && m_matchIndex >= 0 && std::cmp_less(m_matchIndex, m_matches.size());
}

void PlaylistSearchController::selectMatch(int matchIndex)
{
    if(m_matches.empty()) {
        m_matchIndex = -1;
        updateMatchLabel();
        return;
    }

    const auto matchCount = static_cast<int>(m_matches.size());
    matchIndex            = ((matchIndex % matchCount) + matchCount) % matchCount;

    const int playlistIndex = m_matches.at(matchIndex);
    QModelIndex modelIndex  = m_model->indexAtPlaylistIndex(playlistIndex, true);
    while(modelIndex.isValid() && m_header->isSectionHidden(modelIndex.column())) {
        modelIndex = modelIndex.siblingAtColumn(modelIndex.column() + 1);
    }

    if(!modelIndex.isValid()) {
        m_matches.erase(m_matches.begin() + matchIndex);
        selectMatch(std::min(matchIndex, static_cast<int>(m_matches.size()) - 1));
        return;
    }

    m_matchIndex = matchIndex;
    m_view->setCurrentIndex(modelIndex);

    if(m_view->selectionModel()) {
        m_view->selectionModel()->select(modelIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_view->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);

    updateMatchLabel();
}

void PlaylistSearchController::updateMatchLabel() const
{
    const auto matchCount = static_cast<int>(m_matches.size());
    const bool hasSearch  = !m_search->text().trimmed().isEmpty();
    const bool hasMatches = matchCount > 0 && m_matchIndex >= 0;
    const bool canRefresh = hasSearch && m_resultsOutOfDate;

    m_previous->setEnabled(hasMatches || canRefresh);
    m_next->setEnabled(hasMatches || canRefresh);

    if(!hasSearch) {
        m_matchesLabel->clear();
    }
    else if(m_resultsOutOfDate) {
        m_matchesLabel->setText(tr("Results are out of date"));
    }
    else if(hasMatches) {
        //: Search result matches e.g. "6 of 11 matches"
        m_matchesLabel->setText(tr("%1 of %2 matches").arg(m_matchIndex + 1).arg(matchCount));
    }
    else {
        m_matchesLabel->setText(tr("0 matches"));
    }
}

} // namespace Fooyin

#include "moc_playlistsearchcontroller.cpp"
