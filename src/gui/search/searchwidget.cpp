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

#include "playlist/playlistcontroller.h"
#include "playlist/playlistwidget.h"
#include "searchcontroller.h"

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

#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QStyleOptionFrame>

namespace Fooyin {
SearchWidget::SearchWidget(SearchController* controller, PlaylistController* playlistController, MusicLibrary* library,
                           SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_searchController{controller}
    , m_playlistController{playlistController}
    , m_library{library}
    , m_settings{settings}
    , m_searchBox{new QLineEdit(this)}
    , m_defaultPlaceholder{tr("Search library…")}
    , m_mode{SearchMode::Library}
    , m_unconnected{true}
    , m_autoSearch{false}
{
    setObjectName(SearchWidget::name());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(m_searchBox);

    m_searchBox->setPlaceholderText(m_defaultPlaceholder);
    m_searchBox->setClearButtonEnabled(true);

    QObject::connect(m_searchBox, &QLineEdit::textChanged, this, [this]() {
        if(m_autoSearch) {
            searchChanged();
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
}

SearchWidget::~SearchWidget()
{
    m_searchController->removeConnectedWidgets(id());
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

void SearchWidget::showEvent(QShowEvent* event)
{
    updateConnectedState();
    FyWidget::showEvent(event);
}

void SearchWidget::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        searchChanged();
    }

    FyWidget::keyPressEvent(event);
}

void SearchWidget::updateConnectedState()
{
    const auto widgets = m_searchController->connectedWidgets(id());
    m_unconnected      = widgets.empty() || (widgets.size() == 1 && qobject_cast<PlaylistWidget*>(widgets.front()));
}

void SearchWidget::searchChanged()
{
    if(!m_unconnected) {
        m_searchController->changeSearch(id(), m_searchBox->text());
        return;
    }

    const TrackList tracks = m_mode == SearchMode::PlaylistInline && m_playlistController->currentPlaylist()
                               ? m_playlistController->currentPlaylist()->tracks()
                               : m_library->tracks();

    Utils::asyncExec([search = m_searchBox->text(), tracks]() {
        ScriptParser parser;
        return parser.filter(search, tracks);
    }).then(this, [this](const TrackList& filteredTracks) {
        if(m_mode == SearchMode::PlaylistInline) {
            m_playlistController->selectTrackIds(Track::trackIdsForTracks(filteredTracks));
        }
        else if(auto* playlist = m_playlistController->playlistHandler()->createPlaylist(
                    QStringLiteral("Search Results"), filteredTracks)) {
            m_playlistController->changeCurrentPlaylist(playlist);
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

        auto* searchPlaylistInline = new QAction(tr("Playlist (Inline)"), this);
        searchPlaylistInline->setCheckable(true);
        searchPlaylistInline->setChecked(m_mode == SearchMode::PlaylistInline);
        QObject::connect(searchPlaylistInline, &QAction::triggered, this,
                         [this]() { m_mode = SearchMode::PlaylistInline; });
        searchInMenu->addAction(searchPlaylistInline);
    }

    menu->addSeparator();

    auto* changePlaceholder = new QAction(tr("Change placeholder text"), this);
    QObject::connect(changePlaceholder, &QAction::triggered, this, &SearchWidget::changePlaceholderText);
    menu->addAction(changePlaceholder);

    auto* manageConnections = new QAction(tr("Manage connections"), this);
    QObject::connect(manageConnections, &QAction::triggered, this,
                     [this]() { m_searchController->setupWidgetConnections(id()); });
    menu->addAction(manageConnections);

    QStyleOptionFrame opt;
    opt.initFrom(m_searchBox);

    const QRect rect = m_searchBox->style()->subElementRect(QStyle::SE_LineEditContents, &opt, m_searchBox);
    const QPoint pos{rect.right() - 5, rect.center().y()};

    menu->popup(m_searchBox->mapToGlobal(pos));
}
} // namespace Fooyin

#include "moc_searchwidget.cpp"
