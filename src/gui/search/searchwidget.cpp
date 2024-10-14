/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "searchwidget.h"

#include "internalguisettings.h"
#include "playlist/playlistcontroller.h"
#include "playlist/playlistwidget.h"
#include "searchcontroller.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/playlist/playlisthandler.h>
#include <core/scripting/scriptparser.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/widgets/popuplineedit.h>
#include <utils/actions/actioncontainer.h>
#include <utils/async.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QStyleOptionFrame>

constexpr auto QuickSearchState = "Searching/QuickSearchState";

namespace Fooyin {
SearchWidget::SearchWidget(SearchController* controller, PlaylistController* playlistController, MusicLibrary* library,
                           SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_searchController{controller}
    , m_playlistController{playlistController}
    , m_playlistHandler{m_playlistController->playlistHandler()}
    , m_library{library}
    , m_settings{settings}
    , m_searchBox{new QLineEdit(this)}
    , m_defaultPlaceholder{tr("Search library…")}
    , m_mode{SearchMode::Library}
    , m_forceNewPlaylist{false}
    , m_unconnected{true}
    , m_autoSearch{!isQuickSearch()}
{
    setObjectName(SearchWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_searchBox);

    m_searchBox->setPlaceholderText(m_defaultPlaceholder);
    m_searchBox->setClearButtonEnabled(true);

    loadColours();

    QObject::connect(m_searchBox, &QLineEdit::textChanged, this, [this]() {
        if(m_autoSearch) {
            m_searchTimer.start((m_settings->value<Settings::Gui::SearchAutoDelay>() + 1) * 30, this);
        }
    });
    QObject::connect(m_searchController, &SearchController::connectionChanged, this, [this](const Id& widgetId) {
        if(widgetId == id()) {
            updateConnectedState();
        }
    });

    auto* selectReceiver = new QAction(Utils::iconFromTheme(Constants::Icons::Options), tr("Options"), this);
    QObject::connect(selectReceiver, &QAction::triggered, this, [this]() { showOptionsMenu(); });
    m_searchBox->addAction(selectReceiver, QLineEdit::TrailingPosition);

    m_settings->subscribe<Settings::Gui::IconTheme>(
        this, [selectReceiver]() { selectReceiver->setIcon(Utils::iconFromTheme(Constants::Icons::Options)); });
    m_settings->subscribe<Settings::Gui::SearchErrorBg>(this, &SearchWidget::loadColours);
    m_settings->subscribe<Settings::Gui::SearchErrorFg>(this, &SearchWidget::loadColours);
}

SearchWidget::~SearchWidget()
{
    m_searchController->removeConnectedWidgets(id());
}

QString SearchWidget::defaultPlaylistName()
{
    return tr("Search Results");
}

QString SearchWidget::name() const
{
    return tr("Search Bar");
}

QString SearchWidget::layoutName() const
{
    return QStringLiteral("SearchBar");
}

void SearchWidget::layoutEditingMenu(QMenu* menu)
{
    auto* manageConnections = new QAction(tr("Manage connections"), this);
    QObject::connect(manageConnections, &QAction::triggered, this,
                     [this]() { m_searchController->setupWidgetConnections(id()); });
    menu->addAction(manageConnections);
}

void SearchWidget::saveLayoutData(QJsonObject& layout)
{
    layout[u"AutoSearch"] = m_autoSearch;
    layout[u"SearchMode"] = static_cast<quint8>(m_mode);

    const QString placeholderText = m_searchBox->placeholderText();
    if(!placeholderText.isEmpty() && placeholderText != m_defaultPlaceholder) {
        layout[u"Placeholder"] = placeholderText;
    }

    const auto connectedWidgets = m_searchController->connectedWidgetIds(id());

    if(connectedWidgets.empty()) {
        return;
    }

    QStringList widgetIds;
    std::ranges::transform(connectedWidgets, std::back_inserter(widgetIds), [](const Id& id) { return id.name(); });

    layout[u"Widgets"] = widgetIds.join(QStringLiteral("|"));
}

void SearchWidget::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains(u"AutoSearch")) {
        m_autoSearch = layout.value(u"AutoSearch").toBool();
    }

    if(layout.contains(u"SearchMode")) {
        m_mode = static_cast<SearchMode>(layout.value(u"SearchMode").toVariant().value<quint8>());
    }

    if(layout.contains(u"Placeholder")) {
        m_searchBox->setPlaceholderText(layout.value(u"Placeholder").toString());
    }

    if(!layout.contains(u"Widgets")) {
        return;
    }

    const QStringList widgetIds = layout.value(u"Widgets").toString().split(u'|');

    if(widgetIds.isEmpty()) {
        return;
    }

    IdSet connectedWidgets;

    for(const QString& id : widgetIds) {
        connectedWidgets.emplace(id.trimmed());
    }

    m_searchController->setConnectedWidgets(id(), connectedWidgets);
}

QSize SearchWidget::sizeHint() const
{
    QSize size = m_searchBox->sizeHint();
    size.rwidth() += 400;
    return size;
}

QSize SearchWidget::minimumSizeHint() const
{
    return m_searchBox->minimumSizeHint();
}

void SearchWidget::showEvent(QShowEvent* event)
{
    updateConnectedState();

    if(isQuickSearch()) {
        const FyStateSettings stateSettings;
        const QJsonObject layoutData = stateSettings.value(QLatin1String{QuickSearchState}).toJsonObject();
        loadLayoutData(layoutData);
    }

    FyWidget::showEvent(event);
}

void SearchWidget::closeEvent(QCloseEvent* event)
{
    if(isQuickSearch()) {
        QJsonObject layoutData;
        saveLayoutData(layoutData);

        FyStateSettings stateSettings;
        stateSettings.setValue(QLatin1String{QuickSearchState}, layoutData);
    }
    FyWidget::closeEvent(event);
}

void SearchWidget::keyPressEvent(QKeyEvent* event)
{
    const int key        = event->key();
    const auto modifiers = event->modifiers();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        if(modifiers == Qt::ControlModifier) {
            m_forceNewPlaylist = true;
        }
        else if(modifiers == Qt::AltModifier) {
            m_forceMode = SearchMode::Playlist;
        }
        else if(modifiers == (Qt::AltModifier | Qt::ShiftModifier)) {
            m_forceMode = SearchMode::AllPlaylists;
        }
        else if(modifiers == Qt::ShiftModifier) {
            m_forceMode = SearchMode::PlaylistInline;
        }
        else if(modifiers == (Qt::ControlModifier | Qt::AltModifier)) {
            m_forceMode        = SearchMode::Playlist;
            m_forceNewPlaylist = true;
        }
        else if(modifiers == (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) {
            m_forceMode        = SearchMode::AllPlaylists;
            m_forceNewPlaylist = true;
        }

        searchChanged(true);
    }
    else if(key == Qt::Key_Escape && isQuickSearch()) {
        close();
    }
    else if(key == Qt::Key_Backspace && modifiers == Qt::ControlModifier) {
        deleteWord();
    }

    FyWidget::keyPressEvent(event);
}

void SearchWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_searchTimer.timerId()) {
        m_searchTimer.stop();
        searchChanged();
    }
    FyWidget::timerEvent(event);
}

bool SearchWidget::isQuickSearch() const
{
    return parentWidget() == nullptr;
}

Playlist* SearchWidget::findOrAddPlaylist(const TrackList& tracks)
{
    QString searchResultsName = m_settings->value<Settings::Gui::SearchPlaylistName>();

    if(!m_forceNewPlaylist) {
        const auto playlists = m_playlistHandler->playlists();
        for(auto* playlist : playlists) {
            if(playlist->name().startsWith(searchResultsName)) {
                if(m_settings->value<Settings::Gui::SearchPlaylistAppendSearch>()) {
                    searchResultsName.append(QStringLiteral(" [%1]").arg(m_searchBox->text()));
                }
                if(searchResultsName != playlist->name()) {
                    m_playlistHandler->renamePlaylist(playlist->id(), searchResultsName);
                }
                m_playlistHandler->replacePlaylistTracks(playlist->id(), tracks);
                return playlist;
            }
        }
    }

    const bool forceNew = std::exchange(m_forceNewPlaylist, false);

    if(auto* playlist = forceNew ? m_playlistHandler->createNewPlaylist(searchResultsName, tracks)
                                 : m_playlistHandler->createPlaylist(searchResultsName, tracks)) {
        if(m_settings->value<Settings::Gui::SearchPlaylistAppendSearch>()) {
            searchResultsName.append(QStringLiteral(" (%1)").arg(m_searchBox->text()));
            m_playlistHandler->renamePlaylist(playlist->id(), searchResultsName);
        }
        m_playlistController->changeCurrentPlaylist(playlist);
        return playlist;
    }

    return nullptr;
}

TrackList SearchWidget::getTracksToSearch(SearchMode mode) const
{
    switch(mode) {
        case(SearchMode::AllPlaylists): {
            const QString searchResultsName = m_settings->value<Settings::Gui::SearchPlaylistName>();
            const auto playlists            = m_playlistHandler->playlists();
            TrackList allTracks;
            for(const Playlist* playlist : playlists) {
                if(!playlist->name().contains(searchResultsName)) {
                    const TrackList playlistTracks = playlist->tracks();
                    allTracks.insert(allTracks.end(), playlistTracks.cbegin(), playlistTracks.cend());
                }
            }
            return allTracks;
        }
        case(SearchMode::Library):
            return m_library->tracks();
        case(SearchMode::Playlist):
        case(SearchMode::PlaylistInline):
            if(m_playlistController->currentPlaylist()) {
                return m_playlistController->currentPlaylist()->tracks();
            }
            [[fallthrough]];
        default:
            return {};
    }
}

void SearchWidget::deleteWord()
{
    QString text             = m_searchBox->text();
    const int cursorPosition = m_searchBox->cursorPosition();

    static const QRegularExpression wordRegex{QStringLiteral("\\W")};

    int lastWordBoundary = static_cast<int>(text.lastIndexOf(wordRegex, cursorPosition - 1));
    if(lastWordBoundary == -1) {
        lastWordBoundary = 0;
    }

    m_searchBox->setText(text.remove(lastWordBoundary, cursorPosition - lastWordBoundary));
    m_searchBox->setCursorPosition(lastWordBoundary);
}

bool SearchWidget::handleFilteredTracks(SearchMode mode, const TrackList& tracks)
{
    if(tracks.empty()) {
        if(!m_searchBox->text().isEmpty()) {
            handleSearchFailed();
        }
        return false;
    }

    resetColours();

    if(mode == SearchMode::PlaylistInline) {
        m_playlistController->selectTrackIds(Track::trackIdsForTracks(tracks));
    }
    else {
        if(auto* playlist = findOrAddPlaylist(tracks)) {
            m_playlistController->changeCurrentPlaylist(playlist);
        }
    }

    if(!m_autoSearch && m_settings->value<Settings::Gui::SearchSuccessClear>()) {
        m_searchBox->clear();
    }
    if(!m_autoSearch && m_settings->value<Settings::Gui::SearchSuccessFocus>()) {
        m_playlistController->focusPlaylist();
    }

    return true;
}

void SearchWidget::handleSearchFailed()
{
    if(m_colours.failBg) {
        QPalette palette = m_searchBox->palette();
        palette.setColor(QPalette::Base, m_colours.failBg.value());
        m_searchBox->setPalette(palette);
    }
    if(m_colours.failFg) {
        QPalette palette = m_searchBox->palette();
        palette.setColor(QPalette::Text, m_colours.failFg.value());
        m_searchBox->setPalette(palette);
    }
}

void SearchWidget::loadColours()
{
    const auto failBg = m_settings->value<Settings::Gui::SearchErrorBg>();
    if(failBg.isNull()) {
        m_colours.failBg = {};
    }
    else {
        m_colours.failBg = failBg.value<QColor>();
    }

    const auto failFg = m_settings->value<Settings::Gui::SearchErrorFg>();
    if(failFg.isNull()) {
        m_colours.failFg = {};
    }
    else {
        m_colours.failFg = failFg.value<QColor>();
    }
}

void SearchWidget::resetColours()
{
    const QPalette defaultPalette = m_searchBox->style()->standardPalette();
    m_searchBox->setPalette(defaultPalette);
}

void SearchWidget::updateConnectedState()
{
    const auto widgets = m_searchController->connectedWidgets(id());
    m_unconnected      = widgets.empty() || (widgets.size() == 1 && qobject_cast<PlaylistWidget*>(widgets.front()));

    if(m_unconnected) {
        static const QString searchTooltip
            = QStringLiteral("<b>%1</b><br><br>"
                             "<b>Ctrl</b>: %2<br>"
                             "<b>Alt</b>: %3<br>"
                             "<b>Alt + Shift</b>: %4<br>"
                             "<b>Shift</b>: %5<br>"
                             "<b>Ctrl + Alt</b>: %6<br>"
                             "<b>Ctrl + Alt + Shift</b>: %7<br>"
                             "<b>Ctrl + Backspace</b>: %8<br>")
                  .arg(tr("Special Keys"), tr("Force the creation of a new results playlist"),
                       tr("Force search in the current playlist"), tr("Force search in all playlists"),
                       tr("Force inline playlist search"),
                       tr("Force new results playlist using the current playlist as the source"),
                       tr("Force new results playlist using all playlists"), tr("Delete a word in the search box"));
        m_searchBox->setToolTip(searchTooltip);
    }
    else {
        m_searchBox->setToolTip({});
    }
}

void SearchWidget::searchChanged(bool enterKey)
{
    if(!m_unconnected) {
        m_searchController->changeSearch(id(), m_searchBox->text());
        return;
    }

    const auto mode = m_forceMode ? std::exchange(m_forceMode, {}).value() : m_mode; // NOLINT

    Utils::asyncExec([search = m_searchBox->text(), tracks = getTracksToSearch(mode)]() {
        ScriptParser parser;
        return parser.filter(search, tracks);
    }).then(this, [this, mode, enterKey](const TrackList& filteredTracks) {
        if(handleFilteredTracks(mode, filteredTracks) && enterKey) {
            if(isQuickSearch() && m_settings->value<Settings::Gui::SearchSuccessClose>()) {
                close();
            }
            else if(m_settings->value<Settings::Gui::SearchSuccessClear>()) {
                m_searchBox->clear();
            }
        }
    });
}

void SearchWidget::changePlaceholderText()
{
    auto* lineEdit = new PopupLineEdit(m_searchBox->placeholderText(), this);
    lineEdit->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(lineEdit, &PopupLineEdit::editingCancelled, lineEdit, &QWidget::close);
    QObject::connect(lineEdit, &PopupLineEdit::editingFinished, this, [this, lineEdit]() {
        const QString text = lineEdit->text();
        if(text != m_searchBox->placeholderText()) {
            m_searchBox->setPlaceholderText(!text.isEmpty() ? text : m_defaultPlaceholder);
        }
        lineEdit->close();
    });

    lineEdit->setGeometry(m_searchBox->geometry());
    lineEdit->show();
    lineEdit->selectAll();
    lineEdit->setFocus(Qt::ActiveWindowFocusReason);
}

void SearchWidget::showOptionsMenu()
{
    auto* menu = new QMenu(tr("Options"), this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* autoSearch = new QAction(tr("Autosearch"), this);
    autoSearch->setCheckable(true);
    autoSearch->setChecked(m_autoSearch);
    QObject::connect(autoSearch, &QAction::triggered, this, [this](const bool checked) { m_autoSearch = checked; });
    menu->addAction(autoSearch);

    if(m_unconnected) {
        auto* searchInMenu = menu->addMenu(tr("Search in"));

        auto* searchLibrary = new QAction(tr("Library"), this);
        searchLibrary->setCheckable(true);
        searchLibrary->setChecked(m_mode == SearchMode::Library);
        QObject::connect(searchLibrary, &QAction::triggered, this, [this]() { m_mode = SearchMode::Library; });
        searchInMenu->addAction(searchLibrary);

        auto* searchPlaylist = new QAction(tr("Playlist"), this);
        searchPlaylist->setCheckable(true);
        searchPlaylist->setChecked(m_mode == SearchMode::Playlist);
        QObject::connect(searchPlaylist, &QAction::triggered, this, [this]() { m_mode = SearchMode::Playlist; });
        searchInMenu->addAction(searchPlaylist);

        auto* searchPlaylistInline = new QAction(tr("Playlist (Inline)"), this);
        searchPlaylistInline->setCheckable(true);
        searchPlaylistInline->setChecked(m_mode == SearchMode::PlaylistInline);
        QObject::connect(searchPlaylistInline, &QAction::triggered, this,
                         [this]() { m_mode = SearchMode::PlaylistInline; });
        searchInMenu->addAction(searchPlaylistInline);

        auto* searchAllPlaylist = new QAction(tr("All Playlists"), this);
        searchAllPlaylist->setCheckable(true);
        searchAllPlaylist->setChecked(m_mode == SearchMode::AllPlaylists);
        QObject::connect(searchAllPlaylist, &QAction::triggered, this, [this]() { m_mode = SearchMode::AllPlaylists; });
        searchInMenu->addAction(searchAllPlaylist);
    }

    menu->addSeparator();

    if(!isQuickSearch()) {
        // Quick search widget can't be connected to other widgets
        auto* changePlaceholder = new QAction(tr("Change placeholder text"), this);
        QObject::connect(changePlaceholder, &QAction::triggered, this, &SearchWidget::changePlaceholderText);
        menu->addAction(changePlaceholder);

        auto* manageConnections = new QAction(tr("Manage connections"), this);
        QObject::connect(manageConnections, &QAction::triggered, this,
                         [this]() { m_searchController->setupWidgetConnections(id()); });
        menu->addAction(manageConnections);
    }

    auto* searching = new QAction(tr("Help"), this);
    QObject::connect(searching, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QStringLiteral("https://docs.fooyin.org/en/latest/searching/basics.html"));
    });
    menu->addAction(searching);

    QStyleOptionFrame opt;
    opt.initFrom(m_searchBox);

    const QRect rect = m_searchBox->style()->subElementRect(QStyle::SE_LineEditContents, &opt, m_searchBox);
    const QPoint pos{rect.right() - 5, rect.center().y()};

    menu->popup(m_searchBox->mapToGlobal(pos));
}
} // namespace Fooyin

#include "moc_searchwidget.cpp"
