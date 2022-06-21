/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistwidget.h"

#include "core/library/musiclibrary.h"
#include "core/player/playermanager.h"
#include "playlistdelegate.h"
#include "utils/settings.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>

namespace Library {
PlaylistWidget::PlaylistWidget(PlayerManager* playerManager, MusicLibrary* library, QWidget* parent)
    : Widget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_library(library)
    , m_playerManager(playerManager)
    , m_model(playerManager, library, this)
    , m_settings(Settings::instance())
    , m_altRowColours(m_settings->value(Settings::Setting::PlaylistAltColours).toBool())
{
    setObjectName("Playlist");
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(&m_playlist);

    m_playlist.setModel(&m_model);
    m_playlist.setItemDelegate(new PlaylistDelegate(this));
    setupConnections();
    reset();

    setHeaderHidden(!m_settings->value(Settings::Setting::PlaylistHeader).toBool());
    setScrollbarHidden(!m_settings->value(Settings::Setting::PlaylistScrollBar).toBool());
}

PlaylistWidget::~PlaylistWidget() = default;

void PlaylistWidget::reset()
{
    m_playlist.expandAll();
    m_playlist.scrollToTop();
}

void PlaylistWidget::setupConnections()
{
    connect(m_settings, &Settings::playlistAltColorsChanged, this, [=] {
        m_altRowColours = !m_altRowColours;
    });
    connect(m_settings, &Settings::playlistHeaderChanged, this, [=] {
        setHeaderHidden(!isHeaderHidden());
    });
    connect(m_settings, &Settings::playlistScrollBarChanged, this, [=] {
        setScrollbarHidden(!isScrollbarHidden());
    });
    connect(m_playlist.header(), &QHeaderView::sectionClicked, this, &PlaylistWidget::switchOrder);
    connect(m_playlist.header(), &QHeaderView::customContextMenuRequested, this,
            &PlaylistWidget::customHeaderMenuRequested);
    connect(&m_model, &PlaylistModel::modelReset, this, &PlaylistWidget::reset);
    connect(&m_playlist, &PlaylistView::doubleClicked, this, &PlaylistWidget::playTrack);
    connect(this, &PlaylistWidget::clickedTrack, m_playerManager, &PlayerManager::reset);
    connect(this, &PlaylistWidget::clickedTrack, m_library, &Library::MusicLibrary::prepareTracks);
    connect(m_playerManager, &PlayerManager::playStateChanged, this, &PlaylistWidget::changeState);
    connect(m_playerManager, &PlayerManager::nextTrack, this, &PlaylistWidget::nextTrack);
    connect(m_playlist.selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &PlaylistWidget::selectionChanged);
}

bool PlaylistWidget::isHeaderHidden()
{
    return m_playlist.isHeaderHidden();
}

void PlaylistWidget::setHeaderHidden(bool b)
{
    m_playlist.setHeaderHidden(b);
}

bool PlaylistWidget::isScrollbarHidden()
{
    return m_playlist.verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff;
}

void PlaylistWidget::setScrollbarHidden(bool b)
{
    m_playlist.setVerticalScrollBarPolicy(b ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
}

void PlaylistWidget::layoutEditingMenu(QMenu* menu)
{
    auto* showHeaders = new QAction("Show Header", menu);
    showHeaders->setCheckable(true);
    showHeaders->setChecked(!isHeaderHidden());
    QAction::connect(showHeaders, &QAction::triggered, this, [=] {
        m_settings->set(Settings::Setting::PlaylistHeader, isHeaderHidden());
    });
    menu->addAction(showHeaders);

    auto* showScrollBar = new QAction("Show Scrollbar", menu);
    showScrollBar->setCheckable(true);
    showScrollBar->setChecked(!isScrollbarHidden());
    QAction::connect(showScrollBar, &QAction::triggered, this, [=] {
        m_settings->set(Settings::Setting::PlaylistScrollBar, isScrollbarHidden());
    });
    menu->addAction(showScrollBar);

    auto* altRowColours = new QAction("Alternative Row Colours", menu);
    altRowColours->setCheckable(true);
    altRowColours->setChecked(m_altRowColours);
    QAction::connect(altRowColours, &QAction::triggered, this, [=] {
        m_settings->set(Settings::Setting::PlaylistAltColours, !m_altRowColours);
    });
    menu->addAction(altRowColours);
}

void PlaylistWidget::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    QModelIndexList indexes = m_playlist.selectionModel()->selectedIndexes();
    QSet<Track*> tracks;
    for(const auto& index : indexes) {
        if(index.isValid()) {
            const auto type = index.data(Role::Type).value<PlaylistItem::Type>();
            if(type == PlaylistItem::Type::Track) {
                auto* data = index.data(ItemRole::Data).value<Track*>();
                tracks.insert(data);
            }
        }
    }
    m_library->trackSelectionChanged(tracks);
}

void PlaylistWidget::keyPressEvent(QKeyEvent* e)
{
    const auto key = e->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        const QModelIndex index = m_playlist.selectionModel()->selectedIndexes().constFirst();
        const auto type = index.data(Role::Type).value<PlaylistItem::Type>();

        if(type != PlaylistItem::Type::Track) {
            return;
        }

        auto idx = index.data(ItemRole::Index).toInt();

        emit clickedTrack(idx, false);
        m_model.changeTrackState();
        m_playlist.clearSelection();
    }
    QWidget::keyPressEvent(e);
}

// void PlaylistWidget::spanHeaders(QModelIndex parent)
//{
//     for (int i = 0; i < m_model->rowCount(parent); i++)
//     {
//         auto idx = m_model->index(i, 0, parent);
//         if (m_model->hasChildren(idx))
//         {
//             m_playlist->setFirstColumnSpanned(i, parent, true);
//             spanHeaders(idx);
//         }
//     }
// }

void PlaylistWidget::customHeaderMenuRequested(QPoint pos)
{
    QMenu menu;

    const auto order = m_library->sortOrder();

    QActionGroup sortOrder{&menu};

    QAction yearSort{&menu};
    yearSort.setText("Year");
    yearSort.setData(QVariant::fromValue<Library::SortOrder>(Library::SortOrder::YearDesc));
    yearSort.setCheckable(true);
    yearSort.setChecked(order == Library::SortOrder::YearAsc || order == Library::SortOrder::YearDesc);
    menu.addAction(&yearSort);

    sortOrder.addAction(&yearSort);

    menu.addSeparator();

    menu.setDefaultAction(sortOrder.checkedAction());

    connect(&sortOrder, &QActionGroup::triggered, this, &PlaylistWidget::changeOrder);

    menu.exec(mapToGlobal(pos));
}

void PlaylistWidget::changeOrder(QAction* action)
{
    auto order = action->data().value<Library::SortOrder>();
    m_library->changeOrder(order);
}

void PlaylistWidget::switchOrder()
{
    const auto order = m_library->sortOrder();
    switch(order) {
        case(Library::SortOrder::TitleAsc):
            return m_library->changeOrder(Library::SortOrder::TitleDesc);
        case(Library::SortOrder::TitleDesc):
            return m_library->changeOrder(Library::SortOrder::TitleAsc);
        case(Library::SortOrder::YearAsc):
            return m_library->changeOrder(Library::SortOrder::YearDesc);
        case(Library::SortOrder::YearDesc):
            return m_library->changeOrder(Library::SortOrder::YearAsc);
        case(Library::SortOrder::NoSorting):
            return m_library->changeOrder(Library::SortOrder::TitleAsc);
    }
}

void PlaylistWidget::changeState(Player::PlayState state)
{
    switch(state) {
        case(Player::PlayState::Playing):
            m_model.changeTrackState();
            return findCurrent();
        case(Player::PlayState::Stopped):
        case(Player::PlayState::Paused):
            return m_model.changeTrackState();
    }
}

void PlaylistWidget::playTrack(const QModelIndex& index)
{
    const auto type = index.data(Role::Type).value<PlaylistItem::Type>();
    if(type != PlaylistItem::Type::Track) {
        return;
    }

    auto idx = index.data(ItemRole::Index).toInt();

    emit clickedTrack(idx, false);
    m_model.changeTrackState();
    m_playlist.clearSelection();
}

void PlaylistWidget::nextTrack()
{
    m_model.changeTrackState();
}

void PlaylistWidget::findCurrent()
{
    const auto* track = m_playerManager->currentTrack();

    if(!track) {
        return;
    }

    QModelIndex index;
    //    index = m_>model->match({}, ItemRole::Id, track->id(), 1, {});
    index = m_model.indexOfItem(track->id());

    if(index.isValid()) {
        m_playlist.scrollTo(index);
        //        setCurrentIndex(index.constFirst());
    }
}
} // namespace Library
