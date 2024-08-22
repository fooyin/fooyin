/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "searchdialog.h"

#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"
#include "playlist/playlistview.h"
#include "playlist/playlistwidget.h"

#include <core/application.h>
#include <core/library/trackfilter.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>
#include <utils/utils.h>

#include <QAction>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

using namespace std::chrono_literals;

constexpr auto AutoSelect  = "Search/AutoSelect";
constexpr auto WindowState = "Search/WindowState";
constexpr auto SearchState = "Search/PlaylistState";

namespace Fooyin {
SearchDialog::SearchDialog(ActionManager* actionManager, PlaylistInteractor* playlistInteractor,
                           CoverProvider* coverProvider, Application* core, QWidget* parent)
    : QDialog{parent}
    , m_playlistInteractor{playlistInteractor}
    , m_settings{core->settingsManager()}
    , m_searchBar{new QLineEdit(this)}
    , m_view{new PlaylistWidget(actionManager, playlistInteractor, coverProvider, core, this)}
    , m_autoSelect{m_settings->fileValue(AutoSelect, false).toBool()}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});

    layout->addWidget(m_searchBar);
    layout->addWidget(m_view);

    auto* searchMenu = new QAction(Utils::iconFromTheme(Constants::Icons::Options), tr("Options"), this);
    QObject::connect(searchMenu, &QAction::triggered, this, &SearchDialog::showOptionsMenu);
    m_searchBar->addAction(searchMenu, QLineEdit::TrailingPosition);

    QObject::connect(m_view->view()->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &SearchDialog::selectInPlaylist);
    QObject::connect(m_view->model(), &PlaylistModel::modelReset, this, &SearchDialog::updateTitle);
    QObject::connect(m_view->model(), &PlaylistModel::modelReset, this, [this]() {
        if(m_autoSelect) {
            m_view->view()->selectAll();
        }
    });
    QObject::connect(m_playlistInteractor->playlistController(), &PlaylistController::currentPlaylistChanged, this,
                     &SearchDialog::search);
    QObject::connect(m_playlistInteractor->playlistController(), &PlaylistController::currentPlaylistTracksChanged,
                     this, &SearchDialog::search);

    auto* throttler = new SignalThrottler(this);
    throttler->setTimeout(100ms);
    QObject::connect(m_searchBar, &QLineEdit::textChanged, throttler, &SignalThrottler::throttle);
    QObject::connect(throttler, &SignalThrottler::triggered, this, &SearchDialog::search);

    m_view->setDetached(true);

    updateTitle();
    loadState();
}

SearchDialog::~SearchDialog()
{
    saveState();
}

QSize SearchDialog::sizeHint() const
{
    return {800, 480};
}

void SearchDialog::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        m_view->startPlayback();
    }

    QDialog::keyPressEvent(event);
}

void SearchDialog::search()
{
    m_view->searchEvent(m_searchBar->text());
}

void SearchDialog::updateTitle()
{
    QString title = tr("Search Playlist");

    if(!m_searchBar->text().isEmpty()) {
        const int trackCount = m_view->model()->rowCount({});
        title += u" (" + tr("%1 results").arg(trackCount) + u")";
    }

    setWindowTitle(title);
}

void SearchDialog::showOptionsMenu()
{
    auto* menu = new QMenu(tr("Options"), this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* autoSelect = new QAction(tr("Auto-select on search"), this);
    QObject::connect(autoSelect, &QAction::triggered, this, [this](const bool checked) {
        m_autoSelect = checked;
        m_settings->fileSet(AutoSelect, checked);
    });
    autoSelect->setCheckable(true);
    autoSelect->setChecked(m_autoSelect);
    menu->addAction(autoSelect);

    QStyleOptionFrame opt;
    opt.initFrom(m_searchBar);

    const QRect rect = m_searchBar->style()->subElementRect(QStyle::SE_LineEditContents, &opt, m_searchBar);
    const QPoint pos{rect.right() - 5, rect.center().y()};

    menu->popup(m_searchBar->mapToGlobal(pos));
}

void SearchDialog::selectInPlaylist(const QItemSelection& selected, const QItemSelection& /*deselected*/)
{
    std::vector<int> trackIds;

    for(const QModelIndex& index : selected.indexes()) {
        trackIds.emplace_back(index.data(PlaylistItem::Role::TrackId).toInt());
    }

    m_playlistInteractor->playlistController()->selectTrackIds(trackIds);
}

void SearchDialog::saveState()
{
    m_settings->fileSet(WindowState, saveGeometry());

    QJsonObject layout;
    m_view->saveLayoutData(layout);

    const QByteArray state = QJsonDocument{layout}.toJson(QJsonDocument::Compact).toBase64();
    m_settings->fileSet(SearchState, state);
}

void SearchDialog::loadState()
{
    restoreGeometry(m_settings->fileValue(WindowState).toByteArray());

    const QByteArray state = m_settings->fileValue(SearchState).toByteArray();
    if(!state.isEmpty()) {
        const QJsonObject layout = QJsonDocument::fromJson(QByteArray::fromBase64(state)).object();
        if(!layout.isEmpty()) {
            m_view->loadLayoutData(layout);
        }
    }

    m_view->finalise();
}
} // namespace Fooyin

#include "moc_searchdialog.cpp"
